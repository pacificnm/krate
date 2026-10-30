#include <QtTest>

#include <QTemporaryDir>

#include "RackRunControl.h"
#include "SequencerController.h"
#include "core/Transport.h"
#include "rack/Rack.h"

using namespace krate;
using namespace krate::seq;
using namespace Qt::StringLiterals;

// The SEQ screen's controller (docs/SEQUENCER.md): edits reach the player at
// once, targets resolve to instruments, and sequencer.json keeps it all.
class TestSeqController : public QObject
{
    Q_OBJECT

    static std::vector<NoteTarget> targets()
    {
        return {{QStringLiteral("k303"), QStringLiteral("k303"), 0, 0, false},
                {QStringLiteral("K909"), QStringLiteral("k909"), 1, 9, true},
                {QStringLiteral("Krimson B"), QStringLiteral("krimson:B"), 3, 3, false}};
    }

    // The first note-on the player sends within a beat of play.
    static std::optional<DeviceEvent> firstOn(Sequencer &seq)
    {
        std::array<DeviceEvent, 64> out{};
        for (std::uint64_t at = 0; at < 24000; at += 64) {
            TransportState t;
            t.playing = true;
            t.restarted = at == 0;
            t.beatAtBlockStart = double(at) / 24000.0;
            const std::uint32_t n = seq.render(t, 64, 48000.0, out.data(), std::uint32_t(out.size()));
            for (std::uint32_t i = 0; i < n; ++i)
                if ((out[i].event.data[0] & 0xF0) == 0x90)
                    return out[i];
        }
        return std::nullopt;
    }

private slots:
    void editsPlayAndPersist()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sequencer.json"));
        {
            Sequencer seq;
            SequencerController c(seq, targets(), path);
            c.load();
            QCOMPARE(c.targetChoices(), (QStringList{"None", "k303", "K909", "Krimson B"}));
            c.setTrack(2);
            c.setTarget(2, 3); // Krimson B
            QCOMPARE(c.addNote(48, 64, 24, 90), 0);
            c.setTarget(5, 2); // K909
            QVERIFY(c.tracks()[5].toMap().value(u"drums"_s).toBool());

            const auto on = firstOn(seq);
            QVERIFY(on);
            QCOMPARE(on->device, 3);
            QCOMPARE(on->event.data[0], std::uint8_t(0x93));
            QCOMPARE(on->event.data[1], std::uint8_t(64));
            QCOMPARE(on->event.frame, std::uint32_t(12000 % 64)); // step 3 at 120 BPM: frame 12000

            c.moveNote(0, 96, 67);
            c.resizeNote(0, 48);
            c.setVelocity(0, 200); // clamped
            const QVariantMap note = c.notes()[0].toMap();
            QCOMPARE(note.value(u"start"_s).toInt(), 96);
            QCOMPARE(note.value(u"pitch"_s).toInt(), 67);
            QCOMPARE(note.value(u"length"_s).toInt(), 48);
            QCOMPARE(note.value(u"velocity"_s).toInt(), 127);

            c.setGrid(2);       // 1/16T
            c.setLength(1000);  // rounded to the grid: 62.5 steps -> 63 x 16 = 1008
            QCOMPARE(c.length(), 1008);
            c.setLength(99999); // at most 4 bars
            QCOMPARE(c.length(), kMaxTicks);
            c.setMute(2, true);
            c.setPattern(4);
            QCOMPARE(c.notes().size(), 0);
            c.setPattern(0);
            c.copyPattern();
            c.setPattern(7);
            c.pastePattern();
            QCOMPARE(c.notes().size(), 1);
            QVERIFY(c.patternsUsed()[7].toBool());
        } // the destructor writes what's waiting

        Sequencer seq;
        SequencerController c(seq, targets(), path);
        c.load();
        QCOMPARE(c.pattern(), 7);
        QCOMPARE(c.tracks()[2].toMap().value(u"choice"_s).toInt(), 3);
        QVERIFY(c.tracks()[2].toMap().value(u"mute"_s).toBool());
        QCOMPARE(c.state().bank->patterns[0].length, std::uint16_t(kMaxTicks));
        QCOMPARE(c.state().bank->patterns[7].tracks[2].count, std::uint16_t(1));
        c.setTrack(2);
        c.deleteNote(0);
        QCOMPARE(c.notes().size(), 0);
    }

    // Bars from a song: each chunk replaces one track's notes in a pattern (the
    // other tracks stay), sets the pattern's length and grid, and stops at pattern 16.
    void importChunks()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.setPattern(14);
        c.setTrack(1);
        c.addNote(0, 40, 24, 100);  // track 1: stays
        c.setTrack(0);
        c.addNote(96, 30, 24, 100); // track 0: replaced
        QCOMPARE(c.patternsWithNotes(14, 3, 0), (QList<int>{14}));

        std::vector<songimport::Chunk> chunks(3);
        for (auto &chunk : chunks) {
            chunk.length = 8 * kTicksPerBeat;
            chunk.grid = 2;
            chunk.notes = {{0, 16, 36, 110}, {48, 16, 38, 90}};
        }
        QCOMPARE(c.importChunks(14, 0, chunks, 2), 2); // patterns 15 and 16; the third doesn't fit
        const Pattern &p = c.state().bank->patterns[14];
        QCOMPARE(p.length, std::uint16_t(8 * kTicksPerBeat));
        QCOMPARE(p.grid, std::uint8_t(2));
        QCOMPARE(p.tracks[0].count, std::uint16_t(2));
        QCOMPARE(p.tracks[0].notes[0].pitch, std::uint8_t(36));
        QCOMPARE(p.tracks[1].count, std::uint16_t(1));
        QCOMPARE(c.state().bank->patterns[15].tracks[0].count, std::uint16_t(2));
        QCOMPARE(c.tracks()[0].toMap().value(u"choice"_s).toInt(), 2); // K909
        QCOMPARE(c.pattern(), 14);
        QCOMPARE(c.track(), 0);
    }

    // The chain: add (at the end or after a step), repeats clamped, remove; the
    // pattern buttons wrap.
    void chainEdits()
    {
        QTemporaryDir dir;
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.load();
        c.addToChain(2);
        c.addToChain(5);
        c.addToChain(9, 0); // after the first
        c.setChainRepeats(1, 99);
        const auto steps = c.chain();
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps[0].toMap().value(u"pattern"_s).toInt(), 2);
        QCOMPARE(steps[1].toMap().value(u"pattern"_s).toInt(), 9);
        QCOMPARE(steps[1].toMap().value(u"repeats"_s).toInt(), kMaxRepeats);
        QCOMPARE(steps[2].toMap().value(u"pattern"_s).toInt(), 5);
        c.removeChainStep(0);
        QCOMPARE(c.chain().size(), 2);
        c.setChainMode(true);
        QVERIFY(seq.chainMode());
        // One play mode at a time; the range stays in order.
        c.setPlayMode(1);
        QVERIFY(seq.rangeMode() && !seq.chainMode());
        QVERIFY(!c.chainMode());
        c.setRangeTo(5);
        c.setRangeFrom(8); // past TO: TO follows
        QCOMPARE(c.rangeFrom(), 8);
        QCOMPARE(c.rangeTo(), 8);
        c.setRangeTo(2);   // before FROM: FROM follows
        QCOMPARE(c.rangeFrom(), 2);
        c.setPlayMode(2);
        QVERIFY(seq.chainMode() && !seq.rangeMode());
        c.setPattern(0);
        c.stepPattern(-1);
        QCOMPARE(c.pattern(), kPatterns - 1);
        c.stepPattern(1);
        QCOMPARE(c.pattern(), 0);
        c.clearChain();
        QCOMPARE(c.chain().size(), 0);
    }

    // RUN works like an instrument's RUN / STOP: from silence it starts the
    // rack; pressed again it stops it; off while the rack plays, it joins.
    void runStartsAndStops()
    {
        QTemporaryDir dir;
        krate::Rack rack(64);
        krate::Transport transport;
        RackRunControl run(rack, transport, {});
        Sequencer seq;
        SequencerController c(seq, targets(), dir.filePath(QStringLiteral("sequencer.json")));
        c.setRunControl(&run);
        c.load();

        QVERIFY(!transport.playing());
        c.toggleRun();
        QVERIFY(transport.playing());
        QVERIFY(c.running());
        c.toggleRun();
        QVERIFY(!c.running());
        QVERIFY(!transport.playing()); // nothing else was running

        transport.setPlaying(true); // the rack's PLAY, the sequencer off
        c.toggleRun();
        QVERIFY(c.running());
        QVERIFY(transport.playing());
    }
};

QTEST_GUILESS_MAIN(TestSeqController)
#include "tst_seqcontroller.moc"
