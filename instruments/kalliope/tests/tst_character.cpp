#include <QtTest>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "kalliope/dsp/Chorus.h"
#include "kalliope/dsp/Engine.h"
#include "kalliope/dsp/Filters.h"

using namespace kalliope;

// The analog character (docs/DSP.md#analog, #building-blocks, #effects):
// drift and voice spread, the ladder's saturating feedback and drive, the
// chorus's darkened copies, the reverb, Unison Detune, aftertouch.
class TestCharacter : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Rig
    {
        Engine engine;
        std::vector<float> out;
        std::vector<krate::MidiEvent> pending;

        explicit Rig(int keyMode = 0)
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::Chorus, 0.0f);
            engine.setParam(ParamId::KeyMode, position(keyMode, positions::kKeyMode));
        }
        void send(std::uint8_t status, std::uint8_t d1, std::uint8_t d2) { pending.push_back({0, 3, {status, d1, d2}}); }
        void on(int note, int velocity = 100) { send(0x90, std::uint8_t(note), std::uint8_t(velocity)); }
        void off(int note) { send(0x80, std::uint8_t(note), 0); }
        void run(double seconds)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()));
                pending.clear();
                out.insert(out.end(), l.begin(), l.end());
            }
        }
        // A pure tone: VCO-2's sine alone, the filter wide open.
        void sine()
        {
            engine.setParam(ParamId::Vco2Wave, position(0, positions::kVco2Wave));
            engine.setParam(ParamId::Mix, 1.0f);
            engine.setParam(ParamId::Cutoff, 1.0f);
            engine.setParam(ParamId::VcfEnv, 0.0f);
            engine.setParam(ParamId::Res, 0.0f);
            engine.setParam(ParamId::Env2S, 1.0f);
        }
    };

    static double rms(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += double(x[i]) * x[i];
        return std::sqrt(sum / double(to - from));
    }

    // Frequency from the upward zero crossings in [from, to), interpolated.
    static double frequency(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double first = -1.0, last = -1.0;
        int cycles = -1;
        for (std::size_t i = from + 1; i < to; ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
                const double t = double(i - 1) + double(x[i - 1]) / double(x[i - 1] - x[i]);
                if (first < 0.0)
                    first = t;
                last = t;
                ++cycles;
            }
        return cycles > 0 ? cycles * kRate / (last - first) : 0.0;
    }

    // The pitch over 0.25 s windows of a 4 s note: the spread (max - min) in cents.
    static double pitchSpreadCents(float analog)
    {
        Rig rig(2); // Solo: voice 0
        rig.sine();
        rig.engine.setParam(ParamId::Analog, analog);
        rig.on(69);
        rig.run(4.0);
        double lo = 1e9, hi = 0.0;
        for (std::size_t w = 1; w < 16; ++w) {
            const double f = frequency(rig.out, w * 12000, (w + 1) * 12000);
            lo = std::min(lo, f);
            hi = std::max(hi, f);
        }
        return 1200.0 * std::log2(hi / lo);
    }

private slots:
    // Analog 0: a steady pitch; Analog 100 %: it wanders by cents, as an analog oscillator does.
    void analogDrifts()
    {
        const double still = pitchSpreadCents(0.0f);
        const double drifting = pitchSpreadCents(1.0f);
        QVERIFY2(still < 0.2, qPrintable(QString::number(still)));
        QVERIFY2(drifting > 3.0 && drifting < 25.0, qPrintable(QString::number(drifting)));
    }

    // Analog: the voices are not quite alike (each its own tuning, cutoff,
    // envelope times): two voices on one note differ; at 0 they match bit for bit.
    void analogVoicesDiffer()
    {
        const auto render = [](std::uint32_t seed, double analog) {
            Voice voice;
            voice.prepare(2.0 * kRate);
            voice.setSeed(seed);
            VoiceSettings s;
            s.vco2Wave = Wave::Sine;
            s.mix = 1.0;
            s.cutoffOctaves = std::log2(16000.0);
            s.env2 = {0.001, 0.3, 1.0, 0.3};
            s.analog = analog;
            voice.noteOn(81, 1.0f, true);
            std::vector<float> lfo(kBlock, 0.0f), out(2 * 24000, 0.0f);
            for (std::uint32_t f = 0; f < 24000; f += kBlock)
                voice.render(out.data() + 2 * f, kBlock, lfo.data(), s);
            return out;
        };
        QVERIFY(render(1u, 0.0) == render(2u, 0.0));
        const std::vector<float> a = render(1u, 1.0), b = render(2u, 1.0);
        // The same note, the two voices' pitches apart by cents (2x rate: frequency() reads half).
        const double cents = 1200.0 * std::log2(frequency(a, 9600, 48000) / frequency(b, 9600, 48000));
        QVERIFY2(std::fabs(cents) > 0.3 && std::fabs(cents) < 15.0, qPrintable(QString::number(cents)));
    }

    // The ladder's feedback saturates: a loud signal at the resonant peak gets
    // less of its boost than a quiet one (the resonance compresses).
    void resonanceCompresses()
    {
        const double fc = 1000.0;
        double gain[2];
        int i = 0;
        for (double amplitude : {0.02, 0.8}) {
            Ladder ladder;
            ladder.set(fc, 3.6, kRate);
            double in = 0.0, out = 0.0;
            for (int n = 0; n < 48000; ++n) {
                const double x = amplitude * std::sin(2.0 * std::numbers::pi * fc * n / kRate);
                const double y = ladder.process(float(x), true);
                if (n >= 24000) {
                    in += x * x;
                    out += y * y;
                }
            }
            gain[i++] = std::sqrt(out / in);
        }
        QVERIFY2(gain[1] < 0.6 * gain[0], qPrintable(QStringLiteral("%1 %2").arg(gain[0]).arg(gain[1])));
    }

    // Full resonance on silence rings (self-oscillation) and stays bounded.
    void selfOscillationIsBounded()
    {
        Ladder ladder;
        ladder.set(800.0, 4.2, kRate);
        float peak = 0.0f;
        for (int n = 0; n < 96000; ++n) {
            const float y = ladder.process(n == 0 ? 0.5f : 0.0f, true);
            QVERIFY(std::isfinite(y));
            if (n > 48000)
                peak = std::max(peak, std::fabs(y));
        }
        QVERIFY2(peak > 0.05f && peak < 2.0f, qPrintable(QString::number(peak)));
    }

    // Drive: clean at 0 (bit for bit the undriven ladder), harmonics at 100 %.
    void driveAddsHarmonics()
    {
        const double f0 = 200.0;
        const auto render = [&](double drive) {
            Ladder ladder;
            ladder.set(18000.0, 0.0, 2.0 * kRate, drive);
            std::vector<float> y(96000);
            for (std::size_t n = 0; n < y.size(); ++n)
                y[n] = ladder.process(float(0.5 * std::sin(2.0 * std::numbers::pi * f0 * double(n) / (2.0 * kRate))), true);
            return y;
        };
        Ladder plain;
        plain.set(18000.0, 0.0, 2.0 * kRate);
        const std::vector<float> clean = render(1.0);
        for (std::size_t n = 0; n < 1000; ++n)
            QCOMPARE(plain.process(float(0.5 * std::sin(2.0 * std::numbers::pi * f0 * double(n) / (2.0 * kRate))), true),
                     clean[n]);
        // The 3rd harmonic's level against the fundamental's (correlation over whole cycles).
        const auto third = [&](const std::vector<float> &y) {
            double a1 = 0, b1 = 0, a3 = 0, b3 = 0;
            for (std::size_t n = 48000; n < 96000; ++n) {
                const double w = 2.0 * std::numbers::pi * f0 * double(n) / (2.0 * kRate);
                a1 += y[n] * std::sin(w);
                b1 += y[n] * std::cos(w);
                a3 += y[n] * std::sin(3 * w);
                b3 += y[n] * std::cos(3 * w);
            }
            return std::hypot(a3, b3) / std::hypot(a1, b1);
        };
        const double cleanThird = third(clean), drivenThird = third(render(8.0));
        QVERIFY2(drivenThird > 0.1 && drivenThird > 5.0 * cleanThird,
                 qPrintable(QStringLiteral("%1 %2").arg(cleanThird).arg(drivenThird)));
    }

    // The chorus's copies are darker than the dry signal: a high tone gets
    // less of the copy than a low one, so it moves the level less.
    void chorusCopiesAreDark()
    {
        const auto wetShare = [](double hz) {
            Chorus chorus;
            chorus.prepare(kRate);
            const std::size_t n = 48000;
            std::vector<float> in(n), l(n), r(n);
            for (std::size_t i = 0; i < n; ++i)
                in[i] = float(0.3 * std::sin(2.0 * std::numbers::pi * hz * double(i) / kRate));
            chorus.process(in.data(), l.data(), r.data(), std::uint32_t(n), Chorus::I);
            // The copy alone: the side minus its scaled dry.
            double wet = 0.0, dry = 0.0;
            for (std::size_t i = 4800; i < n; ++i) {
                const double d = double(l[i]) - double(r[i]); // the dry cancels between the sides
                wet += d * d;
                dry += double(in[i]) * in[i];
            }
            return std::sqrt(wet / dry);
        };
        const double low = wetShare(440.0), high = wetShare(12000.0);
        QVERIFY2(high < 0.5 * low, qPrintable(QStringLiteral("%1 %2").arg(low).arg(high)));
    }

    // Reverb: a short note leaves a tail with Reverb Level up, none with it down.
    void reverbLeavesATail()
    {
        double tail[2], note = 0.0;
        for (int i = 0; i < 2; ++i) {
            Rig rig;
            rig.engine.setParam(ParamId::Env2D, 0.2f);
            rig.engine.setParam(ParamId::Env2S, 0.0f);
            rig.engine.setParam(ParamId::Env2R, 0.2f);
            rig.engine.setParam(ParamId::ReverbLevel, i == 0 ? 0.0f : 0.6f);
            rig.engine.setParam(ParamId::ReverbSize, 0.6f);
            rig.on(60);
            rig.run(0.05);
            rig.off(60);
            rig.run(1.0);
            tail[i] = rms(rig.out, 24000, 48000); // 0.5 .. 1 s: the note has long gone
            note = rms(rig.out, 0, 2400);
        }
        // Without: silence. With: the tail some 30 dB under the note, still there.
        QVERIFY2(tail[0] < 1e-6 * note && tail[1] > 0.01 * note,
                 qPrintable(QStringLiteral("%1 %2 %3").arg(note).arg(tail[0]).arg(tail[1])));
    }

    // Unison Detune 0: the stack sits on one pitch (a steady level); wide, it
    // beats (the level swells and dips).
    void unisonDetuneSpreads()
    {
        double swing[2];
        for (int i = 0; i < 2; ++i) {
            Rig rig(1);
            rig.sine();
            rig.engine.setParam(ParamId::Analog, 0.0f);
            rig.engine.setParam(ParamId::UnisonDetune, i == 0 ? 0.0f : 1.0f);
            rig.on(69);
            rig.run(2.0);
            double lo = 1e9, hi = 0.0;
            for (std::size_t w = 4; w < 200; ++w) { // 10 ms windows
                const double level = rms(rig.out, w * 480, (w + 1) * 480);
                lo = std::min(lo, level);
                hi = std::max(hi, level);
            }
            swing[i] = hi / std::max(lo, 1e-9);
        }
        QVERIFY2(swing[0] < 1.05 && swing[1] > 2.0, qPrintable(QStringLiteral("%1 %2").arg(swing[0]).arg(swing[1])));
    }

    // Aftertouch to Filter opens the cutoff (brighter, louder through a low
    // cutoff); with Pressure Off it does nothing.
    void pressureOpensTheFilter()
    {
        const auto level = [](int dest, int pressure) {
            Rig rig;
            rig.engine.setParam(ParamId::Cutoff, 0.25f);
            rig.engine.setParam(ParamId::VcfEnv, 0.0f);
            rig.engine.setParam(ParamId::Env2S, 1.0f);
            rig.engine.setParam(ParamId::PressureDest, position(dest, positions::kPressureDest));
            rig.on(60);
            rig.pending.push_back({0, 2, {0xD0, std::uint8_t(pressure), 0}});
            rig.run(0.5);
            return rms(rig.out, 12000, 24000);
        };
        QCOMPARE(level(0, 127), level(0, 0));
        QVERIFY2(level(1, 127) > 2.0 * level(1, 0), qPrintable(QStringLiteral("%1 %2").arg(level(1, 0)).arg(level(1, 127))));
    }

    // Aftertouch to LFO: vibrato appears with pressure (the pitch moves).
    void pressureAddsVibrato()
    {
        const auto spread = [](int pressure) {
            Rig rig(2);
            rig.sine();
            rig.engine.setParam(ParamId::Analog, 0.0f);
            rig.engine.setParam(ParamId::LfoRate, 0.6f);
            rig.engine.setParam(ParamId::PressureDest, position(2, positions::kPressureDest));
            rig.on(69);
            rig.pending.push_back({0, 2, {0xD0, std::uint8_t(pressure), 0}});
            rig.run(2.0);
            double lo = 1e9, hi = 0.0;
            for (std::size_t w = 4; w < 40; ++w) {
                const double f = frequency(rig.out, w * 2400, (w + 1) * 2400);
                lo = std::min(lo, f);
                hi = std::max(hi, f);
            }
            return 1200.0 * std::log2(hi / lo);
        };
        QVERIFY2(spread(0) < 1.0, qPrintable(QString::number(spread(0))));
        QVERIFY2(spread(127) > 20.0, qPrintable(QString::number(spread(127))));
    }
};

QTEST_GUILESS_MAIN(TestCharacter)
#include "tst_character.moc"
