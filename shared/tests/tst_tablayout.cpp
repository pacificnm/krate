#include <QtTest>

#include <QElapsedTimer>

#include <algorithm>
#include <string>
#include <vector>

#include "song/GpFile.h"
#include "song/TabLayout.h"

using namespace krate::song;

namespace {

ScoreNote note(int string, int fret, std::uint32_t techniques = 0)
{
    ScoreNote n;
    n.string = string;
    n.fret = fret;
    n.pitch = 40 + fret;
    n.techniques = techniques;
    return n;
}

ScoreBeat beat(Tick start, int value, std::vector<ScoreNote> notes = {})
{
    ScoreBeat b;
    b.start = start;
    b.length = 960 * 4 / value;
    b.noteValue = value;
    b.notes = std::move(notes);
    return b;
}

// Four bars of an original exercise on a six-string guitar: 4/4, 4/4 (repeat
// end x3), 3/4, 3/4 (marked as a first ending). The bars' content is filled per test.
Score makeScore(int bars = 4)
{
    Score s;
    s.ppq = 960;
    for (int m = 0; m < bars; ++m) {
        MasterBar b;
        b.time = {0, m < 2 || m >= 4 ? 4 : 3, 4};
        s.bars.push_back(b);
    }
    if (bars > 1) { // bars 1-2 repeat, three times
        s.bars[0].repeatStart = true;
        s.bars[1].repeatEnd = true;
        s.bars[1].repeatCount = 3;
    }
    if (bars > 3)
        s.bars[3].endings = 0b01;
    ScoreTrack t;
    t.name = "Guitar";
    t.tuning = {40, 45, 50, 55, 59, 64};
    t.bars.resize(std::size_t(bars));
    for (int m = 0; m < bars; ++m) {
        const int beats = s.bars[std::size_t(m)].time.numerator;
        std::vector<ScoreBeat> voice;
        for (int k = 0; k < beats; ++k)
            voice.push_back(beat(Tick(k) * 960, 4, {note(k % 6, k)}));
        t.bars[std::size_t(m)].voices.push_back(voice);
    }
    s.tracks.push_back(t);
    s.playOrder = playOrder(s.bars);
    Tick at = 0;
    for (int m : s.playOrder) {
        s.playStarts.push_back(at);
        at += Tick(960) * s.bars[std::size_t(m)].time.numerator;
    }
    return s;
}

std::vector<const TabItem *> texts(const TabLayoutResult &l, const std::string &text)
{
    std::vector<const TabItem *> out;
    for (const TabSystem &s : l.systems)
        for (const TabItem &i : s.items)
            if (i.kind == TabItem::Kind::Text && i.text == text)
                out.push_back(&i);
    return out;
}

} // namespace

class TestTabLayout : public QObject
{
    Q_OBJECT

private slots:
    void barsFillSystems()
    {
        Score s = makeScore(12);
        const TabLayoutResult l = layoutTab(s, 0, 600.0);
        QVERIFY(l.systems.size() >= 2);
        QCOMPARE(l.strings, 6);
        int placed = 0;
        for (std::size_t k = 0; k < l.systems.size(); ++k) {
            const TabSystem &sys = l.systems[k];
            QVERIFY(!sys.bars.empty());
            if (k + 1 < l.systems.size()) // full systems reach the right edge
                QVERIFY(std::abs(sys.bars.back().x1 - 599.0) < 1.0);
            QVERIFY(sys.bars.back().x1 <= 600.0);
            for (std::size_t b = 1; b < sys.bars.size(); ++b)
                QCOMPARE(sys.bars[b].x0, sys.bars[b - 1].x1);
            placed += int(sys.bars.size());
            QCOMPARE(sys.staffBottom - sys.staffTop, 5 * TabStyle{}.lineGap);
            if (k > 0)
                QCOMPARE(sys.top, l.systems[k - 1].top + l.systems[k - 1].height);
        }
        QCOMPARE(placed, 12);
        QCOMPARE(l.height, l.systems.back().top + l.systems.back().height);
        for (const auto &[system, index] : l.barPlace)
            QVERIFY(system >= 0 && index >= 0);
        // Wider: fewer systems.
        QVERIFY(layoutTab(s, 0, 1400.0).systems.size() < l.systems.size());
    }

    void fretsSitOnTheirStrings()
    {
        Score s = makeScore();
        const TabLayoutResult l = layoutTab(s, 0, 1200.0);
        const TabSystem &sys = l.systems.front();
        // Bar 0: string k gets fret k; string 0 (the lowest) is the bottom line.
        for (int k = 0; k < 4; ++k) {
            const auto found = texts(l, std::to_string(k));
            QVERIFY(!found.empty());
            const double want = sys.staffTop + double(5 - k) * TabStyle{}.lineGap;
            const auto onString = std::find_if(found.begin(), found.end(), [&](const TabItem *i) { return std::abs(i->y1 - want) < 0.01; });
            QVERIFY(onString != found.end());
            QVERIFY((*onString)->knockout); // the staff line stops behind it
        }
        // Beats are left to right inside their bar.
        const TabBarBox &box = sys.bars.front();
        QCOMPARE(box.ticks, (std::vector<Tick>{0, 960, 1920, 2880}));
        QVERIFY(std::is_sorted(box.xs.begin(), box.xs.end()));
        QVERIFY(box.xs.front() > box.x0 && box.xs.back() < box.x1);
        // Tuning letters, high string on top.
        QCOMPARE(texts(l, "E").size(), std::size_t(2 * l.systems.size()));
        QCOMPARE(texts(l, "B").size(), l.systems.size());
    }

    void marks()
    {
        Score s = makeScore();
        auto &voice = s.tracks[0].bars[0].voices[0];
        voice[0].notes = {note(1, 5, HammerOn)};
        voice[1].notes = {note(1, 7, HammerTarget)}; // hammer-on: h
        voice[2].notes = {note(2, 9, Slide | PalmMute)};
        voice[3].notes = {note(2, 7)}; // slide down from 9
        auto &v1 = s.tracks[0].bars[1].voices[0];
        v1[0].notes = {note(0, 3, DeadNote)};
        v1[1].notes = {note(3, 12, Harmonic)};
        v1[2].notes = {note(3, 12, TieDestination)};
        ScoreNote bent = note(4, 7, Bend | Vibrato);
        bent.bend = {{0.0, 0.0}, {0.5, 2.0}};
        v1[3].notes = {bent};
        s.tracks[0].bars[2].voices[0][0].notes = {note(4, 9, HammerOn)};
        s.tracks[0].bars[2].voices[0][1].notes = {note(4, 5, HammerTarget)}; // pull-off: p
        const TabLayoutResult l = layoutTab(s, 0, 1600.0);
        QCOMPARE(texts(l, "h").size(), std::size_t(1));
        QCOMPARE(texts(l, "p").size(), std::size_t(1));
        QCOMPARE(texts(l, "P.M.").size(), std::size_t(1));
        QCOMPARE(texts(l, "x").size(), std::size_t(1));
        QCOMPARE(texts(l, "<12>").size(), std::size_t(1));
        QCOMPARE(texts(l, "(12)").size(), std::size_t(1));
        QCOMPARE(texts(l, "full").size(), std::size_t(1));
        QCOMPARE(texts(l, "~").size(), std::size_t(1));
        QCOMPARE(texts(l, "~").front()->tone, TabItem::Tone::Ink);
        // Time signatures: bar 1 (4/4) and bar 3 (3/4) only.
        QCOMPARE(texts(l, "3").size() >= 1, true);
        int fours = 0;
        for (const TabItem *i : texts(l, "4"))
            fours += i->bold ? 1 : 0;
        QCOMPARE(fours, 3); // 4/4 at the start, the 4 of 3/4
        // Repeat x3 and the second ending.
        QCOMPARE(texts(l, "x3").size(), std::size_t(1));
        QCOMPARE(texts(l, "1.").size(), std::size_t(1));
    }

    void bendLabels()
    {
        Score s = makeScore();
        for (const auto &[semitones, label] : std::vector<std::pair<double, std::string>>{
                 {1.0, "½"}, {0.5, "¼"}, {3.0, "1½"}, {4.0, "2"}}) {
            ScoreNote bent = note(4, 7, Bend);
            bent.bend = {{0.0, 0.0}, {1.0, semitones}};
            s.tracks[0].bars[0].voices[0][0].notes = {bent};
            const auto found = texts(layoutTab(s, 0, 1600.0), label);
            QCOMPARE(std::count_if(found.begin(), found.end(), [](const TabItem *i) { return i->tone == TabItem::Tone::Accent; }), 1);
        }
    }

    void cursorFollowsThePlayOrder()
    {
        Score s = makeScore();
        const TabLayoutResult l = layoutTab(s, 0, 1600.0);
        QCOMPARE(s.playOrder, (std::vector<int>{0, 1, 0, 1, 0, 1, 2, 3}));
        const TabBarBox &bar0 = l.systems[std::size_t(l.barPlace[0].first)].bars[std::size_t(l.barPlace[0].second)];
        TabCursor c = cursorAt(s, l, 0);
        QCOMPARE(c.bar, 0);
        QCOMPARE(c.x, bar0.xs[0]);
        c = cursorAt(s, l, 480); // halfway through the first beat
        QCOMPARE(c.x, (bar0.xs[0] + bar0.xs[1]) / 2.0);
        // The third time through bar 0 (after two repeats) is still bar 0.
        c = cursorAt(s, l, s.playStarts[4] + 960);
        QCOMPARE(c.bar, 0);
        QCOMPARE(c.x, bar0.xs[1]);
        c = cursorAt(s, l, s.playStarts[7]);
        QCOMPARE(c.bar, 3);
        QCOMPARE(cursorAt(s, l, s.playStarts[7] + 960 * 3 + 10).system, -1); // past the end
        QCOMPARE(cursorAt(s, {}, 0).system, -1);
    }

    void clicksFindBars()
    {
        Score s = makeScore(12);
        const TabLayoutResult l = layoutTab(s, 0, 600.0);
        for (int m : {0, 5, 11}) {
            const auto [system, index] = l.barPlace[std::size_t(m)];
            const TabSystem &sys = l.systems[std::size_t(system)];
            const TabBarBox &box = sys.bars[std::size_t(index)];
            QCOMPARE(barAt(l, (box.x0 + box.x1) / 2.0, sys.staffTop + 5.0), m);
        }
        QCOMPARE(barAt(l, 5.0, -10.0), -1);
        QCOMPARE(barAt(l, 5.0, l.height + 10.0), -1);
    }

    void nothingToDraw()
    {
        Score s = makeScore();
        s.tracks[0].drums = true;
        QVERIFY(layoutTab(s, 0, 800.0).systems.empty());
        s.tracks[0].drums = false;
        QVERIFY(layoutTab(s, 0, 50.0).systems.empty()); // too narrow
        QVERIFY(layoutTab(s, 3, 800.0).systems.empty()); // no such track
        s.tracks[0].tuning.clear();
        QVERIFY(layoutTab(s, 0, 800.0).systems.empty());
    }

    // --- Notation ---------------------------------------------------------------

    static std::vector<const TabItem *> items(const TabLayoutResult &l, TabItem::Kind kind)
    {
        std::vector<const TabItem *> out;
        for (const TabSystem &s : l.systems)
            for (const TabItem &i : s.items)
                if (i.kind == kind)
                    out.push_back(&i);
        return out;
    }

    // One 4/4 bar of four quarter notes with the given pitches.
    static Score oneBar(std::vector<int> pitches, const std::string &type = "keys", bool drums = false)
    {
        Score s = makeScore(1);
        s.tracks[0].type = type;
        s.tracks[0].drums = drums;
        auto &voice = s.tracks[0].bars[0].voices[0];
        for (std::size_t k = 0; k < voice.size(); ++k) {
            ScoreNote n;
            n.pitch = pitches[k];
            voice[k].notes = {n};
        }
        return s;
    }

    void notationStaffAndClef()
    {
        const TabStyle style;
        TabLayoutResult l = layoutNotation(oneBar({60, 62, 64, 65}), 0, 900.0);
        QCOMPARE(l.strings, 5);
        const TabSystem &sys = l.systems.front();
        QCOMPARE(sys.staffBottom - sys.staffTop, 4 * style.noteGap);
        QCOMPARE(texts(l, "\U0001D11E").size(), std::size_t(1)); // treble clef
        QCOMPARE(items(l, TabItem::Kind::Note).size(), std::size_t(4));
        // E4 sits on the bottom line, F4 a half-gap above, middle C a ledger line below.
        const auto heads = items(l, TabItem::Kind::Note);
        QCOMPARE(heads[2]->y1, sys.staffBottom);
        QCOMPARE(heads[3]->y1, sys.staffBottom - style.noteGap / 2.0);
        QCOMPARE(heads[0]->y1, sys.staffBottom + style.noteGap);
        QVERIFY(heads[0]->filled); // quarter notes
        // Low notes get the bass clef; guitar is written an octave up (so a low E is treble).
        QCOMPARE(texts(layoutNotation(oneBar({36, 40, 43, 45}), 0, 900.0), "\U0001D122").size(), std::size_t(1));
        const TabLayoutResult guitar = layoutNotation(oneBar({52, 55, 57, 59}, "electricGuitar"), 0, 900.0);
        QCOMPARE(texts(guitar, "\U0001D11E").size(), std::size_t(1));
        QCOMPARE(items(guitar, TabItem::Kind::Note)[0]->y1, guitar.systems[0].staffBottom); // E3 written as E4
        // Tab of the same track still works; notation also covers tracks without strings.
        Score noStrings = oneBar({60, 62, 64, 65});
        noStrings.tracks[0].tuning.clear();
        QVERIFY(layoutTab(noStrings, 0, 900.0).systems.empty());
        QVERIFY(!layoutNotation(noStrings, 0, 900.0).systems.empty());
    }

    void accidentalsLedgerLinesAndRests()
    {
        // F#4, F#4 (no second sharp), F4 (natural), and a rest.
        Score s = oneBar({66, 66, 65, 0});
        s.tracks[0].bars[0].voices[0][3].notes.clear();
        TabLayoutResult l = layoutNotation(s, 0, 900.0);
        QCOMPARE(texts(l, "\u266F").size(), std::size_t(1));
        QCOMPARE(texts(l, "\u266E").size(), std::size_t(1));
        QCOMPARE(texts(l, "\U0001D13D").size(), std::size_t(1)); // a quarter rest
        for (const TabItem *t : texts(l, "\U0001D13D"))
            QVERIFY(t->music);
        // A5 (81) is a ledger line above the treble staff; C6 (84) two (A5's and C6's).
        const auto ledgers = [](const TabLayoutResult &r) {
            int n = 0;
            for (const TabItem *i : items(r, TabItem::Kind::Line))
                if (i->y1 == i->y2 && i->x2 - i->x1 < 20.0)
                    ++n;
            return n;
        };
        QCOMPARE(ledgers(layoutNotation(oneBar({81, 64, 64, 64}), 0, 900.0)), 1);
        QCOMPARE(ledgers(layoutNotation(oneBar({84, 64, 64, 64}), 0, 900.0)), 2);
        QCOMPARE(ledgers(layoutNotation(oneBar({69, 64, 64, 64}), 0, 900.0)), 0); // A4: in the staff
        // Half notes are white heads.
        Score halves = oneBar({64, 64, 64, 64});
        halves.tracks[0].bars[0].voices[0] = {beat(0, 2, {}), beat(1920, 2, {})};
        halves.tracks[0].bars[0].voices[0][0].notes = {ScoreNote{0, 0, 64, 0, {}}};
        halves.tracks[0].bars[0].voices[0][1].notes = {ScoreNote{0, 0, 65, 0, {}}};
        for (const TabItem *h : items(layoutNotation(halves, 0, 900.0), TabItem::Kind::Note))
            QVERIFY(!h->filled);
    }

    void drumNotation()
    {
        Score s = oneBar({36, 38, 42, 46}, "drumKit", true);
        s.tracks[0].tuning.clear();
        QVERIFY(layoutTab(s, 0, 900.0).systems.empty()); // no tab for drums
        const TabLayoutResult l = layoutNotation(s, 0, 900.0);
        QCOMPARE(texts(l, "\U0001D125").size(), std::size_t(1)); // percussion clef
        QCOMPARE(items(l, TabItem::Kind::Note).size(), std::size_t(2)); // kick and snare heads
        QCOMPARE(texts(l, "o").size(), std::size_t(1));                  // the open hi-hat
        const TabSystem &sys = l.systems.front();
        const auto heads = items(l, TabItem::Kind::Note);
        QCOMPARE(heads[0]->y1, sys.staffBottom - TabStyle{}.noteGap / 2.0); // kick in the bottom space
        QCOMPARE(heads[1]->y1, sys.staffTop + 1.5 * TabStyle{}.noteGap);    // snare in the third space
    }

    void notationCursorAndClicks()
    {
        Score s = makeScore();
        const TabLayoutResult l = layoutNotation(s, 0, 1600.0);
        const TabCursor c = cursorAt(s, l, s.playStarts[7]);
        QCOMPARE(c.bar, 3);
        const auto [system, index] = l.barPlace[3];
        const TabBarBox &box = l.systems[std::size_t(system)].bars[std::size_t(index)];
        QCOMPARE(barAt(l, (box.x0 + box.x1) / 2.0, l.systems[std::size_t(system)].staffTop), 3);
    }

    void longSongsAreQuick()
    {
        Score s = makeScore(4);
        // 600 bars of sixteenths across six strings.
        s.bars.assign(600, s.bars[0]);
        s.bars[0].repeatStart = false;
        for (MasterBar &b : s.bars)
            b = MasterBar{};
        std::vector<ScoreBeat> voice;
        for (int k = 0; k < 16; ++k)
            voice.push_back(beat(Tick(k) * 240, 16, {note(k % 6, k % 12, k % 2 ? HammerTarget : HammerOn)}));
        s.tracks[0].bars.assign(600, ScoreBar{{voice}});
        s.playOrder = playOrder(s.bars);
        QElapsedTimer timer;
        timer.start();
        const TabLayoutResult l = layoutTab(s, 0, 1300.0);
        const qint64 ms = timer.elapsed();
        QVERIFY(!l.systems.empty());
        QVERIFY2(ms < 250, qPrintable(QStringLiteral("%1 ms").arg(ms)));
    }
};

QTEST_GUILESS_MAIN(TestTabLayout)
#include "tst_tablayout.moc"
