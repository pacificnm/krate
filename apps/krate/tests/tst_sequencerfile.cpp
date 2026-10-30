#include <QtTest>

#include "SequencerFile.h"

using namespace krate::seq;

// sequencer.json (docs/SEQUENCER.md): a round trip keeps everything; bad
// values fall back to the defaults.
class TestSequencerFile : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        SequencerState state;
        state.pattern = 5;
        state.running = false;
        state.targets[0] = QStringLiteral("k303");
        state.targets[3] = QStringLiteral("krimson:B");
        state.mutes[3] = true;
        Pattern &p = state.bank->patterns[5];
        p.length = 16 * kTicksPerBeat;
        p.grid = 2;
        p.tracks[3].add({0, 16, 60, 90});
        p.tracks[3].add({1520, 64, 127, 1});
        state.chainMode = true;
        state.bank->chain[0] = {5, 2};
        state.bank->chain[1] = {0, 16};
        state.bank->chainLength = 2;

        const SequencerState back = sequencerFromJson(sequencerToJson(state));
        QCOMPARE(back.pattern, 5);
        QCOMPARE(back.running, false);
        QCOMPARE(back.targets[0], QStringLiteral("k303"));
        QCOMPARE(back.targets[3], QStringLiteral("krimson:B"));
        QCOMPARE(back.targets[1], QString());
        QVERIFY(back.mutes[3] && !back.mutes[0]);
        const Pattern &q = back.bank->patterns[5];
        QCOMPARE(q.length, p.length);
        QCOMPARE(q.grid, p.grid);
        QCOMPARE(q.tracks[3].count, std::uint16_t(2));
        QVERIFY(q.tracks[3].notes[0] == p.tracks[3].notes[0]);
        QVERIFY(q.tracks[3].notes[1] == p.tracks[3].notes[1]);
        QVERIFY(back.bank->patterns[0].empty());
        QVERIFY(back.chainMode);
        QCOMPARE(back.bank->chainLength, std::uint8_t(2));
        QVERIFY(back.bank->chain[0] == state.bank->chain[0]);
        QVERIFY(back.bank->chain[1] == state.bank->chain[1]);
        QCOMPARE(back.bank->patterns[0].length, std::uint16_t(4 * kTicksPerBeat));
    }

    void badValuesFallBack()
    {
        const QJsonObject json = QJsonDocument::fromJson(R"({"version":1,"pattern":99,
            "patterns":[{"length":99999,"grid":9,"tracks":[[[0,24,60,100],[-1,24,60,100],[0,0,60,100],[0,24,200,100],[0,24,61,0],[1]]]}]})")
                                     .object();
        const SequencerState s = sequencerFromJson(json);
        QCOMPARE(s.pattern, kPatterns - 1);
        QCOMPARE(s.bank->patterns[0].length, std::uint16_t(kMaxTicks));
        QCOMPARE(s.bank->patterns[0].grid, std::uint8_t(int(kGridTicks.size()) - 1));
        QCOMPARE(s.bank->patterns[0].tracks[0].count, std::uint16_t(2)); // the good note, and the velocity clamped to 1
        QCOMPARE(s.bank->patterns[0].tracks[0].notes[1].velocity, std::uint8_t(1));
    }
};

QTEST_GUILESS_MAIN(TestSequencerFile)
#include "tst_sequencerfile.moc"
