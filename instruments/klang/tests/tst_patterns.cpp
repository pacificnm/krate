#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "core/Transport.h"
#include "klang/dsp/Engine.h"

using namespace klang;
namespace p = klang::positions;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;
constexpr double kStep = 0.125; // a 1/16 at 120 BPM, seconds

// A 5 ms burst of 1 kHz: each hit a clear onset.
Sample blip(double hz = 1000.0, double seconds = 0.005)
{
    std::vector<float> x(std::size_t(seconds * kRate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = float(0.5 * std::sin(2.0 * std::numbers::pi * hz * double(i) / kRate));
    return Sample::fromChannels(std::move(x), {}, kRate, "blip");
}

struct Rig
{
    Engine engine;
    krate::Transport transport;
    std::vector<float> out;
    Sample sample = blip();

    Rig()
    {
        engine.prepare(kRate, kBlock);
        engine.setParam(ParamId::Volume, 1.0f);
        engine.setParam(ParamId::DelayLevel, 0.0f);
        engine.setParam(ParamId::ReverbLevel, 0.0f);
        for (int pad = 0; pad < Engine::kPads; ++pad)
            engine.setSample(pad, &sample);
        transport.setBpm(120.0);
        transport.setPlaying(true);
    }
    void run(double seconds, std::vector<krate::MidiEvent> midi = {})
    {
        std::vector<float> l(kBlock), r(kBlock);
        for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
            const krate::TransportState state = transport.advance(kBlock, kRate);
            krate::processStereo(engine, l.data(), r.data(), kBlock, f == 0 ? midi.data() : nullptr,
                                 f == 0 ? std::uint32_t(midi.size()) : 0, &state);
            out.insert(out.end(), l.begin(), l.end());
        }
    }
    // When each hit starts (seconds): loud after at least 2 ms of silence.
    std::vector<double> onsets() const
    {
        std::vector<double> t;
        int quiet = 1000;
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (std::fabs(out[i]) > 0.02f) {
                if (quiet > int(0.002 * kRate))
                    t.push_back(double(i) / kRate);
                quiet = 0;
            } else {
                ++quiet;
            }
        }
        return t;
    }
    double peakAround(double t) const
    {
        float peak = 0.0f;
        for (auto i = std::size_t(t * kRate); i < std::size_t((t + 0.006) * kRate) && i < out.size(); ++i)
            peak = std::max(peak, std::fabs(out[i]));
        return peak;
    }
};

void on(Pattern &pattern, int pad, std::initializer_list<int> steps)
{
    for (int s : steps)
        pattern.steps[std::size_t(pad)][std::size_t(s)].on = true;
}

bool near(double a, double b) { return std::abs(a - b) < 0.0015; } // a block and a bit

} // namespace

class TestPatterns : public QObject
{
    Q_OBJECT

private slots:
    void stepsOnTheGrid()
    {
        Rig rig;
        Pattern pattern;
        on(pattern, 0, {0, 4, 8, 12});
        rig.engine.setPattern(0, pattern);
        rig.run(2.2);
        const auto t = rig.onsets();
        QCOMPARE(int(t.size()), 5); // and the next pass's first
        for (int k = 0; k < 5; ++k)
            QVERIFY2(near(t[std::size_t(k)], k * 4 * kStep), qPrintable(QString::number(t[std::size_t(k)])));
    }

    // Swing 75 %: the off-beat sixteenth falls half a step late.
    void swing()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Swing, 1.0f);
        Pattern pattern;
        on(pattern, 0, {0, 1, 2, 3});
        rig.engine.setPattern(0, pattern);
        rig.run(0.6);
        const auto t = rig.onsets();
        QVERIFY(t.size() >= 4);
        QVERIFY(near(t[1], 1.5 * kStep));
        QVERIFY(near(t[2], 2 * kStep));
        QVERIFY(near(t[3], 3.5 * kStep));
    }

    // Repeat: that many hits spread through the step; accent: louder.
    void repeatAndAccent()
    {
        Rig rig;
        Pattern pattern;
        on(pattern, 0, {0, 4});
        pattern.steps[0][0].repeat = 4;
        pattern.steps[0][4].accent = true;
        rig.engine.setPattern(0, pattern);
        rig.run(0.7);
        const auto t = rig.onsets();
        QCOMPARE(int(t.size()), 5);
        for (int k = 0; k < 4; ++k)
            QVERIFY(near(t[std::size_t(k)], k * kStep / 4));
        QVERIFY(rig.peakAround(t[4]) > 1.2 * rig.peakAround(t[0]));
    }

    // A step's pitch and reverse.
    void pitchAndReverse()
    {
        Rig rig;
        const Sample tone = blip(500.0, 0.1);
        rig.engine.setSample(0, &tone);
        Pattern pattern;
        on(pattern, 0, {0});
        pattern.steps[0][0].pitch = 12;
        rig.engine.setPattern(0, pattern);
        rig.run(0.06);
        int crossings = 0;
        for (std::size_t i = 1; i < std::size_t(0.04 * kRate); ++i)
            crossings += rig.out[i - 1] < 0.0f && rig.out[i] >= 0.0f ? 1 : 0;
        QVERIFY2(std::abs(crossings - 40) <= 1, qPrintable(QString::number(crossings))); // 1 kHz for 40 ms

        // Reverse: a ramp plays falling.
        Rig back;
        std::vector<float> ramp(4800);
        for (std::size_t i = 0; i < ramp.size(); ++i)
            ramp[i] = float(double(i) / 4800.0);
        const Sample up = Sample::fromChannels(ramp, {}, kRate, "ramp");
        back.engine.setSample(0, &up);
        back.engine.setPadParam(0, ParamId::PadPan, 0.0f);
        Pattern reversed;
        on(reversed, 0, {0});
        reversed.steps[0][0].reverse = true;
        back.engine.setPattern(0, reversed);
        back.run(0.1);
        QVERIFY(back.out[std::size_t(0.01 * kRate)] > back.out[std::size_t(0.08 * kRate)]);
    }

    // A pattern's length: it wraps there.
    void length()
    {
        Rig rig;
        Pattern pattern;
        pattern.length = 3;
        on(pattern, 0, {0});
        rig.engine.setPattern(0, pattern);
        rig.run(1.0);
        const auto t = rig.onsets();
        QVERIFY(t.size() >= 3);
        QVERIFY(near(t[1], 3 * kStep));
        QVERIFY(near(t[2], 6 * kStep));
    }

    // A cued pattern starts at the end of the pass; its gate steps come with it.
    void changeAtTheEndOfThePass()
    {
        Rig rig;
        Pattern a, b;
        on(a, 0, {0});
        on(b, 0, {2});
        b.gate = 0x00FF;
        rig.engine.setPattern(0, a);
        rig.engine.setPattern(1, b);
        rig.run(0.5);
        rig.engine.cuePattern(1);
        rig.run(1.9); // to 2.4 s: pattern B's first hit is at 2.25 s
        const auto t = rig.onsets();
        QVERIFY(t.size() >= 2);
        QVERIFY(near(t[0], 0.0));
        QVERIFY2(near(t[1], 16 * kStep + 2 * kStep), qPrintable(QString::number(t[1]))); // pattern B's step 3
        QCOMPARE(rig.engine.playingPattern(), 1);
        QCOMPARE(int(rig.engine.gatePattern()), 0x00FF);
    }

    // Program Change cues a pattern; stopped, the cue is the pattern at once.
    void programChange()
    {
        Rig rig;
        rig.transport.setPlaying(false);
        rig.run(0.01, {{0, 2, {0xC9, 5, 0}}});
        QCOMPARE(rig.engine.cuedPattern(), 5);
        rig.run(0.01);
        QCOMPARE(rig.engine.playingPattern(), 5);
    }

    // Switched off (the rack's RUN for Klang), the pattern doesn't play.
    void switchedOff()
    {
        Rig rig;
        Pattern pattern;
        on(pattern, 0, {0, 4});
        rig.engine.setPattern(0, pattern);
        rig.engine.setSequencerEnabled(false);
        rig.run(1.0);
        QVERIFY(rig.onsets().empty());
        QCOMPARE(rig.engine.playingStep(), -1);
    }

    // A Gate pad from a step holds for most of the step, then lets go.
    void gatePadHoldsItsStep()
    {
        Rig rig;
        const Sample tone = blip(500.0, 1.0);
        rig.engine.setSample(0, &tone);
        rig.engine.setPadParam(0, ParamId::Mode, position(1, p::kMode));
        Pattern pattern;
        on(pattern, 0, {0});
        rig.engine.setPattern(0, pattern);
        rig.run(0.5);
        double before = 0.0, after = 0.0;
        for (auto i = std::size_t(0.05 * kRate); i < std::size_t(0.1 * kRate); ++i)
            before = std::max(before, double(std::fabs(rig.out[i])));
        for (auto i = std::size_t(0.15 * kRate); i < std::size_t(0.45 * kRate); ++i)
            after = std::max(after, double(std::fabs(rig.out[i])));
        QVERIFY(before > 0.03);
        QCOMPARE(after, 0.0);
    }
};

QTEST_GUILESS_MAIN(TestPatterns)
#include "tst_patterns.moc"
