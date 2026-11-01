#include <QtTest>

#include <QJsonArray>

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
        state.targets[0] = QStringLiteral("korrode");
        state.targets[3] = QStringLiteral("krimson:B");
        state.mutes[3] = true;
        Pattern &p = state.bank->patterns[5];
        p.length = 16 * kTicksPerBeat;
        p.grid = 2;
        p.tracks[3].add({0, 16, 60, 90});
        p.tracks[3].add({1520, 64, 127, 1});
        state.chainMode = true;
        state.rangeFrom = 2;
        state.rangeTo = 9;
        state.bank->chain[0] = {5, 2};
        state.bank->chain[1] = {0, 16};
        state.bank->chainLength = 2;

        const SequencerState back = sequencerFromJson(sequencerToJson(state));
        QCOMPARE(back.pattern, 5);
        QCOMPARE(back.running, false);
        QCOMPARE(back.targets[0], QStringLiteral("korrode"));
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
        QVERIFY(!back.rangeMode);
        QCOMPARE(back.rangeFrom, 2);
        QCOMPARE(back.rangeTo, 9);
        QCOMPARE(back.bank->chainLength, std::uint8_t(2));
        QVERIFY(back.bank->chain[0] == state.bank->chain[0]);
        QVERIFY(back.bank->chain[1] == state.bank->chain[1]);
        QCOMPARE(back.bank->patterns[0].length, std::uint16_t(4 * kTicksPerBeat));
    }

    // The arranger's part (docs/DAW.md, D3): back as it was; bad clips (out
    // of range, overlapping, of no kind) left out; none in older files.
    void arrangement()
    {
        SequencerState state;
        state.songMode = true;
        state.startBar = 8;
        state.loopOn = true;
        state.loopFrom = 4;
        state.loopTo = 12;
        state.punchFrom = 6;
        state.punchTo = 7;
        state.arrangement.midi.push_back({{{0, 48, 60, 90}, {200000, 96, 64, 100}}});
        state.arrangement.lanes[0] = {{0, 4, 2, -1}, {4, 2, -1, 0}};
        state.arrangement.lanes[7] = {{10, 1, 15, -1}};
        state.automation = {{QStringLiteral("kalliope"), QStringLiteral("cutoff"), {{0.0, 0.25f}, {8.5, 1.0f}}}};
        const SequencerState back = sequencerFromJson(sequencerToJson(state));
        QVERIFY(back.arrangement == state.arrangement);
        QVERIFY(back.automation == state.automation);
        QVERIFY(back.songMode);
        QCOMPARE(back.startBar, 8);
        QVERIFY(back.loopOn);
        QCOMPARE(back.loopFrom, 4);
        QCOMPARE(back.loopTo, 12);
        QVERIFY(!back.punchOn);
        QCOMPARE(back.punchFrom, 6);
        QCOMPARE(back.punchTo, 7);

        QJsonObject json = sequencerToJson(state);
        QJsonObject arrange = json["arrange"].toObject();
        QJsonArray lanes = arrange["lanes"].toArray();
        lanes[1] = QJsonArray{QJsonArray{0, 2, 0, -1}, QJsonArray{1, 1, 3, -1}, // overlaps the first
                              QJsonArray{3, 1, 16, -1}, QJsonArray{4, 1, -1, 5}, // no such pattern, no such clip
                              QJsonArray{5, 1, -1, -1}, QJsonArray{6, 0, 1, -1}, QJsonArray{7, 1, 1, -1}};
        arrange["lanes"] = lanes;
        json["arrange"] = arrange;
        const SequencerState bad = sequencerFromJson(json);
        QCOMPARE(bad.arrangement.lanes[1].size(), std::size_t(2));
        QCOMPARE(bad.arrangement.lanes[1][1].startBar, 7);

        json.remove("arrange");
        const SequencerState old = sequencerFromJson(json);
        QCOMPARE(old.arrangement.endBar(), 0);
        QVERIFY(!old.songMode);
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
