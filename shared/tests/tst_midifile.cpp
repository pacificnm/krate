#include <QtTest>

#include <QTemporaryDir>

#include <cmath>
#include <fstream>
#include <initializer_list>
#include <vector>

#include "song/MidiFile.h"

using namespace krate::song;

namespace {

using Bytes = std::vector<std::uint8_t>;

void append(Bytes &out, std::initializer_list<int> bytes)
{
    for (int b : bytes)
        out.push_back(std::uint8_t(b));
}
void be(Bytes &out, std::uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; --i)
        out.push_back(std::uint8_t(v >> (8 * i)));
}
void vlq(Bytes &out, std::uint32_t v)
{
    Bytes b{std::uint8_t(v & 0x7F)};
    while (v >>= 7)
        b.insert(b.begin(), std::uint8_t((v & 0x7F) | 0x80));
    out.insert(out.end(), b.begin(), b.end());
}

// A track's events, each with its delta time.
struct TrackBuilder
{
    Bytes body;
    TrackBuilder &event(std::uint32_t delta, std::initializer_list<int> bytes)
    {
        vlq(body, delta);
        append(body, bytes);
        return *this;
    }
    TrackBuilder &meta(std::uint32_t delta, int type, const std::string &text)
    {
        vlq(body, delta);
        append(body, {0xFF, type});
        vlq(body, std::uint32_t(text.size()));
        body.insert(body.end(), text.begin(), text.end());
        return *this;
    }
    TrackBuilder &tempo(std::uint32_t delta, double bpm)
    {
        const auto us = std::uint32_t(std::lround(60000000.0 / bpm));
        return event(delta, {0xFF, 0x51, 0x03, int(us >> 16 & 0xFF), int(us >> 8 & 0xFF), int(us & 0xFF)});
    }
    TrackBuilder &time(std::uint32_t delta, int num, int denPower)
    {
        return event(delta, {0xFF, 0x58, 0x04, num, denPower, 24, 8});
    }
};

Bytes smf(int format, int division, const std::vector<TrackBuilder> &tracks, bool endOfTrack = true)
{
    Bytes out;
    append(out, {'M', 'T', 'h', 'd'});
    be(out, 6, 4);
    be(out, std::uint32_t(format), 2);
    be(out, std::uint32_t(tracks.size()), 2);
    be(out, std::uint32_t(division), 2);
    for (TrackBuilder t : tracks) {
        if (endOfTrack)
            t.event(0, {0xFF, 0x2F, 0x00});
        append(out, {'M', 'T', 'r', 'k'});
        be(out, std::uint32_t(t.body.size()), 4);
        out.insert(out.end(), t.body.begin(), t.body.end());
    }
    return out;
}

MidiFileResult read(const Bytes &b) { return readMidiFile(std::span<const std::uint8_t>(b)); }

} // namespace

class TestMidiFile : public QObject
{
    Q_OBJECT

private slots:
    void tempoMapTimes()
    {
        TempoMap map(480);
        QCOMPARE(map.bpmAt(0), 120.0); // the default
        map.addTempo(0, 100.0);
        map.addTempo(1920, 150.0); // after one 4/4 bar
        QCOMPARE(map.bpmAt(1919), 100.0);
        QCOMPARE(map.bpmAt(1920), 150.0);
        QCOMPARE(map.beatAt(960), 2.0);
        // Bar 1 at 100 BPM is 2.4 s; a beat at 150 is 0.4 s.
        QVERIFY(std::abs(map.secondsAt(1920) - 2.4) < 1e-9);
        QVERIFY(std::abs(map.secondsAt(1920 + 480) - 2.8) < 1e-9);
        QCOMPARE(map.tickAtSeconds(2.8), Tick(2400));
        QCOMPARE(map.tickAtSeconds(1.2), Tick(960));
        map.addTempo(1920, 60.0); // replaces
        QCOMPARE(map.tempos().size(), std::size_t(2));
        QCOMPARE(map.bpmAt(5000), 60.0);
    }

    void barsFollowTimeSignatures()
    {
        TempoMap map(480);
        map.addTimeSignature(0, 4, 4);       // bars 0-1: 1920 ticks each
        map.addTimeSignature(3840, 3, 4);    // bars 2-3: 1440
        map.addTimeSignature(3840 + 2880, 6, 8); // bar 4 on: 1440, beats are eighths
        QCOMPARE(map.barStart(0), Tick(0));
        QCOMPARE(map.barStart(2), Tick(3840));
        QCOMPARE(map.barStart(4), Tick(6720));
        QCOMPARE(map.barStart(6), Tick(6720 + 2 * 1440));
        BarPosition p = map.barAt(3840 + 1440 + 480);
        QCOMPARE(p.bar, 3);
        QCOMPARE(p.beat, 1.0);
        QCOMPARE(p.time.numerator, 3);
        p = map.barAt(6720 + 1440 + 720);
        QCOMPARE(p.bar, 5);
        QCOMPARE(p.beat, 3.0); // eighth notes
        QCOMPARE(p.barStart, Tick(6720 + 1440));
        QCOMPARE(map.barAt(0).bar, 0);
        QCOMPARE(map.barAt(1919).bar, 0);
        QCOMPARE(map.barAt(1920).bar, 1);
    }

    void formatOneSong()
    {
        TrackBuilder conductor;
        conductor.meta(0, 0x03, "Practice Song").tempo(0, 100.0).time(0, 4, 2).tempo(1920, 120.0);
        TrackBuilder guitar;
        guitar.meta(0, 0x03, "Guitar")
            .event(0, {0xC0, 29})
            .event(0, {0x90, 40, 100})
            .event(0, {47, 90})          // running status
            .event(480, {0x90, 40, 0})   // velocity 0 = off
            .event(0, {47, 0})
            .event(0, {0xE0, 0x00, 0x50}) // a bend
            .event(0, {0x90, 52, 80})
            .meta(0, 0x05, "la")          // lyrics: not kept
            .event(480, {0x80, 52, 64});
        TrackBuilder drums;
        drums.meta(0, 0x03, "Drums").event(0, {0x99, 36, 120}).event(240, {0x89, 36, 0});
        const auto result = read(smf(1, 480, {conductor, guitar, drums}));
        QVERIFY2(result.song, result.error.c_str());
        const Song &song = *result.song;
        QCOMPARE(song.title, std::string("Practice Song"));
        QCOMPARE(song.tempo.ppq(), 480);
        QCOMPARE(song.tempo.bpmAt(0), 100.0);
        QCOMPARE(song.tempo.bpmAt(1920), 120.0);
        QCOMPARE(song.tracks.size(), std::size_t(2)); // the conductor has no notes
        const Track &g = song.tracks[0];
        QCOMPARE(g.name, std::string("Guitar"));
        QCOMPARE(g.channel, 0);
        QCOMPARE(g.program, 29);
        QVERIFY(!g.drums);
        QCOMPARE(g.noteCount, 3);
        QCOMPARE(g.lowestNote, 40);
        QCOMPARE(g.highestNote, 52);
        QCOMPARE(g.events.size(), std::size_t(7));
        QCOMPARE(int(g.events[2].status), 0x80); // velocity-0 note-on stored as note-off
        QCOMPARE(g.events[2].tick, Tick(480));
        QCOMPARE(int(g.events[4].type()), 0xE0);
        QCOMPARE(int(g.events[4].data2), 0x50);
        const Track &d = song.tracks[1];
        QVERIFY(d.drums);
        QCOMPARE(d.channel, 9);
        QCOMPARE(song.length, Tick(1920)); // the conductor's last tempo change
    }

    // Format 0 has one track; its channels become separate parts.
    void formatZeroSplitsChannels()
    {
        TrackBuilder all;
        all.meta(0, 0x03, "Band")
            .event(0, {0x90, 60, 100})
            .event(0, {0x91, 36, 100})
            .event(0, {0x99, 42, 100})
            .event(480, {0x80, 60, 0})
            .event(0, {0x81, 36, 0})
            .event(0, {0x89, 42, 0});
        const auto result = read(smf(0, 96, {all}));
        QVERIFY2(result.song, result.error.c_str());
        const Song &song = *result.song;
        QCOMPARE(song.tracks.size(), std::size_t(3));
        QCOMPARE(song.tracks[0].name, std::string("Band ch 1"));
        QCOMPARE(song.tracks[1].name, std::string("Band ch 2"));
        QCOMPARE(song.tracks[1].lowestNote, 36);
        QVERIFY(song.tracks[2].drums);
        QCOMPARE(song.title, std::string()); // format 0: no conductor track
    }

    // Every note-on gets a note-off: a retriggered key ends the earlier note,
    // and notes left on end with the song; stray note-offs are dropped.
    void notesAreClosed()
    {
        TrackBuilder t;
        t.event(0, {0x80, 50, 0})    // stray
            .event(0, {0x90, 60, 100})
            .event(100, {0x90, 60, 90}) // again, still on
            .event(100, {0x80, 60, 0})
            .event(0, {0x90, 64, 100})  // never released
            .event(300, {0xB0, 64, 127});
        const auto result = read(smf(1, 480, {t}));
        QVERIFY2(result.song, result.error.c_str());
        const auto &e = result.song->tracks.at(0).events;
        std::vector<std::pair<int, Tick>> seen;
        for (const Event &ev : e)
            seen.push_back({ev.status, ev.tick});
        const std::vector<std::pair<int, Tick>> want{{0x90, 0}, {0x80, 100}, {0x90, 100}, {0x80, 200},
                                                     {0x90, 200}, {0xB0, 500}, {0x80, 500}};
        QCOMPARE(seen, want);
        QCOMPARE(result.song->tracks.at(0).noteCount, 3);
        QCOMPARE(result.song->tracks.at(0).name, std::string("Track 1")); // unnamed
    }

    // Files from tab software put each string on its own channel: one track
    // stays one part, its events keep their channels, and set-up messages on
    // channels with no notes are kept with it. Latin-1 names become UTF-8.
    void stringsOnChannelsStayOneTrack()
    {
        TrackBuilder bass;
        bass.meta(0, 0x03, "Bass G\xfcnter") // Latin-1 u-umlaut
            .event(0, {0xB0, 7, 100})          // set-up on channel 1, no notes there
            .event(0, {0xE1, 0, 64})
            .event(0, {0x91, 28, 100})         // E string
            .event(0, {0x92, 33, 100})         // A string
            .event(240, {0x81, 28, 0})
            .event(0, {0xE2, 0, 80})           // bend on the A string only
            .event(240, {0x82, 33, 0});
        TrackBuilder setupOnly; // no notes at all: dropped
        setupOnly.meta(0, 0x03, "Empty").event(0, {0xB3, 7, 100});
        const auto result = read(smf(1, 480, {bass, setupOnly}));
        QVERIFY2(result.song, result.error.c_str());
        const Song &song = *result.song;
        QCOMPARE(song.tracks.size(), std::size_t(1));
        const Track &t = song.tracks[0];
        QCOMPARE(t.name, std::string("Bass G\xc3\xbcnter"));
        QCOMPARE(t.noteCount, 2);
        QCOMPARE(t.events.size(), std::size_t(7));
        QCOMPARE(t.channel, 1); // the first channel with the most notes
        QVERIFY(!t.drums);
        int bendChannel = -1;
        for (const Event &e : t.events)
            if (e.type() == 0xE0 && e.tick == 240)
                bendChannel = e.channel();
        QCOMPARE(bendChannel, 2);
        QCOMPARE(song.title, std::string()); // the first track has notes: no conductor
        // Valid UTF-8 stays as it is.
        TrackBuilder named;
        named.meta(0, 0x03, "Caf\xc3\xa9").event(0, {0x90, 60, 1}).event(1, {0x80, 60, 0});
        QCOMPARE(read(smf(1, 480, {named})).song->tracks.at(0).name, std::string("Caf\xc3\xa9"));
    }

    void skipsWhatItDoesNotUse()
    {
        Bytes file = smf(1, 480, {});
        // An unknown chunk, then a track with sysex and aftertouch.
        append(file, {'X', 'Y', 'Z', 'W', 0, 0, 0, 2, 0xAA, 0xBB});
        TrackBuilder t;
        t.event(0, {0xF0, 0x03, 0x7E, 0x09, 0xF7})
            .event(0, {0xD0, 40})
            .event(0, {0xA0, 60, 10})
            .event(0, {0x90, 60, 100})
            .event(10, {0x80, 60, 0})
            .event(0, {0xFF, 0x2F, 0x00});
        append(file, {'M', 'T', 'r', 'k'});
        be(file, std::uint32_t(t.body.size()), 4);
        file.insert(file.end(), t.body.begin(), t.body.end());
        file[11] = 1; // one track
        const auto result = read(file);
        QVERIFY2(result.song, result.error.c_str());
        QCOMPARE(result.song->tracks.at(0).events.size(), std::size_t(2));
    }

    void errors()
    {
        QVERIFY(!read(Bytes{'R', 'I', 'F', 'F'}).song);
        QCOMPARE(read(Bytes{'R', 'I', 'F', 'F'}).error, std::string("not a MIDI file"));
        Bytes smpte = smf(1, 0xE728, {TrackBuilder{}});
        QVERIFY(!read(smpte).song);
        QVERIFY(!read(smf(2, 480, {TrackBuilder{}})).song);
        // Cut short in the middle of a track.
        TrackBuilder t;
        t.event(0, {0x90, 60, 100}).event(480, {0x80, 60, 0});
        Bytes cut = smf(1, 480, {t});
        cut.resize(cut.size() - 6);
        const auto result = read(cut);
        QVERIFY(!result.song);
        QVERIFY(!result.error.empty());
        // Data before any status byte.
        TrackBuilder bad;
        bad.event(0, {60, 100});
        QVERIFY(!read(smf(1, 480, {bad})).song);
    }

    void readsAFile()
    {
        QTemporaryDir dir;
        const std::string path = dir.filePath(QStringLiteral("song.mid")).toStdString();
        TrackBuilder t;
        t.event(0, {0x90, 60, 100}).event(480, {0x80, 60, 0});
        const Bytes data = smf(1, 480, {t});
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
        const auto result = readMidiFile(path);
        QVERIFY2(result.song, result.error.c_str());
        QCOMPARE(result.song->tracks.size(), std::size_t(1));
        QVERIFY(!readMidiFile(dir.filePath(QStringLiteral("missing.mid")).toStdString()).song);
    }
};

QTEST_GUILESS_MAIN(TestMidiFile)
#include "tst_midifile.moc"
