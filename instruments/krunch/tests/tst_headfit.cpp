#include <QtTest>

#include <cmath>
#include <thread>

#include "HeadFit.h"
#include "SyntheticGuitar.h"
#include "app/HeadFile.h"

using namespace krunch;

// Fitting heads to captures (docs/HEADS.md, krunch-fit): the comparison, the
// search space, the search, and a fit finding its way to a known amp.
class TestHeadFit : public QObject
{
    Q_OBJECT

    static fit::Material material()
    {
        return fit::buildMaterial({tools::SyntheticGuitar(fit::kRate).riff()}, 4, 0.5, 0.2);
    }
    static std::vector<fit::Features> features(const fit::Material &m, const HeadDesign &head)
    {
        std::vector<fit::Features> f;
        for (const fit::Window &w : m.windows)
            f.push_back(fit::analyse(fit::renderHead(head, w, m.leadIn), w.toneHz));
        return f;
    }

private slots:
    // The same amp scores nothing apart; a louder copy only its level.
    void sameAmpScoresZero()
    {
        const fit::Material m = material();
        QCOMPARE(int(m.windows.size()), 4 + 9); // the guitar, soft to hard, and the tones
        const HeadDesign head = explicitHead(Voice::Crunch, PreTube::HighGain, PowerTube::BritishPentode);
        const auto a = features(m, head);
        const fit::Score same = fit::compare(a, a);
        QCOMPARE(same.total, 0.0);
        HeadDesign louder = head;
        louder.outputDb = 6.0;
        const fit::Score level = fit::compare(a, features(m, louder));
        QVERIFY2(std::fabs(level.levelDb + 6.0) < 0.05, qPrintable(QString::number(level.levelDb)));
        QVERIFY2(level.total < 0.05, qPrintable(QString::number(level.total))); // the level isn't the sound
        HeadDesign other = head;
        other.stage[1].cathodeOhms = 1500.0; // its clipper bypassed: another amp
        QVERIFY(fit::compare(a, features(m, other)).total > 1.0);
    }

    // Each of the head's insides maps to 0..1 and back.
    void spaceRoundTrip()
    {
        const HeadDesign head = explicitHead(Voice::Heavy, PreTube::Smooth, PowerTube::AmericanBeam);
        const fit::Space space(head);
        QVERIFY(space.dims() > 30);
        const std::vector<double> x = space.encode(head);
        for (double v : x)
            QVERIFY(v >= 0.0 && v <= 1.0);
        const HeadDesign back = space.decode(x);
        QCOMPARE(back.stages, head.stages);
        QVERIFY(!back.channelTube && !back.channelPower);
        QCOMPARE(back.tube[2], PreTube::Smooth);
        QVERIFY(std::fabs(back.stage[2].cathodeOhms - head.stage[2].cathodeOhms) < 1e-6);
        QVERIFY(std::fabs(back.power.headroom - head.power.headroom) < 1e-9);
        QVERIFY(std::fabs(space.encode(back)[5] - x[5]) < 1e-12);
    }

    // The search finds a known minimum.
    void searchFindsTheMinimum()
    {
        const std::vector<double> target{0.2, 0.8, 0.5, 0.35, 0.9, 0.1};
        const auto f = [&](const std::vector<std::vector<double>> &xs) {
            std::vector<double> out;
            for (const auto &x : xs) {
                double d = 0.0;
                for (std::size_t i = 0; i < x.size(); ++i)
                    d += (x[i] - target[i]) * (x[i] - target[i]) * double(i + 1);
                out.push_back(d);
            }
            return out;
        };
        std::mt19937 rng(7);
        const fit::SearchResult r = fit::search(f, std::vector<double>(6, 0.5), 0.2, 3000, rng);
        QVERIFY2(r.bestValue < 1e-4, qPrintable(QString::number(r.bestValue)));
        QVERIFY(r.evaluations <= 3000);
    }

    // A fit from the built-in it was made from closes most of the gap to an
    // amp built a little otherwise.
    void fitsAKnownAmp()
    {
        const fit::Material m = material();
        const HeadDesign start = explicitHead(Voice::Crunch, PreTube::HighGain, PowerTube::BritishPentode);
        HeadDesign target = start;
        target.trimDb += 5.0;
        target.power.voiceDb = 3.0;
        target.stage[0].bypassHz = 120.0;
        const auto reference = features(m, target);
        const fit::Space space(start);
        const auto evaluate = [&](const std::vector<std::vector<double>> &xs) {
            std::vector<double> out(xs.size());
            fit::parallelFor(int(xs.size()), int(std::max(1u, std::thread::hardware_concurrency())),
                             [&](int i) { out[std::size_t(i)] = fit::compare(reference, features(m, space.decode(xs[std::size_t(i)]))).total; });
            return out;
        };
        const double before = evaluate({space.encode(start)}).front();
        std::mt19937 rng(3);
        const fit::SearchResult r = fit::search(evaluate, space.encode(start), 0.05, 400, rng);
        QVERIFY2(r.bestValue < 0.5 * before, qPrintable(QStringLiteral("%1 -> %2").arg(before).arg(r.bestValue)));
    }
};

QTEST_GUILESS_MAIN(TestHeadFit)
#include "tst_headfit.moc"
