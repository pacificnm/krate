#include <QtTest>

#include "SongImport.h"
#include "song/Arrange.h"

using namespace krate::song;
using namespace krate::seq;
using namespace songimport;

// Copying a song's bars into the sequencer (docs/SEQUENCER.md, Q3): bars into
// patterns of up to 16 beats, notes fitted to the grid that moves fewest,
// drums mapped, Korrode kept to one note, repeats taken from the first time through.
class TestSongImport : public QObject
{
    Q_OBJECT

    static Event on(Tick t, int ch, int key, int vel = 100) { return {t, std::uint8_t(0x90 | ch), std::uint8_t(key), std::uint8_t(vel)}; }
    static Event off(Tick t, int ch, int key) { return {t, std::uint8_t(0x80 | ch), std::uint8_t(key), 0}; }

    // 480 ppq, 4/4 (or the given signature), one track with `events`.
    static Song song(std::vector<Event> events, Tick length, int numerator = 4, bool drums = false)
    {
        Song s;
        s.tempo = TempoMap(480);
        s.tempo.addTempo(0, 120.0);
        s.tempo.addTimeSignature(0, numerator, 4);
        Track t;
        t.drums = drums;
        std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.tick < b.tick; });
        t.events = std::move(events);
        s.tracks = {t};
        s.length = length;
        return s;
    }

private slots:
    // Six bars of 16ths: two patterns (4 bars, then 2), grid 1/16, nothing moved.
    void barsIntoPatterns()
    {
        std::vector<Event> ev;
        for (Tick t = 0; t < 6 * 1920; t += 120) {
            ev.push_back(on(t, 0, 40));
            ev.push_back(off(t + 120, 0, 40));
        }
        const Song s = song(ev, 6 * 1920);
        QCOMPARE(barCount(s, nullptr), 6);
        const auto spans = barSpans(s, nullptr, 0, 5);
        QCOMPARE(spans.size(), std::size_t(6));
        const Result r = convert(s, 0, spans, {});
        QCOMPARE(r.chunks.size(), std::size_t(2));
        QCOMPARE(r.chunks[0].barCount, 4);
        QCOMPARE(r.chunks[0].length, std::uint16_t(16 * kTicksPerBeat));
        QCOMPARE(r.chunks[1].length, std::uint16_t(8 * kTicksPerBeat));
        QCOMPARE(r.chunks[0].grid, std::uint8_t(0));
        QCOMPARE(r.chunks[0].notes.size(), std::size_t(64));
        QCOMPARE(r.chunks[0].notes[1].start, std::uint16_t(24));
        QCOMPARE(r.chunks[0].notes[1].length, std::uint16_t(24));
        QCOMPARE(r.report.snapped, 0);
        QCOMPARE(r.report.notes, 96);
    }

    // 16th triplets pick the triplet grid; a slightly late note is snapped (and counted).
    void gridAndSnap()
    {
        std::vector<Event> ev;
        for (Tick t = 0; t < 1920; t += 160) {
            ev.push_back(on(t, 0, 50));
            ev.push_back(off(t + 160, 0, 50));
        }
        const Result r = convert(song(ev, 1920), 0, barSpans(song(ev, 1920), nullptr, 0, 0), {});
        QCOMPARE(r.chunks[0].grid, std::uint8_t(2)); // 1/16T
        QCOMPARE(r.report.snapped, 0);
        QCOMPARE(r.chunks[0].notes[1].start, std::uint16_t(32));

        const Song late = song({on(130, 0, 50), off(250, 0, 50)}, 1920);
        const Result l = convert(late, 0, barSpans(late, nullptr, 0, 0), {});
        QCOMPARE(l.chunks[0].grid, std::uint8_t(0));
        QCOMPARE(l.chunks[0].notes[0].start, std::uint16_t(24));
        QCOMPARE(l.report.snapped, 1);
    }

    // 3/4: five bars (15 beats) fit a pattern.
    void threeFour()
    {
        const Song s = song({on(0, 0, 40), off(240, 0, 40)}, 7 * 1440, 3);
        const Result r = convert(s, 0, barSpans(s, nullptr, 0, 6), {});
        QCOMPARE(r.chunks.size(), std::size_t(2));
        QCOMPARE(r.chunks[0].barCount, 5);
        QCOMPARE(r.chunks[0].length, std::uint16_t(15 * kTicksPerBeat));
    }

    // Korrode: a chord keeps its top note.
    void monoKeepsTheTop()
    {
        const Song s = song({on(0, 0, 40), on(0, 1, 47), on(0, 2, 52), off(480, 0, 40), off(480, 1, 47), off(480, 2, 52)}, 1920);
        Options o;
        o.mono = true;
        const Result r = convert(s, 0, barSpans(s, nullptr, 0, 0), o);
        QCOMPARE(r.chunks[0].notes.size(), std::size_t(1));
        QCOMPARE(r.chunks[0].notes[0].pitch, std::uint8_t(52));
        QCOMPARE(r.report.reduced, 2);
        const Result poly = convert(s, 0, barSpans(s, nullptr, 0, 0), {});
        QCOMPARE(poly.chunks[0].notes.size(), std::size_t(3));
    }

    // Drums: mapped to Kessel's voices; notes it has no voice for are dropped.
    void drumsMapped()
    {
        const Song s = song({on(0, 9, 36), off(60, 9, 36), on(480, 9, 52), off(540, 9, 52), on(960, 9, 29), off(1000, 9, 29)},
                            1920, 4, true);
        Options o;
        o.drumMap = true;
        const Result r = convert(s, 0, barSpans(s, nullptr, 0, 0), o);
        QCOMPARE(r.chunks[0].notes.size(), std::size_t(2));
        QCOMPARE(r.chunks[0].notes[0].pitch, std::uint8_t(36));
        QCOMPARE(r.chunks[0].notes[1].pitch, std::uint8_t(mapToBasicKit(52)));
        QCOMPARE(r.report.dropped, 1);
    }

    // A tab file with a repeat: a written bar comes from the first time it
    // plays, and a note doesn't run on into a bar the song doesn't play next.
    void repeatsAndWrittenBars()
    {
        // Written bars 0 1 2, played 0 1 0 1 2. Bar 1's last note is held into the repeat.
        const Song s = song({on(1920, 0, 45), off(2400, 0, 45), on(3360, 0, 47), off(4320, 0, 47), // bar 1, its last held on
                             on(9600, 0, 50), off(9720, 0, 50)},                                     // bar 2 (played 5th)
                            5 * 1920);
        Score score;
        score.bars.resize(3);
        score.playOrder = {0, 1, 0, 1, 2};
        score.playStarts = {0, 1920, 3840, 5760, 7680};
        QCOMPARE(barCount(s, &score), 3);
        const auto spans = barSpans(s, &score, 1, 2);
        QCOMPARE(spans.size(), std::size_t(2));
        QCOMPARE(spans[0].start, Tick(1920));
        QCOMPARE(spans[1].start, Tick(7680)); // bar 2: after the repeat
        const Result r = convert(s, 0, spans, {});
        QCOMPARE(r.chunks.size(), std::size_t(1));
        // Bar 1's held note stops at its bar's end (bar 2 isn't what plays after it).
        QCOMPARE(r.chunks[0].notes[1].start, std::uint16_t(3 * kTicksPerBeat));
        QCOMPARE(r.chunks[0].notes[1].length, std::uint16_t(kTicksPerBeat));
        QCOMPARE(r.chunks[0].notes.size(), std::size_t(2)); // bar 2 (played at 7680) has nothing until 9600
    }
};

QTEST_GUILESS_MAIN(TestSongImport)
#include "tst_songimport.moc"
