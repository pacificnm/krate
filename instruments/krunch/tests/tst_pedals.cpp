#include <QtTest>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include "dsp/ClassicPedals.h"
#include "dsp/Fuzz.h"
#include "dsp/SignaturePedals.h"
#include "dsp/GreenDrive.h"
#include "dsp/ModPedals.h"
#include "dsp/StudioPedals.h"
#include "dsp/Pedal.h"
#include "dsp/TimePedals.h"

using namespace krunch;

namespace {

constexpr double kRate = 48000.0;

double db(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }

std::vector<float> sine(std::size_t n, double hz, float amplitude)
{
    std::vector<float> s(n);
    for (std::size_t i = 0; i < n; ++i)
        s[i] = amplitude * float(std::sin(2.0 * std::numbers::pi * hz * double(i) / kRate));
    return s;
}

std::vector<float> run(Pedal &p, std::vector<float> x, std::uint32_t block = 128)
{
    for (std::size_t at = 0; at < x.size(); at += block)
        p.process(x.data() + at, std::uint32_t(std::min<std::size_t>(block, x.size() - at)));
    return x;
}

// Amplitude of `hz` in x[from, from + n).
double level(const std::vector<float> &x, double hz, std::size_t from, std::size_t n)
{
    std::complex<double> sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        sum += double(x[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * hz * double(i) / kRate);
    return 2.0 * std::abs(sum) / double(n);
}

double thd(const std::vector<float> &x, double hz, std::size_t from, std::size_t n)
{
    double h = 0.0;
    for (int k = 2; k <= 10; ++k)
        h += std::pow(level(x, hz * k, from, n), 2.0);
    return std::sqrt(h) / level(x, hz, from, n);
}

std::unique_ptr<GreenDrive> drive(float d, float tone = 0.5f, float lvl = 0.5f)
{
    auto g = std::make_unique<GreenDrive>();
    g->prepare(kRate);
    g->setKnob(GreenDrive::Drive, d);
    g->setKnob(GreenDrive::Tone, tone);
    g->setKnob(GreenDrive::Level, lvl);
    return g;
}

// Small-signal gain of the pedal at hz (well below clipping).
double gainAt(float d, double hz, float tone = 0.5f)
{
    auto g = drive(d, tone);
    const auto y = run(*g, sine(24000, hz, 0.0005f));
    return level(y, hz, 12000, 9600) / 0.0005;
}

} // namespace

class TestPedals : public QObject
{
    Q_OBJECT

private slots:
    void types()
    {
        QCOMPARE(pedalTypes().size(), std::size_t(14));
        QCOMPARE(pedalTypes()[2]->type, std::string_view("green_drive"));
        for (const PedalInfo *info : pedalTypes()) {
            auto p = makePedal(info->type);
            QVERIFY(p);
            QCOMPARE(&p->info(), info);
            QVERIFY(info->knobs.size() <= std::size_t(Pedal::kMaxKnobs));
            for (const KnobInfo &k : info->knobs)
                QVERIFY(!k.format(k.defaultValue).empty());
        }
        QVERIFY(!makePedal("green_drive")->info().loopByDefault);
        QVERIFY(makePedal("delay")->info().loopByDefault);
        QVERIFY(!makePedal("nope"));
        QCOMPARE(GreenDrive::kInfo.knobs.size(), std::size_t(3));
    }

    // --- Footswitch ---------------------------------------------------------------

    void offPassesTheSignalUntouched()
    {
        auto g = drive(1.0f);
        g->setOn(false);
        const auto x = sine(4800, 220.0, 0.3f);
        QCOMPARE(run(*g, x), x);
    }

    void switchingFadesWithoutAClick()
    {
        auto g = drive(0.8f);
        g->setOn(false);
        auto x = sine(24000, 110.0, 0.2f);
        std::vector<float> y;
        for (std::size_t at = 0; at < x.size(); at += 128) {
            if (at == 9600)
                g->setOn(true);
            if (at == 19200)
                g->setOn(false);
            g->process(x.data() + at, 128);
        }
        // No sample-to-sample step bigger than the drive itself makes while on.
        float steady = 0.0f, worst = 0.0f;
        for (std::size_t i = 12001; i < 19000; ++i)
            steady = std::max(steady, std::fabs(x[i] - x[i - 1]));
        for (std::size_t i = 9500; i < 9600 + 1000; ++i)
            worst = std::max(worst, std::fabs(x[i] - x[i - 1]));
        for (std::size_t i = 19100; i < 19200 + 1000; ++i)
            worst = std::max(worst, std::fabs(x[i] - x[i - 1]));
        QVERIFY2(worst < 1.5f * steady, qPrintable(QStringLiteral("%1 vs %2").arg(worst).arg(steady)));
    }

    void aNewPedalFadesIn()
    {
        auto g = drive(0.5f);
        std::vector<float> x(128, 0.3f);
        g->process(x.data(), 128);
        QVERIFY(std::fabs(x[0] - 0.3f) < 0.01f); // starts as the dry signal, then fades to the effect
    }

    // --- Green Drive ------------------------------------------------------------------

    // The mid hump: the clipping stage only amplifies above ~720 Hz.
    void boostsTheMidsNotTheBass()
    {
        const double bass = gainAt(0.5f, 80.0), mids = gainAt(0.5f, 1000.0);
        QVERIFY2(db(mids) - db(bass) > 12.0, qPrintable(QStringLiteral("%1 vs %2 dB").arg(db(mids)).arg(db(bass))));
    }

    void moreDriveMoreGainAndDistortion()
    {
        QVERIFY(db(gainAt(1.0f, 1000.0)) > db(gainAt(0.0f, 1000.0)) + 12.0);
        auto low = drive(0.1f), high = drive(0.9f);
        const auto a = run(*low, sine(24000, 330.0, 0.1f)), b = run(*high, sine(24000, 330.0, 0.1f));
        QVERIFY(thd(b, 330.0, 12000, 9600) > 2.0 * thd(a, 330.0, 12000, 9600));
    }

    void toneBrightens()
    {
        QVERIFY(db(gainAt(0.3f, 4000.0, 1.0f)) > db(gainAt(0.3f, 4000.0, 0.0f)) + 10.0);
        // The lows hardly move.
        QVERIFY(std::fabs(db(gainAt(0.3f, 100.0, 1.0f)) - db(gainAt(0.3f, 100.0, 0.0f))) < 2.0);
    }

    void levelIsAVolume()
    {
        const auto at = [](float lvl) {
            auto g = drive(0.5f, 0.5f, lvl);
            const auto y = run(*g, sine(9600, 330.0, 0.1f));
            return db(level(y, 330.0, 4800, 4800));
        };
        QVERIFY(std::fabs((at(1.0f) - at(0.0f)) - 36.0) < 0.5);
    }

    // The diodes cap it: even a hot guitar at full drive comes out bounded.
    void clippingIsBounded()
    {
        auto g = drive(1.0f, 1.0f, 1.0f);
        const auto y = run(*g, sine(9600, 165.0, 0.8f));
        float peak = 0.0f;
        for (float v : y)
            peak = std::max(peak, std::fabs(v));
        QVERIFY(std::isfinite(peak));
        QVERIFY2(peak < 8.0f, qPrintable(QString::number(peak))); // +12 dB of level on (input + 0.6)
    }

    void littleAliasing()
    {
        auto g = drive(1.0f, 1.0f);
        const double hz = 1318.5;
        const auto y = run(*g, sine(24000, hz, 0.3f));
        // Energy away from the harmonics, against all of it.
        const std::size_t from = 12000, n = 4096;
        double harmonic = 0.0, other = 0.0;
        for (std::size_t k = 1; k < n / 2; ++k) {
            std::complex<double> sum = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(n - 1));
                sum += w * double(y[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * double(k * i) / double(n));
            }
            const double f = double(k) * kRate / double(n), nearest = std::round(f / hz) * hz;
            (std::fabs(f - nearest) <= 3.0 * kRate / double(n) && nearest > 0.0 ? harmonic : other) += std::norm(sum);
        }
        QVERIFY(other / (harmonic + other) < 0.01);
    }

    // --- Prism, Vibe, Drum Echo ------------------------------------------------------------

    // Every part out, tone flat, level 0 dB: the guitar passes untouched.
    void prismAllOutIsDry()
    {
        Prism p;
        p.prepare(kRate);
        for (int k : {Prism::Fuzz, Prism::VibeSpeed, Prism::Echo, Prism::Space})
            p.setKnob(k, 0.0f);
        p.setKnob(Prism::Tone, 0.5f);
        p.setKnob(Prism::Level, 0.5f);
        const auto x = sine(9600, 220.0, 0.3f), y = run(p, x);
        float diff = 0.0f;
        for (std::size_t i = 4800; i < x.size(); ++i)
            diff = std::max(diff, std::fabs(y[i] - x[i]));
        QVERIFY2(diff < 1e-5f, qPrintable(QString::number(diff)));
    }

    void prismFuzzForTheSolos()
    {
        const auto distortion = [](float fuzz) {
            Prism p;
            p.prepare(kRate);
            for (int k : {Prism::VibeSpeed, Prism::Echo, Prism::Space})
                p.setKnob(k, 0.0f);
            p.setKnob(Prism::Fuzz, fuzz);
            const auto y = run(p, sine(24000, 220.0, 0.2f));
            return thd(y, 220.0, 12000, 9600);
        };
        QVERIFY(distortion(0.0f) < 0.01);
        QVERIFY(distortion(0.6f) > 0.2);
    }

    void prismEchoRepeatsOnEveryHead()
    {
        Prism p;
        p.prepare(kRate);
        for (int k : {Prism::Fuzz, Prism::VibeSpeed, Prism::Space})
            p.setKnob(k, 0.0f);
        p.setKnob(Prism::Echo, 0.8f); // all heads
        p.setKnob(Prism::Time, 0.3f);
        std::vector<float> warm(4800, 0.0f);
        run(p, warm);
        std::vector<float> y(48000, 0.0f);
        y[0] = 0.5f;
        y = run(p, y);
        const auto head = std::size_t(std::lround(DrumEcho::headSeconds(0.3f) * kRate));
        for (int h = 1; h <= 4; ++h) {
            float e = 0.0f;
            for (std::size_t i = std::size_t(h) * head - 30; i < std::size_t(h) * head + 30; ++i)
                e = std::max(e, std::fabs(y[i]));
            QVERIFY2(e > 0.02f, qPrintable(QStringLiteral("head %1: %2").arg(h).arg(e)));
        }
    }

    void drumEchoHeads()
    {
        QCOMPARE(DrumEcho::headMask(0.0f), 0b0001u);
        QCOMPARE(DrumEcho::headMask(1.0f / 3.0f), 0b0011u);
        QCOMPARE(DrumEcho::headMask(2.0f / 3.0f), 0b0101u);
        QCOMPARE(DrumEcho::headMask(1.0f), 0b1111u);
        // Heads 1+3 only: echoes at 1x and 3x, not 2x (before any swell repeats them).
        DrumEcho d;
        d.prepare(kRate);
        d.setKnob(DrumEcho::Heads, 2.0f / 3.0f);
        d.setKnob(DrumEcho::Swell, 0.0f);
        d.setKnob(DrumEcho::Mix, 1.0f);
        d.setKnob(DrumEcho::Time, 0.2f);
        std::vector<float> warm(4800, 0.0f);
        run(d, warm);
        std::vector<float> y(48000, 0.0f);
        y[0] = 0.5f;
        y = run(d, y);
        const auto head = std::size_t(std::lround(DrumEcho::headSeconds(0.2f) * kRate));
        const auto near = [&](std::size_t at) {
            float e = 0.0f;
            for (std::size_t i = at - 30; i < at + 30; ++i)
                e = std::max(e, std::fabs(y[i]));
            return e;
        };
        QVERIFY(near(head) > 0.1f);
        QVERIFY(near(2 * head) < 0.01f);
        QVERIFY(near(3 * head) > 0.1f);
    }

    void drumEchoSwellDoesNotRunAway()
    {
        DrumEcho d;
        d.prepare(kRate);
        d.setKnob(DrumEcho::Swell, 1.0f);
        d.setKnob(DrumEcho::Mix, 1.0f);
        d.setKnob(DrumEcho::Tone, 1.0f);
        d.setKnob(DrumEcho::Time, 0.0f);
        const auto y = run(d, sine(48000 * 3, 220.0, 0.8f));
        float peak = 0.0f;
        for (float v : y)
            peak = std::max(peak, std::fabs(v));
        QVERIFY(std::isfinite(peak) && peak < 4.0f);
    }

    void vibeThrobs()
    {
        Vibe v;
        v.prepare(kRate);
        v.setKnob(Vibe::Speed, 0.6f);
        const auto y = run(v, sine(96000, 600.0, 0.1f));
        double lo = 1e9, hi = 0.0;
        for (std::size_t at = 9600; at + 480 < y.size(); at += 480) {
            const double l = level(y, 600.0, at, 480);
            lo = std::min(lo, l);
            hi = std::max(hi, l);
        }
        QVERIFY2(db(hi) - db(lo) > 6.0, qPrintable(QStringLiteral("%1 dB").arg(db(hi) - db(lo))));
        // Vibrato: the swept signal alone keeps its level (an all-pass), no notches.
        Vibe vib;
        vib.prepare(kRate);
        vib.setKnob(Vibe::Mode, 1.0f);
        vib.setKnob(Vibe::Speed, 0.6f);
        const auto z = run(vib, sine(96000, 600.0, 0.1f));
        double vlo = 1e9, vhi = 0.0;
        for (std::size_t at = 9600; at + 480 < z.size(); at += 480) {
            const double l = level(z, 600.0, at, 480);
            vlo = std::min(vlo, l);
            vhi = std::max(vhi, l);
        }
        QVERIFY2(db(vhi) - db(vlo) < 3.0, qPrintable(QStringLiteral("%1 dB").arg(db(vhi) - db(vlo))));
    }

    // --- Round Fuzz ---------------------------------------------------------------------

    void fuzzMoreFuzzMoreDistortion()
    {
        const auto distortion = [](float fuzz) {
            RoundFuzz f;
            f.prepare(kRate);
            f.setKnob(RoundFuzz::Fuzz, fuzz);
            const auto y = run(f, sine(24000, 110.0, 0.1f));
            return thd(y, 110.0, 12000, 9600);
        };
        QVERIFY(distortion(1.0f) > 2.0 * distortion(0.1f));
    }

    // Pick lighter (or roll the guitar back) and it cleans up.
    void fuzzCleansUp()
    {
        const auto distortion = [](float amplitude) {
            RoundFuzz f;
            f.prepare(kRate);
            f.setKnob(RoundFuzz::Fuzz, 0.4f);
            const auto y = run(f, sine(24000, 110.0, amplitude));
            return thd(y, 110.0, 12000, 9600);
        };
        const double soft = distortion(0.003f), hard = distortion(0.3f);
        QVERIFY2(soft < 0.3 * hard, qPrintable(QStringLiteral("%1 vs %2").arg(soft).arg(hard)));
        QVERIFY(soft < 0.1);
    }

    // Asymmetric clipping: strong even harmonics (the second).
    void fuzzHasEvenHarmonics()
    {
        RoundFuzz f;
        f.prepare(kRate);
        const auto y = run(f, sine(24000, 110.0, 0.2f));
        const double h1 = level(y, 110.0, 12000, 9600), h2 = level(y, 220.0, 12000, 9600);
        QVERIFY2(h2 > 0.1 * h1, qPrintable(QStringLiteral("2nd at %1 dB").arg(db(h2 / h1))));
    }

    // Bias off centre: quiet notes get stuck and sputter, much quieter than at the centre.
    void fuzzBiasGates()
    {
        const auto out = [](float bias) {
            RoundFuzz f;
            f.prepare(kRate);
            f.setKnob(RoundFuzz::Bias, bias);
            const auto y = run(f, sine(24000, 110.0, 0.01f));
            return level(y, 110.0, 12000, 9600);
        };
        QVERIFY2(db(out(1.0f)) < db(out(0.5f)) - 10.0, qPrintable(QStringLiteral("%1 vs %2 dB").arg(db(out(1.0f))).arg(db(out(0.5f)))));
    }

    void fuzzSiliconIsBrighter()
    {
        const auto brightness = [](float type) {
            RoundFuzz f;
            f.prepare(kRate);
            f.setKnob(RoundFuzz::Type, type);
            const auto y = run(f, sine(24000, 220.0, 0.2f));
            double high = 0.0;
            for (int k = 10; k <= 20; ++k)
                high += std::pow(level(y, 220.0 * k, 12000, 9600), 2.0);
            return std::sqrt(high) / level(y, 220.0, 12000, 9600);
        };
        QVERIFY(brightness(1.0f) > 1.5 * brightness(0.0f));
    }

    void fuzzIsBoundedAndClean()
    {
        RoundFuzz f;
        f.prepare(kRate);
        f.setKnob(RoundFuzz::Fuzz, 1.0f);
        f.setKnob(RoundFuzz::Volume, 1.0f);
        f.setKnob(RoundFuzz::Type, 1.0f);
        const double hz = 1318.5;
        const auto y = run(f, sine(24000, hz, 0.5f));
        float peak = 0.0f;
        for (float v : y)
            peak = std::max(peak, std::fabs(v));
        QVERIFY(std::isfinite(peak) && peak < 4.0f);
        const std::size_t from = 12000, n = 4096;
        double harmonic = 0.0, other = 0.0;
        for (std::size_t k = 1; k < n / 2; ++k) {
            std::complex<double> sum = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(n - 1));
                sum += w * double(y[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * double(k * i) / double(n));
            }
            const double fr = double(k) * kRate / double(n), nearest = std::round(fr / hz) * hz;
            (std::fabs(fr - nearest) <= 3.0 * kRate / double(n) && nearest > 0.0 ? harmonic : other) += std::norm(sum);
        }
        QVERIFY2(other / (harmonic + other) < 0.02, qPrintable(QString::number(other / (harmonic + other))));
    }

    // --- Compressor ---------------------------------------------------------------------

    void compressorCurve()
    {
        // Quiet signals pass with the make-up gain; louder ones are held back,
        // more so with more sustain; the curve never turns over.
        for (float s : {0.0f, 0.5f, 1.0f}) {
            double previous = -1e9;
            for (double in = -70.0; in <= 0.0; in += 1.0) {
                const double out = in + Compressor::gainDb(in, s);
                QVERIFY(out >= previous - 1e-9);
                previous = out;
            }
        }
        const double spread = [](float s) {
            return (0.0 + Compressor::gainDb(0.0, s)) - (-30.0 + Compressor::gainDb(-30.0, s));
        }(1.0f);
        QVERIFY2(spread < 10.0, qPrintable(QString::number(spread))); // 30 dB in, under 10 out
    }

    void compressorEvensOutLoudAndQuiet()
    {
        const auto out = [](float amplitude, float sustain) {
            auto c = std::make_unique<Compressor>();
            c->prepare(kRate);
            c->setKnob(Compressor::Sustain, sustain);
            const auto y = run(*c, sine(48000, 220.0, amplitude));
            return db(level(y, 220.0, 24000, 9600));
        };
        const double loud = out(0.5f, 0.8f), quiet = out(0.02f, 0.8f);
        QVERIFY2(loud - quiet < 0.5 * (db(0.5) - db(0.02)), qPrintable(QStringLiteral("%1 vs %2").arg(loud).arg(quiet)));
    }

    // A fast attack catches the pick; a slow one lets it through.
    void compressorAttack()
    {
        const auto overshoot = [](float attack) {
            Compressor c;
            c.prepare(kRate);
            c.setKnob(Compressor::Sustain, 1.0f);
            c.setKnob(Compressor::Attack, attack);
            std::vector<float> warm(4800, 0.0f);
            run(c, warm);
            auto y = run(c, sine(9600, 220.0, 0.5f));
            float early = 0.0f, late = 0.0f;
            for (std::size_t i = 0; i < 480; ++i)
                early = std::max(early, std::fabs(y[i]));
            for (std::size_t i = 7200; i < 9600; ++i)
                late = std::max(late, std::fabs(y[i]));
            return early / late;
        };
        QVERIFY(overshoot(1.0f) > 1.5 * overshoot(0.0f));
    }

    void compressorBlendZeroIsDry()
    {
        Compressor c;
        c.prepare(kRate);
        c.setKnob(Compressor::Blend, 0.0f);
        c.setKnob(Compressor::Level, 0.5f); // 0 dB
        const auto x = sine(9600, 220.0, 0.3f), y = run(c, x);
        float diff = 0.0f;
        for (std::size_t i = 4800; i < x.size(); ++i)
            diff = std::max(diff, std::fabs(y[i] - x[i]));
        QVERIFY(diff < 1e-6f);
    }

    // --- Graphic EQs --------------------------------------------------------------------

    void eqFlatIsTransparent()
    {
        Eq10 eq;
        eq.prepare(kRate);
        const auto x = sine(9600, 440.0, 0.3f), y = run(eq, x);
        float diff = 0.0f;
        for (std::size_t i = 4800; i < x.size(); ++i)
            diff = std::max(diff, std::fabs(y[i] - x[i]));
        QVERIFY(diff < 1e-6f);
    }

    void eqBandsLiftTheirOwnFrequency()
    {
        const auto gainAt = [](auto &eq, double hz) {
            const auto y = run(eq, sine(48000, hz, 0.1f));
            return db(level(y, hz, 24000, 19200) / 0.1);
        };
        for (int b = 0; b < 10; ++b) {
            Eq10 eq;
            eq.prepare(kRate);
            eq.setKnob(b, 1.0f); // +12 dB
            const double f = Eq10::kFrequencies[std::size_t(b)];
            QVERIFY2(std::fabs(gainAt(eq, f) - 12.0) < 1.0, qPrintable(QStringLiteral("band %1: %2 dB").arg(f).arg(gainAt(eq, f))));
            const double far = b < 5 ? f * 8.0 : f / 8.0; // three octaves away
            Eq10 again;
            again.prepare(kRate);
            again.setKnob(b, 1.0f);
            QVERIFY2(std::fabs(gainAt(again, far)) < 1.0, qPrintable(QStringLiteral("band %1 at %2: %3 dB").arg(f).arg(far).arg(gainAt(again, far))));
        }
        Eq5 cut;
        cut.prepare(kRate);
        cut.setKnob(2, 0.0f); // 800 Hz, -15 dB
        QVERIFY(std::fabs(gainAt(cut, 800.0) + 15.0) < 1.0);
    }

    void eqLevel()
    {
        Eq5 eq;
        eq.prepare(kRate);
        eq.setKnob(5, 1.0f); // +12 dB
        const auto y = run(eq, sine(9600, 1000.0, 0.1f));
        QVERIFY(std::fabs(db(level(y, 1000.0, 4800, 4800) / 0.1) - 12.0) < 0.2);
    }

    // --- Flanger and phaser ------------------------------------------------------------

    void switchesSnap()
    {
        Flanger f;
        f.setKnob(Flanger::Matrix, 0.7f);
        QCOMPARE(f.knob(Flanger::Matrix), 1.0f);
        f.setKnob(Flanger::Matrix, 0.3f);
        QCOMPARE(f.knob(Flanger::Matrix), 0.0f);
        f.setKnob(Flanger::Range, 0.3f); // a knob stays where it's put
        QCOMPARE(f.knob(Flanger::Range), 0.3f);
    }

    // Filter Matrix: a fixed comb, its first notch where the delay puts it
    // (half a cycle late), its first peak an octave up.
    void flangerMatrixIsAFixedComb()
    {
        const auto at = [](float color, double hz) {
            Flanger f;
            f.prepare(kRate);
            f.setKnob(Flanger::Matrix, 1.0f);
            f.setKnob(Flanger::Range, 0.5f);
            f.setKnob(Flanger::Color, color);
            const auto y = run(f, sine(24000, hz, 0.1f));
            return db(level(y, hz, 12000, 9600) / 0.1);
        };
        const double delay = Flanger::delayMs(1.0f, 0.5) / 1000.0;
        const double notch = 0.5 / delay, peak = 1.0 / delay;
        QVERIFY2(at(0.0f, peak) - at(0.0f, notch) > 20.0, qPrintable(QStringLiteral("%1 vs %2").arg(at(0.0f, peak)).arg(at(0.0f, notch))));
        QVERIFY(at(1.0f, peak) > at(0.0f, peak) + 4.0); // Color: sharper, resonant peaks
    }

    void flangerSweeps()
    {
        Flanger f;
        f.prepare(kRate);
        f.setKnob(Flanger::Rate, 1.0f); // 3 Hz
        const auto y = run(f, sine(48000, 1000.0, 0.1f));
        double lo = 1e9, hi = 0.0;
        for (std::size_t at = 4800; at + 480 < y.size(); at += 480) {
            const double l = level(y, 1000.0, at, 480);
            lo = std::min(lo, l);
            hi = std::max(hi, l);
        }
        QVERIFY2(db(hi) - db(lo) > 15.0, qPrintable(QStringLiteral("%1 dB").arg(db(hi) - db(lo))));
    }

    void flangerFullColorStaysBounded()
    {
        Flanger f;
        f.prepare(kRate);
        f.setKnob(Flanger::Color, 1.0f);
        const auto y = run(f, sine(48000 * 2, 330.0, 0.8f));
        float peak = 0.0f;
        for (float v : y)
            peak = std::max(peak, std::fabs(v));
        QVERIFY(std::isfinite(peak) && peak < 6.0f);
    }

    // Two notches moving through the low mids: at a fixed frequency the
    // level dips and comes back as they pass. DEEP's feedback makes the peaks
    // between the notches stand out (and the notches shallower).
    void phaserNotchesMove()
    {
        struct Swing { double low, high; };
        const auto swing = [](float mode, double hz) {
            Phaser p;
            p.prepare(kRate);
            p.setKnob(Phaser::Speed, 0.2f); // slow (0.24 Hz): each 10 ms window sees the sweep stand still
            p.setKnob(Phaser::Mode, mode);
            const auto y = run(p, sine(48000 * 5, hz, 0.1f));
            Swing s{1e9, 0.0};
            for (std::size_t at = 4800; at + 480 < y.size(); at += 480) {
                const double l = level(y, hz, at, 480) / 0.1;
                s.low = std::min(s.low, l);
                s.high = std::max(s.high, l);
            }
            return s;
        };
        const Swing smooth = swing(0.0f, 500.0), deep = swing(1.0f, 500.0);
        QVERIFY(db(smooth.high) - db(smooth.low) > 15.0);
        QVERIFY(db(deep.high) - db(deep.low) > 10.0);
        QVERIFY(db(deep.high) > db(smooth.high) + 2.0); // resonant peaks
    }

    // --- Tremolo -------------------------------------------------------------------------

    void tremoloCurve()
    {
        for (float shape : {0.0f, 0.5f, 1.0f}) {
            double lo = 1.0, hi = 0.0;
            for (double p = 0.0; p < 1.0; p += 0.001) {
                lo = std::min(lo, Tremolo::gainAt(p, 1.0f, shape));
                hi = std::max(hi, Tremolo::gainAt(p, 1.0f, shape));
            }
            QVERIFY(lo < 1e-6 && std::fabs(hi - 1.0) < 1e-6); // full depth: silence to full
        }
        QCOMPARE(Tremolo::gainAt(0.37, 0.0f, 0.5f), 1.0); // no depth: no change
        // Square spends far more of the cycle near full or near silent than sine does.
        const auto nearEnds = [](float shape) {
            int n = 0;
            for (double p = 0.0; p < 1.0; p += 0.001)
                if (const double g = Tremolo::gainAt(p, 1.0f, shape); g < 0.1 || g > 0.9)
                    ++n;
            return n;
        };
        QVERIFY(nearEnds(1.0f) > 2 * nearEnds(0.0f));
    }

    void tremoloPulsesAtItsRate()
    {
        Tremolo t;
        t.prepare(kRate);
        t.setKnob(Tremolo::Rate, 0.5f);
        t.setKnob(Tremolo::Depth, 1.0f);
        t.setKnob(Tremolo::Level, 0.5f);
        std::vector<float> dc(48000 * 2, 0.5f);
        const auto y = run(t, dc);
        // Count the dips: the gain's minima, one per cycle.
        int dips = 0;
        bool low = false;
        for (std::size_t i = 4800; i < y.size(); ++i) {
            if (!low && y[i] < 0.05f) {
                ++dips;
                low = true;
            } else if (low && y[i] > 0.4f) {
                low = false;
            }
        }
        const double expected = Tremolo::rateHz(0.5f) * (double(y.size() - 4800) / kRate);
        QVERIFY2(std::fabs(dips - expected) <= 1.0, qPrintable(QStringLiteral("%1 dips, %2 expected").arg(dips).arg(expected)));
    }

    // Even the square at full depth and speed has no clicks: its edges are rounded.
    void tremoloSquareDoesNotClick()
    {
        Tremolo t;
        t.prepare(kRate);
        t.setKnob(Tremolo::Rate, 1.0f);
        t.setKnob(Tremolo::Depth, 1.0f);
        t.setKnob(Tremolo::Shape, 1.0f);
        std::vector<float> dc(48000, 0.5f);
        const auto y = run(t, dc);
        float step = 0.0f;
        for (std::size_t i = 4801; i < y.size(); ++i)
            step = std::max(step, std::fabs(y[i] - y[i - 1]));
        QVERIFY2(step < 0.02f, qPrintable(QString::number(step))); // a 0.5 jump spread over ~1.5 ms
    }

    void phaserSpeedRange()
    {
        QVERIFY(std::fabs(Phaser::speedHz(0.0f) - 0.1) < 1e-9);
        QVERIFY(std::fabs(Phaser::speedHz(1.0f) - 8.0) < 1e-9);
    }

    // --- Pedalboard -------------------------------------------------------------------

    void boardAddsRemovesAndReorders()
    {
        PedalChain chain, loop;
        std::uint64_t blocks = 0;
        {
            Pedalboard board(chain, loop);
            QVERIFY(board.add("green_drive", kRate, blocks));
            QVERIFY(board.add("green_drive", kRate, blocks));
            QVERIFY(board.add("green_drive", kRate, blocks, 0)); // in front
            QVERIFY(!board.add("nope", kRate, blocks));
            QCOMPARE(board.count(), 3);
            Pedal *a = &board.pedal(0), *b = &board.pedal(1), *c = &board.pedal(2);
            const auto order = [&] {
                std::vector<Pedal *> v;
                for (int i = 0; i < chain.list()->count; ++i)
                    v.push_back(chain.list()->pedals[std::size_t(i)]);
                return v;
            };
            QCOMPARE(order(), (std::vector<Pedal *>{a, b, c}));
            board.move(0, 2, blocks);
            QCOMPARE(order(), (std::vector<Pedal *>{b, c, a}));
            board.move(2, 1, blocks);
            QCOMPARE(order(), (std::vector<Pedal *>{b, a, c}));
            board.remove(0, blocks);
            QCOMPARE(order(), (std::vector<Pedal *>{a, c}));
            QCOMPARE(&board.pedal(0), a);
            board.collect(blocks += 2); // the audio thread moved on: b can go
            while (board.count() < PedalList::kMaxPedals)
                QVERIFY(board.add("green_drive", kRate, blocks));
            QVERIFY(!board.add("green_drive", kRate, blocks)); // full
            board.clear(blocks);
            QCOMPARE(chain.list()->count, 0);
        }
        QCOMPARE(chain.list(), nullptr); // the board is gone: nothing to play
        QCOMPARE(loop.list(), nullptr);
    }

    // Each pedal plays in front of the amp or in its loop, in the board's order.
    void frontOrLoop()
    {
        PedalChain front, loop;
        Pedalboard board(front, loop);
        board.add("green_drive", kRate, 0); // front, by its kind
        board.add("delay", kRate, 0);       // loop, by its kind
        board.add("chorus", kRate, 0);      // loop
        board.add("reverb", kRate, 0, -1, 0); // asked for the front
        QVERIFY(!board.inLoop(0) && board.inLoop(1) && board.inLoop(2) && !board.inLoop(3));
        const auto list = [](const PedalChain &c) {
            std::vector<std::string_view> v;
            for (int i = 0; i < c.list()->count; ++i)
                v.push_back(c.list()->pedals[std::size_t(i)]->info().type);
            return v;
        };
        QCOMPARE(list(front), (std::vector<std::string_view>{"green_drive", "reverb"}));
        QCOMPARE(list(loop), (std::vector<std::string_view>{"delay", "chorus"}));
        board.setInLoop(0, true, 0);
        board.move(0, 2, 0); // delay, chorus, drive, reverb
        QCOMPARE(list(front), (std::vector<std::string_view>{"reverb"}));
        QCOMPARE(list(loop), (std::vector<std::string_view>{"delay", "chorus", "green_drive"}));
    }

    // --- Time-based pedals --------------------------------------------------------------

    void delayRepeatsAtItsTime()
    {
        Delay d;
        d.prepare(kRate);
        d.setKnob(Delay::Time, 0.5f);
        d.setKnob(Delay::Feedback, 0.5f);
        d.setKnob(Delay::Mix, 1.0f);
        d.setKnob(Delay::Tone, 1.0f);
        std::vector<float> x(48000 * 2, 0.0f);
        for (int i = 0; i < 480; ++i) // let it fade in, then a click
            d.process(x.data(), 1);
        std::vector<float> y(48000 * 2, 0.0f);
        y[0] = 0.5f;
        for (std::size_t at = 0; at < y.size(); at += 128)
            d.process(y.data() + at, std::uint32_t(std::min<std::size_t>(128, y.size() - at)));
        const double seconds = 0.04 * std::pow(Delay::kMaxSeconds / 0.04, 0.5);
        const auto first = std::size_t(std::lround(seconds * kRate)), second = 2 * first;
        // The click comes back spread over a couple of samples (the time
        // falls between them): its sum, near where it should be.
        const auto echoNear = [&](std::size_t at) {
            float sum = 0.0f;
            for (std::size_t i = at - 20; i < at + 20; ++i)
                sum += y[i];
            return sum;
        };
        QVERIFY(echoNear(first) > 0.4f);                // the echo (of tanh(0.5)), at its time
        QVERIFY(echoNear(second) < echoNear(first));    // each repeat quieter
        QVERIFY(echoNear(second) > 0.05f);
        QVERIFY(std::fabs(echoNear(first / 2)) < 0.01f); // nothing in between
    }

    void delayDoesNotRunAway()
    {
        Delay d;
        d.prepare(kRate);
        d.setKnob(Delay::Time, 0.1f);
        d.setKnob(Delay::Feedback, 1.0f);
        d.setKnob(Delay::Mix, 1.0f);
        d.setKnob(Delay::Tone, 1.0f);
        const auto y = run(d, sine(48000 * 3, 220.0, 0.8f));
        float peak = 0.0f;
        for (float v : y)
            peak = std::max(peak, std::fabs(v));
        QVERIFY(std::isfinite(peak) && peak < 2.0f);
    }

    void chorusSweepsAndMixZeroIsDry()
    {
        Chorus c;
        c.prepare(kRate);
        c.setKnob(Chorus::Rate, 1.0f);
        c.setKnob(Chorus::Depth, 1.0f);
        const auto x = sine(48000, 440.0, 0.3f);
        const auto y = run(c, x);
        // The sweep moves the copy against the dry: the level wobbles over time.
        double lo = 1e9, hi = 0.0;
        for (std::size_t at = 4800; at + 480 < y.size(); at += 480) {
            const double l = level(y, 440.0, at, 480);
            lo = std::min(lo, l);
            hi = std::max(hi, l);
        }
        QVERIFY2(db(hi) - db(lo) > 3.0, qPrintable(QStringLiteral("%1 dB").arg(db(hi) - db(lo))));
        Chorus dry;
        dry.prepare(kRate);
        dry.setKnob(Chorus::Mix, 0.0f);
        const auto z = run(dry, x);
        float diff = 0.0f;
        for (std::size_t i = 4800; i < x.size(); ++i) // after the fade-in
            diff = std::max(diff, std::fabs(z[i] - x[i]));
        QVERIFY(diff < 1e-6f);
    }

    void reverbTailDecays()
    {
        Reverb r;
        r.prepare(kRate);
        r.setKnob(Reverb::Decay, 0.3f);
        r.setKnob(Reverb::Mix, 1.0f);
        std::vector<float> warm(4800, 0.0f);
        run(r, warm); // past the fade-in
        std::vector<float> y(48000 * 3, 0.0f);
        y[0] = 0.5f;
        y = run(r, y);
        const auto rmsAt = [&](std::size_t from) {
            double s = 0.0;
            for (std::size_t i = from; i < from + 4800; ++i)
                s += double(y[i]) * y[i];
            return std::sqrt(s / 4800.0);
        };
        QVERIFY(rmsAt(4800) > 1e-4);               // a tail after the click
        QVERIFY(rmsAt(48000 * 2) < 0.1 * rmsAt(4800)); // dying away
        Reverb dry;
        dry.prepare(kRate);
        dry.setKnob(Reverb::Mix, 0.0f);
        const auto x = sine(9600, 220.0, 0.3f), z = run(dry, x);
        float diff = 0.0f;
        for (std::size_t i = 4800; i < x.size(); ++i)
            diff = std::max(diff, std::fabs(z[i] - x[i]));
        QVERIFY(diff < 1e-6f);
    }

    // The chain plays the pedals in the board's order: the same as running
    // them one after the other by hand.
    void chainPlaysInOrder()
    {
        const auto setUp = [](Pedal &p, float d, float lvl) {
            p.setKnob(GreenDrive::Drive, d);
            p.setKnob(GreenDrive::Level, lvl);
        };
        const auto byHand = [&](float d1, float l1, float d2, float l2, std::vector<float> x) {
            auto first = drive(d1, 0.5f, l1), second = drive(d2, 0.5f, l2);
            for (std::size_t at = 0; at < x.size(); at += 128) {
                first->process(x.data() + at, 128);
                second->process(x.data() + at, 128);
            }
            return x;
        };
        const auto x = sine(9600, 330.0, 0.3f);
        for (bool swapped : {false, true}) {
            PedalChain chain, loop;
            Pedalboard board(chain, loop);
            board.add("green_drive", kRate, 0);
            board.add("green_drive", kRate, 0);
            setUp(board.pedal(0), 0.1f, 0.2f);
            setUp(board.pedal(1), 0.9f, 0.7f);
            if (swapped)
                board.move(0, 1, 0);
            auto y = x;
            for (std::size_t at = 0; at < y.size(); at += 128)
                chain.process(y.data() + at, 128);
            const auto expected = swapped ? byHand(0.9f, 0.7f, 0.1f, 0.2f, x) : byHand(0.1f, 0.2f, 0.9f, 0.7f, x);
            QCOMPARE(y, expected);
        }
    }
};

QTEST_APPLESS_MAIN(TestPedals)
#include "tst_pedals.moc"
