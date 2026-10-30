#include <QtTest>

#include <cmath>
#include <numbers>
#include <set>
#include <string_view>
#include <vector>

#include "core/Transport.h"
#include "klang/dsp/Engine.h"

using namespace klang;
namespace p = klang::positions;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;

Sample sine(double hz, double seconds, double rate = 44100.0)
{
    std::vector<float> x(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = float(0.5 * std::sin(2.0 * std::numbers::pi * hz * double(i) / rate));
    return Sample::fromChannels(std::move(x), {}, rate, "sine");
}

// Rises from -0.5 to +0.5 over its length.
Sample ramp(double seconds, double rate = 48000.0)
{
    std::vector<float> x(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = float(double(i) / double(x.size()) - 0.5);
    return Sample::fromChannels(std::move(x), {}, rate, "ramp");
}

struct Rig
{
    Engine engine;
    krate::Transport transport;
    std::vector<float> left, right;

    Rig()
    {
        engine.prepare(kRate, kBlock);
        engine.setParam(ParamId::Volume, 1.0f);
        transport.setBpm(120.0);
    }
    void set(int pad, ParamId id, float v) { engine.setPadParam(pad, id, v); }
    // Runs `seconds`, with MIDI events at their times (status, data1, data2).
    void run(double seconds, std::vector<std::tuple<double, int, int, int>> events = {})
    {
        std::vector<float> l(kBlock), r(kBlock);
        const auto frames = std::uint64_t(seconds * kRate);
        const std::uint64_t base = left.size();
        std::size_t next = 0;
        for (std::uint64_t f = 0; f < frames; f += kBlock) {
            std::vector<krate::MidiEvent> midi;
            while (next < events.size() && std::uint64_t(std::get<0>(events[next]) * kRate) < f + kBlock) {
                const auto &[t, status, d1, d2] = events[next++];
                midi.push_back({std::uint32_t(std::uint64_t(t * kRate) - f), 3,
                                {std::uint8_t(status), std::uint8_t(d1), std::uint8_t(d2)}});
            }
            const krate::TransportState state = transport.advance(kBlock, kRate);
            krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()), &state);
            left.insert(left.end(), l.begin(), l.end());
            right.insert(right.end(), r.begin(), r.end());
        }
        (void)base;
    }
};

double frequency(const std::vector<float> &x, double from, double to)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (auto i = std::size_t(from * kRate); i < std::size_t(to * kRate); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
            if (first < 0.0)
                first = t;
            else
                ++count;
            last = t;
        }
    return count > 0 ? count * kRate / (last - first) : 0.0;
}

double rms(const std::vector<float> &x, double from, double to)
{
    double sum = 0.0;
    const auto a = std::size_t(from * kRate), b = std::min(x.size(), std::size_t(to * kRate));
    for (std::size_t i = a; i < b; ++i)
        sum += double(x[i]) * x[i];
    return b > a ? std::sqrt(sum / double(b - a)) : 0.0;
}

// The largest step from one sample to the next between `from` and `to`.
double largestJump(const std::vector<float> &x, double from, double to)
{
    double jump = 0.0;
    for (auto i = std::size_t(from * kRate) + 1; i < std::min(x.size(), std::size_t(to * kRate)); ++i)
        jump = std::max(jump, double(std::fabs(x[i] - x[i - 1])));
    return jump;
}

constexpr int kNoteOn = 0x90, kNoteOff = 0x80;

} // namespace

class TestPads : public QObject
{
    Q_OBJECT

private slots:
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QCOMPARE(ParamCount, 36);
        QCOMPARE(PadParamCount, 24);
        std::set<std::string_view> keys;
        for (const auto &info : kParams) {
            QVERIFY(keys.insert(info.key).second);
            for (float v : {0.0f, 0.5f, 1.0f})
                QVERIFY(!info.format(v).empty());
            QCOMPARE(krate::snapToDetent(info, info.defaultValue), info.defaultValue);
        }
        const auto text = [](ParamId id, float v) { return paramDescriptor(id).format(v); };
        QCOMPARE(text(ParamId::Tune, paramDescriptor(ParamId::Tune).defaultValue), std::string("+0 st"));
        QCOMPARE(text(ParamId::Decay, 1.0f), std::string("Full"));
        QCOMPARE(text(ParamId::Sync, 1.0f), std::string("16 beats"));
        QCOMPARE(text(ParamId::Bits, 0.0f), std::string("Off"));
        QCOMPARE(text(ParamId::Bits, 1.0f), std::string("2 bits"));
        QCOMPARE(text(ParamId::Downsample, 1.0f), std::string("1/32"));
        QCOMPARE(text(ParamId::Choke, 0.25f), std::string("Group 1"));
        QCOMPARE(text(ParamId::Mode, 1.0f), std::string("Loop"));
    }

    // The edited pad's values are the table's; each pad keeps its own.
    void editedPad()
    {
        Engine e;
        e.setEditedPad(3);
        e.setParam(ParamId::Cutoff, 0.2f);
        e.setParam(ParamId::Swing, 0.4f);
        QCOMPARE(e.padParam(3, ParamId::Cutoff), 0.2f);
        QCOMPARE(e.padParam(0, ParamId::Cutoff), paramDescriptor(ParamId::Cutoff).defaultValue);
        e.takeParamsChangedByAudio();
        e.setEditedPad(0);
        QCOMPARE(e.param(ParamId::Cutoff), paramDescriptor(ParamId::Cutoff).defaultValue);
        QCOMPARE(e.param(ParamId::Swing), 0.4f); // Klang's own: the same whatever pad
        QCOMPARE(e.takeParamsChangedByAudio() & 1u, 1u); // the panel follows the new pad
    }

    // A sample recorded at 44.1 kHz plays at its pitch at 48 kHz; Tune shifts it;
    // notes 36.. are the pads; Chromatic keys play the edited pad from C4.
    void pitch_data()
    {
        QTest::addColumn<int>("tune");
        QTest::addColumn<bool>("chromatic");
        QTest::addColumn<int>("note");
        QTest::addColumn<double>("hz");
        QTest::newRow("as recorded") << 0 << false << 36 << 440.0;
        QTest::newRow("+12") << 12 << false << 36 << 880.0;
        QTest::newRow("-7") << -7 << false << 36 << 440.0 * std::exp2(-7.0 / 12.0);
        QTest::newRow("chromatic C4") << 0 << true << 60 << 440.0;
        QTest::newRow("chromatic G4") << 0 << true << 67 << 440.0 * std::exp2(7.0 / 12.0);
    }
    void pitch()
    {
        QFETCH(int, tune);
        QFETCH(bool, chromatic);
        QFETCH(int, note);
        QFETCH(double, hz);
        const Sample s = sine(440.0, 1.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Tune, position(tune + 24, p::kTune));
        rig.engine.setParam(ParamId::KeyMode, chromatic ? 1.0f : 0.0f);
        rig.run(0.5, {{0.0, kNoteOn, note, 127}});
        QVERIFY2(std::abs(frequency(rig.left, 0.05, 0.4) - hz) < hz * 0.002,
                 qPrintable(QString::number(frequency(rig.left, 0.05, 0.4))));
    }

    // SYNC fits the region to the beats: a 1 s sine as 1 beat at 120 BPM plays twice as fast.
    void sync()
    {
        const Sample s = sine(440.0, 1.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Sync, position(1, p::kSync)); // 1 beat
        rig.set(0, ParamId::Mode, position(2, p::kMode)); // loop
        rig.run(1.2, {{0.0, kNoteOn, 36, 127}});
        QVERIFY(std::abs(frequency(rig.left, 0.05, 0.45) - 880.0) < 2.0);
        QVERIFY(rms(rig.left, 0.6, 1.1) > 0.2); // looping on past the region's 0.5 s
    }

    // Reverse plays the ramp falling; START and END pick the region.
    void reverseAndRegion()
    {
        const Sample s = ramp(0.5);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::PadPan, 0.5f);
        rig.set(0, ParamId::Reverse, 1.0f);
        rig.set(0, ParamId::Start, 0.5f);
        rig.run(0.5, {{0.0, kNoteOn, 36, 127}});
        // From the region's end (+0.5) down to its start (0): 0.25 s.
        QVERIFY(rig.left[std::size_t(0.01 * kRate)] > rig.left[std::size_t(0.2 * kRate)]);
        QVERIFY(rig.left[std::size_t(0.2 * kRate)] > 0.0f);
        QCOMPARE(rms(rig.left, 0.27, 0.5), 0.0); // and then it's over
    }

    // A loop whose ends don't meet (a ramp) wraps without a click: the seam is crossfaded.
    void loopSeamDoesNotClick()
    {
        const Sample s = ramp(0.25);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Mode, position(2, p::kMode));
        rig.run(1.0, {{0.0, kNoteOn, 36, 127}});
        // A plain wrap would jump by the ramp's whole height (1.0 at the pad's level).
        QVERIFY2(largestJump(rig.left, 0.01, 0.95) < 0.02, qPrintable(QString::number(largestJump(rig.left, 0.01, 0.95))));
        QVERIFY(rms(rig.left, 0.5, 0.95) > 0.1); // still looping
    }

    // Modes: One shot plays on when the key is let go; Gate and Loop stop.
    void modes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("playsOn");
        QTest::newRow("one shot") << 0 << true;
        QTest::newRow("gate") << 1 << false;
        QTest::newRow("loop") << 2 << false;
    }
    void modes()
    {
        QFETCH(int, mode);
        QFETCH(bool, playsOn);
        const Sample s = sine(440.0, 1.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Mode, position(mode, p::kMode));
        rig.run(0.6, {{0.0, kNoteOn, 36, 127}, {0.2, kNoteOff, 36, 0}});
        QCOMPARE(rms(rig.left, 0.3, 0.6) > 0.1, playsOn);
    }

    // A pad in a choke group cuts the others in it (an open hat by a closed one), within a few ms.
    void chokeGroups()
    {
        const Sample s = sine(440.0, 1.0);
        Rig rig;
        for (int pad : {0, 1, 2}) {
            rig.engine.setSample(pad, &s);
            rig.set(pad, ParamId::PadPan, 0.0f); // pad 0 on the left
        }
        rig.set(1, ParamId::PadPan, 1.0f); // pad 1 on the right
        rig.set(0, ParamId::Choke, position(1, p::kChoke));
        rig.set(1, ParamId::Choke, position(1, p::kChoke));
        rig.run(0.5, {{0.0, kNoteOn, 36, 127}, {0.2, kNoteOn, 37, 127}});
        QVERIFY(rms(rig.left, 0.1, 0.19) > 0.2);
        QVERIFY(rms(rig.left, 0.21, 0.5) < 1e-6); // pad 0, cut
        QVERIFY(rms(rig.right, 0.21, 0.5) > 0.2); // pad 1, playing
    }

    // Hitting a pad again mid-sound fades the last hit out instead of cutting it.
    void retriggerDoesNotClick()
    {
        const Sample s = sine(110.0, 1.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Start, 0.3f); // starting mid-wave, too
        rig.run(0.5, {{0.0, kNoteOn, 36, 127}, {0.1003, kNoteOn, 36, 127}, {0.2007, kNoteOn, 36, 127}});
        const double expected = 2.0 * std::numbers::pi * 110.0 / kRate * 0.5 * 1.42; // the sine's own steepest step
        QVERIFY2(largestJump(rig.left, 0.0, 0.5) < 3.0 * expected, qPrintable(QString::number(largestJump(rig.left, 0.0, 0.5))));
    }

    // Bits 2 leaves a handful of levels; Rate 1/32 holds each value for 32 samples.
    void crusher()
    {
        const Sample s = sine(100.0, 1.0, 48000.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::PadLevel, 1.0f);
        rig.set(0, ParamId::PadPan, 0.0f);
        rig.set(0, ParamId::Bits, 1.0f); // 2 bits
        rig.run(0.3, {{0.0, kNoteOn, 36, 127}});
        std::set<int> levels;
        for (std::size_t i = std::size_t(0.05 * kRate); i < std::size_t(0.25 * kRate); ++i)
            levels.insert(int(std::lround(rig.left[i] * 1000.0f)));
        QVERIFY2(levels.size() <= 4, qPrintable(QString::number(levels.size())));

        Rig held;
        held.engine.setSample(0, &s);
        held.set(0, ParamId::Downsample, 1.0f); // 1/32
        held.run(0.2, {{0.0, kNoteOn, 36, 127}});
        int changes = 0;
        for (std::size_t i = std::size_t(0.05 * kRate) + 1; i < std::size_t(0.15 * kRate); ++i)
            changes += held.left[i] != held.left[i - 1] ? 1 : 0;
        QVERIFY2(changes <= int(0.1 * kRate / 32) + 2, qPrintable(QString::number(changes)));
    }

    // The low-pass takes the top off; the high-pass the bottom.
    void filter()
    {
        const Sample s = sine(3000.0, 1.0, 48000.0);
        double level[3];
        for (int type = 0; type < 3; ++type) {
            Rig rig;
            rig.engine.setSample(0, &s);
            rig.set(0, ParamId::Filter, position(type, p::kFilter));                     // off, LP, HP
            rig.set(0, ParamId::Cutoff, float(std::log(300.0 / 20.0) / std::log(1000.0))); // 300 Hz
            rig.run(0.3, {{0.0, kNoteOn, 36, 127}});
            level[type] = rms(rig.left, 0.05, 0.3);
        }
        QVERIFY(level[1] < 0.03 * level[0]);
        QVERIFY(level[2] > 0.9 * level[0]);
    }

    // The trance gate chops gated pads on the 1/16 grid of the transport; others stay steady.
    void tranceGate()
    {
        const Sample s = sine(1000.0, 2.0, 48000.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::Gate, 1.0f);
        rig.engine.setGatePattern(0x5555); // on, off, on, off ...
        rig.engine.setParam(ParamId::GateSmooth, 0.0f);
        rig.transport.setPlaying(true);
        rig.run(1.0, {{0.0, kNoteOn, 36, 127}});
        const double step = 0.125; // 1/16 at 120 BPM
        for (int k = 1; k < 6; ++k) {
            const double t = k * step;
            const double level = rms(rig.left, t + 0.01, t + step - 0.01);
            if (k % 2 == 0)
                QVERIFY2(level > 0.2, qPrintable(QString::number(level)));
            else
                QVERIFY2(level < 0.01, qPrintable(QString::number(level)));
        }
    }

    // The delay send echoes a hit a 1/8 dotted later (at 120 BPM: 375 ms).
    void delaySend()
    {
        const Sample s = sine(1000.0, 0.05, 48000.0);
        Rig rig;
        rig.engine.setSample(0, &s);
        rig.set(0, ParamId::DelaySend, 1.0f);
        rig.engine.setParam(ParamId::DelayLevel, 1.0f);
        rig.engine.setParam(ParamId::ReverbLevel, 0.0f);
        rig.run(0.6, {{0.0, kNoteOn, 36, 127}});
        QVERIFY(rms(rig.left, 0.1, 0.36) < 1e-4);
        QVERIFY(rms(rig.left, 0.38, 0.42) > 0.05);
    }

    // A new sample stops the pad's voices at once; the engine counts its blocks.
    void sampleSwap()
    {
        const Sample a = sine(440.0, 1.0), b = sine(880.0, 1.0);
        Rig rig;
        rig.engine.setSample(0, &a);
        rig.run(0.1, {{0.0, kNoteOn, 36, 127}});
        QVERIFY(rig.engine.padSounding(0));
        const auto before = rig.engine.blocksProcessed();
        rig.engine.setSample(0, &b);
        rig.run(0.01);
        QVERIFY(!rig.engine.padSounding(0));
        QVERIFY(rig.engine.blocksProcessed() > before);
        rig.engine.setSample(0, nullptr);
        rig.run(0.1, {{0.0, kNoteOn, 36, 127}});
        QCOMPARE(rms(rig.left, 0.12, 0.2), 0.0); // no sample: silence
    }

    void deterministic()
    {
        const Sample s = sine(220.0, 1.0);
        std::vector<float> out[2];
        for (auto &o : out) {
            Rig rig;
            rig.engine.setSample(0, &s);
            rig.set(0, ParamId::Drive, 0.8f);
            rig.set(0, ParamId::Ring, 0.5f);
            rig.set(0, ParamId::ReverbSend, 0.5f);
            rig.run(0.3, {{0.0, kNoteOn, 36, 127}});
            o = rig.left;
        }
        QVERIFY(out[0] == out[1]);
    }
};

QTEST_GUILESS_MAIN(TestPads)
#include "tst_pads.moc"
