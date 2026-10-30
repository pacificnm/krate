#include <QtTest>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include "krimson/dsp/Engine.h"
#include "krimson/dsp/Ensemble.h"
#include "krimson/dsp/Filter.h"

using namespace krimson;

// The analog character (docs/DSP.md#analog): drift, the ladder, resonance
// saturation, bigger unison, aftertouch, FX drive, the ensemble.
class TestCharacter : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Rig
    {
        Engine engine;
        std::vector<float> out, outR;
        std::vector<krate::MidiEvent> pending;

        Rig()
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::AenvR, 0.3f);
        }
        void send(std::uint8_t status, std::uint8_t d1, std::uint8_t d2, std::uint8_t size = 3)
        {
            pending.push_back({0, size, {status, d1, d2}});
        }
        void on(int note) { send(0x90, std::uint8_t(note), 100); }
        void run(double seconds)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()));
                pending.clear();
                out.insert(out.end(), l.begin(), l.end());
                outR.insert(outR.end(), r.begin(), r.end());
            }
        }
    };

    static double db(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }
    static double rms(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double s = 0.0;
        for (std::size_t i = from; i < to; ++i)
            s += double(x[i]) * x[i];
        return std::sqrt(s / double(to - from));
    }
    // Frequency from upward zero crossings in [from, to).
    static double frequency(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double first = -1.0, last = -1.0;
        int count = 0;
        for (std::size_t i = from + 1; i < to; ++i)
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
    // Energy above `hz` over the total (a crude brightness).
    static double brightness(const std::vector<float> &x, std::size_t from, std::size_t n, double hz)
    {
        double high = 0.0, all = 0.0;
        for (std::size_t k = 1; k < n / 2; k += 3) {
            std::complex<double> s = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                s += double(x[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * double(k * i) / double(n));
            const double e = std::norm(s);
            all += e;
            if (double(k) * kRate / double(n) > hz)
                high += e;
        }
        return high / all;
    }

private slots:
    // --- Analog ---------------------------------------------------------------------

    // At 0 nothing wanders: the same note twice sounds the same, and in tune.
    void analogOffIsSteady()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Analog, 0.0f);
        rig.engine.setParam(ParamId::Osc1Wave, position(0, positions::kOsc1Wave)); // a sine
        rig.engine.setParam(ParamId::OscMix, 0.0f);
        rig.engine.setParam(ParamId::Cutoff, 1.0f);
        rig.on(69);
        rig.run(2.0);
        const double early = frequency(rig.out, 24000, 48000), late = frequency(rig.out, 72000, 96000);
        QVERIFY(std::fabs(early - 440.0) < 0.5);
        QVERIFY(std::fabs(late - early) < 0.05);
    }

    // At full the pitch wanders over time, a few cents, never far.
    void analogDrifts()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Analog, 1.0f);
        rig.engine.setParam(ParamId::Osc1Wave, position(0, positions::kOsc1Wave));
        rig.engine.setParam(ParamId::OscMix, 0.0f);
        rig.engine.setParam(ParamId::Cutoff, 1.0f);
        rig.on(69);
        rig.run(4.0);
        double lo = 1e9, hi = 0.0;
        for (std::size_t at = 24000; at + 24000 <= rig.out.size(); at += 12000) {
            const double f = frequency(rig.out, at, at + 12000);
            lo = std::min(lo, f);
            hi = std::max(hi, f);
        }
        const double cents = 1200.0 * std::log2(hi / lo);
        QVERIFY2(cents > 1.0 && cents < 40.0, qPrintable(QStringLiteral("%1 cents").arg(cents)));
        QVERIFY(std::fabs(1200.0 * std::log2(lo / 440.0)) < 25.0);
    }

    // Repeatable: the same notes render the same every time (each voice's own seed).
    void analogIsRepeatable()
    {
        const auto render = [] {
            Rig rig;
            rig.engine.setParam(ParamId::Analog, 1.0f);
            rig.on(60);
            rig.on(64);
            rig.run(0.5);
            return rig.out;
        };
        QCOMPARE(render(), render());
    }

    // --- Filter -----------------------------------------------------------------------

    static double response(MultiFilter::Type type, double res, double hz, double amplitude = 0.01)
    {
        MultiFilter f;
        const double fs = 96000.0;
        f.set(1000.0, res, 0.0, fs);
        std::complex<double> acc = 0.0;
        const int n = 48000;
        for (int i = 0; i < 2 * n; ++i) {
            const double x = amplitude * std::sin(2.0 * std::numbers::pi * hz * i / fs);
            const float y = f.process(float(x), type);
            if (i >= n)
                acc += double(y) * std::polar(1.0, -2.0 * std::numbers::pi * hz * i / fs);
        }
        return 2.0 * std::abs(acc) / n / amplitude;
    }

    // LP 24 is a real four-pole: twice LP 12's slope, and its own resonance peak.
    void ladderIsFourPole()
    {
        const double slope24 = db(response(MultiFilter::Type::Lp24, 0.0, 8000.0)) - db(response(MultiFilter::Type::Lp24, 0.0, 4000.0));
        const double slope12 = db(response(MultiFilter::Type::Lp12, 0.0, 8000.0)) - db(response(MultiFilter::Type::Lp12, 0.0, 4000.0));
        QVERIFY2(slope24 < -20.0, qPrintable(QString::number(slope24)));
        QVERIFY(slope24 < slope12 - 8.0);
        QVERIFY(db(response(MultiFilter::Type::Lp24, 0.9, 1000.0)) > 8.0);  // the peak
        QVERIFY(db(response(MultiFilter::Type::Lp24, 0.0, 100.0)) > -1.0); // the passband
    }

    void ladderStaysBounded()
    {
        MultiFilter f;
        f.set(800.0, 1.0, 1.0, 96000.0);
        float peak = 0.0f;
        for (int i = 0; i < 96000; ++i) {
            const float y = f.process(i < 4800 ? (i % 200 < 100 ? 1.0f : -1.0f) : 0.0f, MultiFilter::Type::Lp24);
            QVERIFY(std::isfinite(y));
            peak = std::max(peak, std::fabs(y));
        }
        QVERIFY(peak <= 1.0f);
    }

    // The 12 dB modes' resonance compresses: at the peak, louder in is less than as much louder out.
    void resonanceSaturates()
    {
        const double quiet = response(MultiFilter::Type::Lp12, 0.95, 1000.0, 0.01);
        const double loud = response(MultiFilter::Type::Lp12, 0.95, 1000.0, 0.3);
        QVERIFY2(db(loud) < db(quiet) - 3.0, qPrintable(QStringLiteral("%1 vs %2").arg(db(loud)).arg(db(quiet))));
    }

    // --- Unison ------------------------------------------------------------------------

    void unisonVoices()
    {
        const auto play = [](int index) {
            Rig rig;
            rig.engine.setParam(ParamId::VoiceMode, position(3, positions::kVoiceMode)); // Unison
            rig.engine.setParam(ParamId::UnisonVoices, position(index, positions::kUnisonVoices));
            rig.on(48);
            rig.run(0.5);
            return std::pair{rig.engine.activeVoices(), rms(rig.out, 12000, 24000)};
        };
        const auto [three, l3] = play(0);
        const auto [five, l5] = play(1);
        const auto [seven, l7] = play(2);
        QCOMPARE(three, 3);
        QCOMPARE(five, 5);
        QCOMPARE(seven, 7);
        QVERIFY2(std::fabs(db(l7) - db(l3)) < 3.0, qPrintable(QStringLiteral("%1 vs %2 dB").arg(db(l7)).arg(db(l3))));
        QVERIFY(std::fabs(db(l5) - db(l3)) < 3.0);
    }

    // --- Aftertouch --------------------------------------------------------------------

    // Pressure to Filter: the cutoff up 2.5 octaves (here ~160 Hz to ~900 Hz),
    // so a saw's 5th harmonic comes up against its fundamental.
    void pressureOpensTheFilter()
    {
        const auto fifth = [](bool press) {
            Rig rig;
            rig.engine.setParam(ParamId::PressureDest, position(1, positions::kPressureDest)); // Filter
            rig.engine.setParam(ParamId::Cutoff, 0.3f);
            rig.engine.setParam(ParamId::FenvAmt, 0.0f);
            rig.engine.setParam(ParamId::Analog, 0.0f);
            rig.on(48);
            if (press)
                rig.send(0xD0, 127, 0, 2);
            rig.run(0.5);
            const double f0 = 440.0 * std::pow(2.0, (48 - 69) / 12.0);
            const auto at = [&](double hz) {
                std::complex<double> sum = 0.0;
                for (std::size_t i = 12000; i < 21600; ++i)
                    sum += double(rig.out[i]) * std::polar(1.0, -2.0 * std::numbers::pi * hz * double(i) / kRate);
                return std::abs(sum);
            };
            return db(at(5.0 * f0)) - db(at(f0));
        };
        QVERIFY2(fifth(true) > fifth(false) + 10.0, qPrintable(QStringLiteral("%1 vs %2 dB").arg(fifth(true)).arg(fifth(false))));
    }

    void pressureOffByDefault()
    {
        const auto render = [](bool press) {
            Rig rig;
            rig.on(48);
            if (press)
                rig.send(0xD0, 127, 0, 2);
            rig.run(0.3);
            return rig.out;
        };
        QCOMPARE(render(true), render(false));
    }

    // --- FX drive, ensemble ----------------------------------------------------------------

    void fxDriveAddsHarmonics()
    {
        const auto play = [](float drive) {
            Rig rig;
            rig.engine.setParam(ParamId::Osc1Wave, position(0, positions::kOsc1Wave)); // a sine
            rig.engine.setParam(ParamId::OscMix, 0.0f);
            rig.engine.setParam(ParamId::Cutoff, 1.0f);
            rig.engine.setParam(ParamId::Analog, 0.0f);
            rig.engine.setParam(ParamId::FxDrive, drive);
            rig.on(57);
            rig.run(0.5);
            return brightness(rig.out, 12000, 2048, 500.0);
        };
        QVERIFY(play(0.8f) > 5.0 * play(0.0f));
    }

    void ensembleModulatesAndOffIsDry()
    {
        Ensemble off, on;
        off.prepare(kRate);
        on.prepare(kRate);
        std::vector<float> l(48000), r(48000);
        for (std::size_t i = 0; i < l.size(); ++i)
            l[i] = r[i] = 0.3f * float(std::sin(2.0 * std::numbers::pi * 1000.0 * double(i) / kRate));
        auto l0 = l, r0 = r, l1 = l, r1 = r;
        off.process(l0.data(), r0.data(), std::uint32_t(l0.size()), Ensemble::Off);
        QCOMPARE(l0, l);
        on.process(l1.data(), r1.data(), std::uint32_t(l1.size()), Ensemble::Full);
        double lo = 1e9, hi = 0.0;
        for (std::size_t at = 4800; at + 480 < l1.size(); at += 480) {
            const double v = rms(l1, at, at + 480);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        QVERIFY(db(hi) - db(lo) > 1.0); // the copies move against the dry
        QVERIFY(l1 != r1);               // and differ side to side
    }
};

QTEST_APPLESS_MAIN(TestCharacter)
#include "tst_character.moc"
