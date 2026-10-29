#include <QtTest>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include "dsp/GreenDrive.h"
#include "dsp/Pedal.h"

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
        QCOMPARE(pedalTypes().size(), std::size_t(1));
        QCOMPARE(pedalTypes()[0]->type, std::string_view("green_drive"));
        QVERIFY(makePedal("green_drive"));
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

    // --- Pedalboard -------------------------------------------------------------------

    void boardAddsRemovesAndReorders()
    {
        PedalChain chain;
        std::uint64_t blocks = 0;
        {
            Pedalboard board(chain);
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
            PedalChain chain;
            Pedalboard board(chain);
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
