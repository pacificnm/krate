#include <QtTest>

#include <cmath>
#include <vector>

#include "TestHost.h"
#include "dsp/Klick.h"

using namespace klick;

namespace {

// Peak level in [from, to) of a rendered buffer.
float peakIn(const std::vector<float> &s, long from, long to)
{
    float p = 0.0f;
    for (long i = std::max(0L, from); i < std::min(long(s.size()), to); ++i)
        p = std::max(p, std::abs(s[std::size_t(i)]));
    return p;
}

// Render `seconds` of playing transport in 64-frame blocks.
std::vector<float> renderPlaying(Klick &k, double seconds, double bpm = 120.0, double rate = 48000.0)
{
    TestHost host(k, rate, bpm);
    host.play();
    std::vector<float> all;
    const long blocks = long(seconds * rate / 64.0);
    for (long b = 0; b < blocks; ++b) {
        host.run(64);
        all.insert(all.end(), host.left.begin(), host.left.end());
    }
    return all;
}

} // namespace

class TestKlick : public QObject
{
    Q_OBJECT

private slots:
    void silentWhileStopped()
    {
        Klick k;
        TestHost host(k);
        for (int i = 0; i < 100; ++i) {
            host.run(64);
            QVERIFY(peakIn(host.left, 0, 64) == 0.0f);
        }
    }

    // A click starts on each beat's sample and is gone well before the next.
    void clicksLandOnTheBeat_data()
    {
        QTest::addColumn<double>("bpm");
        QTest::newRow("120") << 120.0;
        QTest::newRow("133") << 133.0;
    }

    void clicksLandOnTheBeat()
    {
        QFETCH(double, bpm);
        Klick k;
        const auto s = renderPlaying(k, 4.0, bpm);
        const double samplesPerBeat = 48000.0 * 60.0 / bpm;
        for (int beat = 1; beat < 7; ++beat) {
            const long at = long(std::ceil(beat * samplesPerBeat));
            QVERIFY2(peakIn(s, at, at + 480) > 0.1f, qPrintable(QString("no click at beat %1").arg(beat)));
            QVERIFY2(peakIn(s, at - 2000, at) < 0.001f, qPrintable(QString("noise before beat %1").arg(beat)));
            QVERIFY(std::abs(s[std::size_t(at - 1)]) < 0.001f); // nothing until the beat's own sample
        }
    }

    void firstBeatOfTheBarIsAccented()
    {
        Klick k; // 4 beats per bar by default
        const auto s = renderPlaying(k, 4.0);
        std::vector<float> peaks;
        for (int beat = 0; beat < 8; ++beat)
            peaks.push_back(peakIn(s, beat * 24000L, beat * 24000L + 480));
        for (int beat = 0; beat < 8; ++beat) {
            if (beat % 4 == 0)
                QVERIFY(peaks[std::size_t(beat)] > 1.2f * peaks[1]);
            else
                QVERIFY(std::abs(peaks[std::size_t(beat)] - peaks[1]) < 0.02f);
        }
    }

    void beatsPerBarChangesTheAccent()
    {
        Klick k;
        k.setParam(int(ParamId::Beats), 0.2f); // 3 beats per bar
        QCOMPARE(units::beatsPerBar(k.param(int(ParamId::Beats))), 3);
        const auto s = renderPlaying(k, 4.0);
        const float plain = peakIn(s, 24000, 24480);
        QVERIFY(peakIn(s, 3 * 24000L, 3 * 24000L + 480) > 1.2f * plain);
        QVERIFY(peakIn(s, 4 * 24000L, 4 * 24000L + 480) < 1.05f * plain);
    }

    // Beat 1 at 120 BPM / 48 kHz is sample 24000, a block boundary (375 x 64):
    // exactly one click there, not one per block.
    void oneClickPerBeatAcrossBlocks()
    {
        Klick k;
        const auto s = renderPlaying(k, 1.0);
        // An onset: a sample above 1e-3 after 100 quiet samples (a lone sample
        // near a zero crossing inside a click doesn't count).
        int onsets = 0;
        for (long i = 0; i < long(s.size()); ++i) // before sample 0 counts as quiet
            if (std::abs(s[std::size_t(i)]) > 1e-3f && peakIn(s, i - 100, i) < 1e-4f)
                ++onsets;
        QCOMPARE(onsets, 2); // beats 0 and 1 in one second
    }

    void restartClicksAgainAtOnce()
    {
        Klick k;
        TestHost host(k);
        host.play();
        for (int i = 0; i < 200; ++i)
            host.run(64);
        host.play(false);
        for (int i = 0; i < 200; ++i)
            host.run(64);
        QVERIFY(peakIn(host.left, 0, 64) < 1e-6f); // stopped and decayed
        host.play();
        host.run(64);
        QVERIFY(peakIn(host.left, 0, 64) > 0.1f);  // beat 0 of the new start
    }

    void levelZeroIsSilent()
    {
        Klick k;
        k.setParam(int(ParamId::Level), 0.0f);
        const auto s = renderPlaying(k, 2.0);
        QCOMPARE(peakIn(s, 0, long(s.size())), 0.0f);
    }

    void hostSetParamsAreFlagged()
    {
        Klick k;
        k.setParam(int(ParamId::Pitch), 0.9f);
        QCOMPARE(k.takeParamsChangedByAudio(), std::uint64_t(0));
        k.setParamFromHost(int(ParamId::Level), 0.25f);
        QCOMPARE(k.param(int(ParamId::Level)), 0.25f);
        QCOMPARE(k.takeParamsChangedByAudio(), std::uint64_t(1) << int(ParamId::Level));
        QCOMPARE(k.takeParamsChangedByAudio(), std::uint64_t(0));
    }

    void displayText()
    {
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Beats)].format(0.4f)), QStringLiteral("4/4"));
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Beats)].format(1.0f)), QStringLiteral("7/4"));
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Pitch)].format(0.0f)), QStringLiteral("600 Hz"));
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Level)].format(1.0f)), QStringLiteral("0.0 dB"));
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Level)].format(0.0f)), QStringLiteral("-inf dB"));
        QCOMPARE(QString::fromStdString(kParams[int(ParamId::Accent)].format(0.6f)), QStringLiteral("60%"));
    }
};

QTEST_APPLESS_MAIN(TestKlick)
#include "tst_klick.moc"
