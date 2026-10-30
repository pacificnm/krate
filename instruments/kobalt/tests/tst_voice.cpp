#include <QtTest>

#include <cmath>
#include <set>
#include <string_view>
#include <vector>

#include "kobalt/dsp/Engine.h"

using namespace kobalt;
namespace p = kobalt::positions;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;

struct Event
{
    double time;
    std::uint8_t status, d1, d2;
    bool sequenced = false;
};

std::vector<float> render(Engine &engine, std::vector<Event> events, double seconds)
{
    std::vector<float> out;
    std::vector<float> l(kBlock), r(kBlock);
    std::size_t next = 0;
    const auto frames = std::uint64_t(seconds * kRate);
    for (std::uint64_t start = 0; start < frames; start += kBlock) {
        std::vector<krate::MidiEvent> midi;
        while (next < events.size() && std::uint64_t(events[next].time * kRate) < start + kBlock) {
            const Event &e = events[next++];
            midi.push_back({std::uint32_t(std::uint64_t(e.time * kRate) - start), 3, {e.status, e.d1, e.d2},
                            e.sequenced});
        }
        krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
        out.insert(out.end(), l.begin(), l.end());
    }
    return out;
}

// A plain test sound: the saw alone, the filter wide open, an organ envelope, no drift.
Engine &plain(Engine &e)
{
    e.prepare(kRate, kBlock);
    e.setParam(ParamId::Cutoff, 1.0f);
    e.setParam(ParamId::Res, 0.0f);
    e.setParam(ParamId::VcfEnv, 0.0f);
    e.setParam(ParamId::KeyFollow, 0.0f);
    e.setParam(ParamId::EnvA, 0.0f);
    e.setParam(ParamId::EnvS, 1.0f);
    e.setParam(ParamId::EnvR, 0.0f);
    e.setParam(ParamId::Analog, 0.0f);
    e.setParam(ParamId::Velocity, 0.0f);
    return e;
}

// Frequency from upward zero crossings between `from` and `to` seconds.
double frequency(const std::vector<float> &x, double from, double to)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (auto i = std::size_t(from * kRate); i < std::size_t(to * kRate); ++i) {
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
            if (first < 0.0)
                first = t;
            else
                ++count;
            last = t;
        }
    }
    return count > 0 ? count * kRate / (last - first) : 0.0;
}

double rms(const std::vector<float> &x, double from, double to)
{
    double sum = 0.0;
    const auto a = std::size_t(from * kRate), b = std::size_t(to * kRate);
    for (std::size_t i = a; i < b; ++i)
        sum += double(x[i]) * x[i];
    return std::sqrt(sum / double(b - a));
}

std::vector<Event> hold(int note, double length = 10.0)
{
    return {{0.0, 0x90, std::uint8_t(note), 100}, {length, 0x80, std::uint8_t(note), 0}};
}

double semitones(double hz, double reference) { return 12.0 * std::log2(hz / reference); }

} // namespace

class TestVoice : public QObject
{
    Q_OBJECT

private slots:
    // kParams is indexed by ParamId; keys never repeat; every value formats.
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QCOMPARE(ParamCount, 42);
        QCOMPARE(paramDescriptor(ParamId::LfoRate).key, std::string_view("lfo_rate"));
        QCOMPARE(paramDescriptor(ParamId::SubType).key, std::string_view("sub_type"));
        QCOMPARE(paramDescriptor(ParamId::EnvTrig).key, std::string_view("env_trig"));
        QCOMPARE(paramDescriptor(ParamId::ReverbSize).key, std::string_view("reverb_size"));
        QCOMPARE(paramDescriptor(ParamId::Tempo).key, std::string_view("tempo"));
        std::set<std::string_view> keys;
        for (const auto &info : kParams) {
            QVERIFY(keys.insert(info.key).second);
            QVERIFY(info.defaultValue >= 0.0f && info.defaultValue <= 1.0f);
            for (float v : {0.0f, 0.5f, 1.0f})
                QVERIFY(!info.format(v).empty());
            QCOMPARE(krate::snapToDetent(info, info.defaultValue), info.defaultValue); // on a detent
        }
        const auto text = [](ParamId id, float v) { return paramDescriptor(id).format(v); };
        QCOMPARE(text(ParamId::VcoRange, position(1, p::kVcoRange)), std::string("8'"));
        QCOMPARE(text(ParamId::SubType, 1.0f), std::string("2 Oct Pulse"));
        QCOMPARE(text(ParamId::EnvTrig, 0.0f), std::string("Gate + Trig"));
        QCOMPARE(text(ParamId::PortaMode, 0.5f), std::string("Auto"));
        QCOMPARE(text(ParamId::PortaTime, 0.0f), std::string("0 ms"));
        QCOMPARE(text(ParamId::PortaTime, 1.0f), std::string("2.00 s"));
        QCOMPARE(text(ParamId::Pw, 0.0f), std::string("50%"));
        QCOMPARE(text(ParamId::Pw, 1.0f), std::string("5%"));
        QCOMPARE(text(ParamId::BendVco, paramDescriptor(ParamId::BendVco).defaultValue), std::string("2 st"));
        QCOMPARE(text(ParamId::VcoMod, 1.0f), std::string("1200 cents"));
        QCOMPARE(text(ParamId::Cutoff, 1.0f), std::string("20.00 kHz"));
    }

    void ranges_data()
    {
        QTest::addColumn<int>("range");
        QTest::addColumn<double>("hz");
        QTest::newRow("16'") << 0 << 220.0;
        QTest::newRow("8'") << 1 << 440.0;
        QTest::newRow("4'") << 2 << 880.0;
        QTest::newRow("2'") << 3 << 1760.0;
    }
    void ranges()
    {
        QFETCH(int, range);
        QFETCH(double, hz);
        Engine e;
        plain(e);
        e.setParam(ParamId::VcoRange, position(range, p::kVcoRange));
        const auto out = render(e, hold(69), 0.6);
        QVERIFY2(std::abs(frequency(out, 0.1, 0.6) - hz) < hz * 0.002,
                 qPrintable(QString::number(frequency(out, 0.1, 0.6))));
    }

    // The sub, alone: one octave down, two, and the narrow pulse two down.
    void subOctaves_data()
    {
        QTest::addColumn<int>("type");
        QTest::addColumn<double>("hz");
        QTest::newRow("1 oct") << 0 << 220.0;
        QTest::newRow("2 oct") << 1 << 110.0;
        QTest::newRow("2 oct pulse") << 2 << 110.0;
    }
    void subOctaves()
    {
        QFETCH(int, type);
        QFETCH(double, hz);
        Engine e;
        plain(e);
        e.setParam(ParamId::SawLevel, 0.0f);
        e.setParam(ParamId::SubLevel, 1.0f);
        e.setParam(ParamId::SubType, position(type, p::kSubType));
        const auto out = render(e, hold(69), 0.6);
        QVERIFY2(std::abs(frequency(out, 0.1, 0.6) - hz) < hz * 0.002,
                 qPrintable(QString::number(frequency(out, 0.1, 0.6))));
    }

    // Pulse Width narrows the pulse: the share of each cycle spent high.
    void pulseWidth()
    {
        double high[2];
        for (int narrow = 0; narrow < 2; ++narrow) {
            Engine e;
            plain(e);
            e.setParam(ParamId::SawLevel, 0.0f);
            e.setParam(ParamId::PulseLevel, 1.0f);
            e.setParam(ParamId::Pw, narrow ? 0.8f : 0.0f); // manual: 50 %, 14 %
            const auto out = render(e, hold(45), 0.5);
            // Without its DC the pulse is high above the cycle's mean.
            double mean = 0.0;
            for (std::size_t i = std::size_t(0.1 * kRate); i < out.size(); ++i)
                mean += out[i];
            mean /= double(out.size() - std::size_t(0.1 * kRate));
            int above = 0;
            for (std::size_t i = std::size_t(0.1 * kRate); i < out.size(); ++i)
                above += out[i] > mean ? 1 : 0;
            high[narrow] = double(above) / double(out.size() - std::size_t(0.1 * kRate));
        }
        QVERIFY2(std::abs(high[0] - 0.5) < 0.03, qPrintable(QString::number(high[0])));
        QVERIFY2(std::abs(high[1] - 0.14) < 0.03, qPrintable(QString::number(high[1])));
    }

    // Every source down: exact silence. Noise alone: something, and no pitch.
    void mixer()
    {
        Engine silent;
        plain(silent);
        silent.setParam(ParamId::SawLevel, 0.0f);
        for (float s : render(silent, hold(60), 0.3))
            QCOMPARE(s, 0.0f);

        Engine noise;
        plain(noise);
        noise.setParam(ParamId::SawLevel, 0.0f);
        noise.setParam(ParamId::NoiseLevel, 1.0f);
        const auto out = render(noise, hold(60), 0.3);
        QVERIFY(rms(out, 0.05, 0.3) > 0.03); // a held saw: 0.11
        QVERIFY(frequency(out, 0.05, 0.3) > 3000.0); // crossings everywhere
    }

    // Full resonance, every source up and a cutoff sweep: it rings but stays bounded.
    void boundedAtFullResonance()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::Res, 1.0f);
        e.setParam(ParamId::Cutoff, 0.3f);
        e.setParam(ParamId::VcfEnv, 1.0f);
        e.setParam(ParamId::EnvD, 0.6f);
        e.setParam(ParamId::EnvS, 0.0f);
        e.setParam(ParamId::VcoRange, 0.0f);
        e.setParam(ParamId::SawLevel, 1.0f);
        e.setParam(ParamId::PulseLevel, 1.0f);
        e.setParam(ParamId::SubLevel, 1.0f);
        e.setParam(ParamId::NoiseLevel, 1.0f);
        e.setParam(ParamId::Volume, 1.0f);
        const auto out = render(e, hold(36), 2.0);
        float peak = 0.0f;
        for (float s : out) {
            QVERIFY(std::isfinite(s));
            peak = std::max(peak, std::fabs(s));
        }
        QVERIFY2(peak < 2.0f, qPrintable(QString::number(peak)));
        QVERIFY(peak > 0.05f);
    }

    // At full resonance the filter sings on its own at the cutoff, from a whisper of noise.
    void selfOscillates()
    {
        double level[2], hz = 0.0;
        for (int res = 0; res < 2; ++res) {
            Engine e;
            plain(e);
            e.setParam(ParamId::SawLevel, 0.0f);
            e.setParam(ParamId::NoiseLevel, 0.02f);
            e.setParam(ParamId::Cutoff, float(std::log(1000.0 / 20.0) / std::log(1000.0))); // 1 kHz
            e.setParam(ParamId::Res, float(res));
            const auto out = render(e, hold(60), 1.0);
            level[res] = rms(out, 0.5, 1.0);
            if (res == 1)
                hz = frequency(out, 0.5, 1.0);
        }
        QVERIFY2(level[1] > 20.0 * level[0], qPrintable(QStringLiteral("%1 %2").arg(level[1]).arg(level[0])));
        QVERIFY2(std::abs(hz - 1000.0) < 150.0, qPrintable(QString::number(hz)));
    }

    // Keyboard: the cutoff follows the note, so a high note through a low cutoff is brighter with it.
    void keyFollow()
    {
        double level[2];
        for (int follow = 0; follow < 2; ++follow) {
            Engine e;
            plain(e);
            e.setParam(ParamId::Cutoff, 0.4f); // ~320 Hz
            e.setParam(ParamId::KeyFollow, float(follow));
            level[follow] = rms(render(e, hold(96), 0.4), 0.1, 0.4);
        }
        QVERIFY(level[1] > 3.0 * level[0]);
    }

    // Last-note priority: a second key plays over the first; letting it go goes back.
    void notePriority()
    {
        Engine e;
        plain(e);
        const auto out = render(e, {{0.0, 0x90, 60, 100}, {0.4, 0x90, 64, 100}, {0.8, 0x80, 64, 0},
                                    {1.2, 0x80, 60, 0}},
                                1.6);
        QVERIFY(std::abs(frequency(out, 0.1, 0.38) - 261.63) < 0.6);
        QVERIFY(std::abs(frequency(out, 0.5, 0.78) - 329.63) < 0.7);
        QVERIFY(std::abs(frequency(out, 0.9, 1.18) - 261.63) < 0.6);
        QVERIFY(rms(out, 1.3, 1.6) < 1e-6); // released (release 1 ms)
    }

    // Portamento: Off jumps, Auto glides only legato, On always glides.
    void portamento_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("legato");
        QTest::addColumn<bool>("glides");
        QTest::newRow("off, legato") << 0 << true << false;
        QTest::newRow("auto, legato") << 1 << true << true;
        QTest::newRow("auto, detached") << 1 << false << false;
        QTest::newRow("on, detached") << 2 << false << true;
    }
    void portamento()
    {
        QFETCH(int, mode);
        QFETCH(bool, legato);
        QFETCH(bool, glides);
        Engine e;
        plain(e);
        e.setParam(ParamId::PortaMode, position(mode, p::kPortaMode));
        e.setParam(ParamId::PortaTime, 0.7f); // 330 ms
        // 57, then 69 half a second later: held over (legato) or released just before.
        std::vector<Event> events{{0.0, 0x90, 57, 100}, {legato ? 0.6 : 0.49, 0x80, 57, 0}, {0.5, 0x90, 69, 100}};
        std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.time < b.time; });
        render(e, events, 0.55);
        const double pitch = e.voice().pitch();
        if (glides)
            QVERIFY2(pitch > 58.0 && pitch < 68.0, qPrintable(QString::number(pitch)));
        else
            QCOMPARE(pitch, 69.0);
        Engine later; // and it gets there
        plain(later);
        later.setParam(ParamId::PortaMode, position(mode, p::kPortaMode));
        later.setParam(ParamId::PortaTime, 0.7f);
        render(later, events, 1.5);
        QVERIFY(std::abs(later.voice().pitch() - 69.0) < 0.01);
    }

    // A second key while the first is held: Gate + Trig starts the envelope
    // again, Gate lets it go on.
    void envelopeTriggers()
    {
        double level[2];
        for (int trig = 0; trig < 2; ++trig) {
            Engine e;
            plain(e);
            e.setParam(ParamId::EnvD, 0.5f); // 100 ms
            e.setParam(ParamId::EnvS, 0.0f);
            e.setParam(ParamId::EnvTrig, position(trig, p::kEnvTrig));
            const auto out = render(e, {{0.0, 0x90, 57, 100}, {0.5, 0x90, 60, 100}}, 0.6);
            level[trig] = rms(out, 0.505, 0.53);
        }
        QVERIFY2(level[0] > 0.01 && level[1] < 0.01 * level[0],
                 qPrintable(QStringLiteral("%1 %2").arg(level[0]).arg(level[1])));
    }

    // LFO trigger: the LFO starts the envelope every cycle while the key is down.
    void lfoTriggersTheEnvelope()
    {
        Engine e;
        plain(e);
        e.setParam(ParamId::EnvD, 0.4f); // 40 ms
        e.setParam(ParamId::EnvS, 0.0f);
        e.setParam(ParamId::EnvTrig, position(2, p::kEnvTrig));
        e.setParam(ParamId::LfoRate, float(std::log(40.0) / std::log(300.0))); // 4 Hz
        const auto out = render(e, hold(57), 2.0);
        // Count the bursts: windows of 10 ms that are loud after a quiet one.
        int bursts = 0;
        bool quiet = true;
        for (double t = 0.3; t < 1.8; t += 0.01) {
            const bool loud = rms(out, t, t + 0.01) > 0.01;
            if (loud && quiet)
                ++bursts;
            quiet = !loud;
        }
        QVERIFY2(bursts >= 5 && bursts <= 7, qPrintable(QString::number(bursts))); // 1.5 s at 4 Hz
    }

    // VCA Gate: the level holds while the key is down, whatever the envelope does.
    void vcaGate()
    {
        double level[2];
        for (int gate = 0; gate < 2; ++gate) {
            Engine e;
            plain(e);
            e.setParam(ParamId::EnvD, 0.4f);
            e.setParam(ParamId::EnvS, 0.0f);
            e.setParam(ParamId::VcaMode, float(gate));
            const auto out = render(e, {{0.0, 0x90, 57, 100}, {0.8, 0x80, 57, 0}}, 1.0);
            level[gate] = rms(out, 0.5, 0.75);
            if (gate == 1)
                QVERIFY(rms(out, 0.81, 1.0) < 1e-6); // and it closes with the key
        }
        QVERIFY(level[0] < 0.001);
        QVERIFY(level[1] > 0.05); // a held saw: 0.11
    }

    void pitchBend()
    {
        Engine e;
        plain(e);
        const auto out = render(e, {{0.0, 0xE0, 0x7F, 0x7F}, {0.0, 0x90, 69, 100}}, 0.5);
        QVERIFY(std::abs(semitones(frequency(out, 0.1, 0.5), 440.0) - 2.0) < 0.01);
    }

    void sustainPedal()
    {
        Engine e;
        plain(e);
        const auto out = render(e, {{0.0, 0xB0, 64, 127}, {0.0, 0x90, 60, 100}, {0.2, 0x80, 60, 0},
                                    {0.6, 0xB0, 64, 0}},
                                0.9);
        QVERIFY(rms(out, 0.3, 0.55) > 0.05); // held by the pedal (a held saw: 0.11)
        QVERIFY(rms(out, 0.7, 0.9) < 1e-6);  // released with it
    }

    // Octave shifts the player's keys; a song's notes play as written.
    void octave()
    {
        for (bool sequenced : {false, true}) {
            Engine e;
            plain(e);
            e.setParam(ParamId::Octave, position(4, p::kOctave)); // +1
            const auto out = render(e, {{0.0, 0x90, 69, 100, sequenced}}, 0.5);
            const double expected = sequenced ? 440.0 : 880.0;
            QVERIFY(std::abs(frequency(out, 0.1, 0.5) - expected) < expected * 0.002);
        }
    }

    void deterministic()
    {
        Engine a, b;
        plain(a);
        plain(b);
        for (Engine *e : {&a, &b}) {
            e->setParam(ParamId::Analog, 1.0f);
            e->setParam(ParamId::NoiseLevel, 0.3f);
            e->setParam(ParamId::VcoMod, 0.2f);
        }
        QVERIFY(render(a, hold(60), 0.3) == render(b, hold(60), 0.3));
    }
};

QTEST_GUILESS_MAIN(TestVoice)
#include "tst_voice.moc"
