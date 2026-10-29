#include <QtTest>

#include <QTemporaryDir>

#include <zlib.h>

#include <fstream>
#include <string>
#include <vector>

#include "song/GpFile.h"
#include "song/Xml.h"
#include "song/Zip.h"

using namespace krate::song;

namespace {

using Bytes = std::vector<std::uint8_t>;

void le(Bytes &out, std::uint32_t v, int n)
{
    for (int i = 0; i < n; ++i)
        out.push_back(std::uint8_t(v >> (8 * i)));
}

// A zip archive of the given files, deflated or stored.
Bytes makeZip(const std::vector<std::pair<std::string, std::string>> &files, bool compress = true)
{
    Bytes out, directory;
    for (const auto &[name, content] : files) {
        Bytes data(content.begin(), content.end());
        int method = 0;
        if (compress) {
            Bytes packed(compressBound(uLong(data.size())) + 64);
            z_stream z{};
            deflateInit2(&z, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
            z.next_in = data.data();
            z.avail_in = uInt(data.size());
            z.next_out = packed.data();
            z.avail_out = uInt(packed.size());
            deflate(&z, Z_FINISH);
            packed.resize(z.total_out);
            deflateEnd(&z);
            data = packed;
            method = 8;
        }
        const auto offset = std::uint32_t(out.size());
        const auto crc = std::uint32_t(crc32(0, reinterpret_cast<const Bytef *>(content.data()), uInt(content.size())));
        le(out, 0x04034b50, 4);
        le(out, 20, 2);
        le(out, 0, 2);
        le(out, std::uint32_t(method), 2);
        le(out, 0, 4); // time, date
        le(out, crc, 4);
        le(out, std::uint32_t(data.size()), 4);
        le(out, std::uint32_t(content.size()), 4);
        le(out, std::uint32_t(name.size()), 2);
        le(out, 0, 2);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());

        le(directory, 0x02014b50, 4);
        le(directory, 20, 2);
        le(directory, 20, 2);
        le(directory, 0, 2);
        le(directory, std::uint32_t(method), 2);
        le(directory, 0, 4);
        le(directory, crc, 4);
        le(directory, std::uint32_t(data.size()), 4);
        le(directory, std::uint32_t(content.size()), 4);
        le(directory, std::uint32_t(name.size()), 2);
        le(directory, 0, 2);
        le(directory, 0, 2);
        le(directory, 0, 2);
        le(directory, 0, 2);
        le(directory, 0, 4);
        le(directory, offset, 4);
        directory.insert(directory.end(), name.begin(), name.end());
    }
    const auto directoryOffset = std::uint32_t(out.size());
    out.insert(out.end(), directory.begin(), directory.end());
    le(out, 0x06054b50, 4);
    le(out, 0, 2);
    le(out, 0, 2);
    le(out, std::uint32_t(files.size()), 2);
    le(out, std::uint32_t(files.size()), 2);
    le(out, std::uint32_t(directory.size()), 4);
    le(out, directoryOffset, 4);
    le(out, 0, 2);
    return out;
}

std::string prop(const std::string &name, const std::string &inner)
{
    return "<Property name=\"" + name + "\">" + inner + "</Property>";
}

std::string note(int id, int string, int fret, int midi, const std::string &extra = {}, const std::string &children = {})
{
    return "<Note id=\"" + std::to_string(id) + "\">" + children + "<Properties>"
           + prop("String", "<String>" + std::to_string(string) + "</String>")
           + prop("Fret", "<Fret>" + std::to_string(fret) + "</Fret>")
           + (midi >= 0 ? prop("Midi", "<Number>" + std::to_string(midi) + "</Number>") : std::string()) + extra
           + "</Properties></Note>";
}

// A small original score: two tracks (a drop-D guitar with a capo-less tuning,
// and drums), four bars. Bars 1-2 repeat; bar 2 changes to 3/4.
//   Guitar bar 1: quarter [low D + A], quarter [5th fret A string, slides],
//                 dotted eighth [tied over], sixteenth rest... (see the tests)
std::string gpif(const std::string &masterBarsXml, const std::string &tempoXml = {})
{
    std::string x = R"(<?xml version="1.0" encoding="utf-8"?>
<!-- a comment -->
<GPIF>
  <GPVersion>8</GPVersion>
  <Score><Title><![CDATA[Practice Riff]]></Title><Artist><![CDATA[Krate & Co]]></Artist></Score>
  <MasterTrack><Automations>)" + tempoXml + R"(</Automations></MasterTrack>
  <Tracks>
    <Track id="0"><Name><![CDATA[Guitar]]></Name>
      <InstrumentSet><Name>Distortion Guitar</Name><Type>electricGuitar</Type></InstrumentSet>
      <Sounds><Sound><MIDI><Program>30</Program></MIDI></Sound></Sounds>
      <Lyrics><Line><Text><![CDATA[never read]]></Text></Line></Lyrics>
      <Staves><Staff><Properties>)" + prop("Tuning", "<Pitches>38 45 50 55 59 64</Pitches>")
                    + prop("CapoFret", "<Fret>0</Fret>") + R"(</Properties></Staff></Staves>
    </Track>
    <Track id="1"><Name><![CDATA[Drums]]></Name>
      <InstrumentSet><Name>Drums</Name><Type>drumKit</Type></InstrumentSet>
      <Staves><Staff><Properties>)" + prop("Tuning", "<Pitches>0 0 0 0 0 0</Pitches>") + R"(</Properties></Staff></Staves>
    </Track>
  </Tracks>
  <MasterBars>)" + masterBarsXml + R"(</MasterBars>
  <Bars>
    <Bar id="0"><Voices>0 -1 -1 -1</Voices></Bar>
    <Bar id="1"><Voices>1 -1 -1 -1</Voices></Bar>
    <Bar id="2"><Voices>2 3 -1 -1</Voices></Bar>
    <Bar id="3"><Voices>4 -1 -1 -1</Voices></Bar>
  </Bars>
  <Voices>
    <Voice id="0"><Beats>0 1 2 3</Beats></Voice>
    <Voice id="1"><Beats>4 4 4 4</Beats></Voice>
    <Voice id="2"><Beats>5 6 7</Beats></Voice>
    <Voice id="3"><Beats>8</Beats></Voice>
    <Voice id="4"><Beats>9</Beats></Voice>
  </Voices>
  <Beats>
    <Beat id="0"><Rhythm ref="0"/><Dynamic>MF</Dynamic><Notes>0 1</Notes></Beat>
    <Beat id="1"><Rhythm ref="0"/><Notes>2</Notes><Lyrics><Line>la</Line></Lyrics></Beat>
    <Beat id="2"><Rhythm ref="1"/><Notes>3</Notes></Beat>
    <Beat id="3"><Rhythm ref="2"/></Beat>
    <Beat id="4"><Rhythm ref="0"/><Notes>10 11</Notes></Beat>
    <Beat id="5"><GraceNotes>BeforeBeat</GraceNotes><Rhythm ref="2"/><Notes>4</Notes></Beat>
    <Beat id="6"><Rhythm ref="3"/><Notes>5 6</Notes></Beat>
    <Beat id="7"><Rhythm ref="4"/><Notes>7</Notes></Beat>
    <Beat id="8"><Rhythm ref="5"/><Notes>8</Notes></Beat>
    <Beat id="9"><Rhythm ref="5"/><Dynamic>PP</Dynamic><Notes>9</Notes></Beat>
  </Beats>
  <Notes>)";
    x += note(0, 0, 0, 38, prop("PalmMuted", "<Enable/>"));
    x += note(1, 1, 0, 45);
    x += note(2, 1, 5, 50, prop("Slide", "<Flags>2</Flags>"));
    x += note(3, 1, 5, 50, {}, "<Tie origin=\"false\" destination=\"true\"/>");
    x += note(4, 2, 2, 52);                                                          // grace
    x += note(5, 3, 7, -1, prop("HopoOrigin", "<Enable/>"), "<Accent>4</Accent>"); // pitch from the tuning: 55 + 7
    x += note(6, 4, 5, 64, prop("HarmonicType", "<HType>pinch</HType>") + prop("HarmonicFret", "<HFret>12</HFret>"));
    x += note(7, 5, 0, 64, prop("HarmonicType", "<HType>Natural</HType>") + prop("HarmonicFret", "<HFret>7</HFret>"));
    x += note(8, 2, 7, 57,
              prop("Bended", "<Enable/>") + prop("BendOriginValue", "<Float>0</Float>")
                  + prop("BendDestinationOffset", "<Float>50</Float>") + prop("BendDestinationValue", "<Float>100</Float>"),
              "<Vibrato>Slight</Vibrato>");
    x += note(9, 0, 3, 41, prop("Muted", "<Enable/>"));
    x += note(10, 0, 36, 36);
    x += note(11, 1, 42, 42);
    x += R"(</Notes>
  <Rhythms>
    <Rhythm id="0"><NoteValue>Quarter</NoteValue></Rhythm>
    <Rhythm id="1"><NoteValue>Eighth</NoteValue><AugmentationDot count="1"/></Rhythm>
    <Rhythm id="2"><NoteValue>16th</NoteValue></Rhythm>
    <Rhythm id="3"><NoteValue>Eighth</NoteValue><PrimaryTuplet num="3" den="2"/></Rhythm>
    <Rhythm id="4"><NoteValue>Half</NoteValue></Rhythm>
    <Rhythm id="5"><NoteValue>Half</NoteValue><AugmentationDot count="1"/></Rhythm>
  </Rhythms>
</GPIF>)";
    return x;
}

// Guitar bar 0 (4/4) and bar 2 (3/4) with drums bar 1 under them, repeated twice.
const std::string kBars = R"(
    <MasterBar><Time>4/4</Time><Bars>0 1</Bars><Repeat start="true" end="false" count="0"/></MasterBar>
    <MasterBar><Time>3/4</Time><Bars>2 -1</Bars><Repeat start="false" end="true" count="2"/></MasterBar>
    <MasterBar><Time>3/4</Time><Bars>3 1</Bars></MasterBar>)";

const std::string kTempo = R"(
    <Automation><Type>Tempo</Type><Linear>false</Linear><Bar>0</Bar><Position>0</Position><Value>90 2</Value></Automation>
    <Automation><Type>Tempo</Type><Linear>false</Linear><Bar>1</Bar><Position>0.5</Position><Value>60 1</Value></Automation>
    <Automation><Type>Tempo</Type><Linear>false</Linear><Bar>2</Bar><Position>0</Position><Value>80 3</Value></Automation>)";

GpFileResult readTab(const std::string &score, bool compress = true)
{
    const Bytes zip = makeZip({{"VERSION", "8.0"}, {"Content/score.gpif", score}}, compress);
    return readGpFile(std::span<const std::uint8_t>(zip));
}

MasterBar bar(bool start = false, bool end = false, int count = 0, std::uint32_t endings = 0)
{
    MasterBar b;
    b.repeatStart = start;
    b.repeatEnd = end;
    b.repeatCount = count;
    b.endings = endings;
    return b;
}

} // namespace

class TestGpFile : public QObject
{
    Q_OBJECT

private slots:
    void zipStoredAndDeflated()
    {
        for (const bool compress : {false, true}) {
            const Bytes zip = makeZip({{"a.txt", "hello"}, {"dir/b.txt", std::string(5000, 'x')}}, compress);
            ZipArchive archive;
            QVERIFY(archive.open(std::span<const std::uint8_t>(zip)));
            QCOMPARE(archive.entries().size(), std::size_t(2));
            const ZipEntry *b = archive.find("dir/b.txt");
            QVERIFY(b);
            const auto data = archive.read(*b);
            QVERIFY(data.has_value());
            QCOMPARE(data->size(), std::size_t(5000));
            QCOMPARE(int((*data)[4999]), int('x'));
            QVERIFY(!archive.read(*b, 100).has_value()); // over the size limit
            QVERIFY(!archive.find("missing"));
        }
        ZipArchive archive;
        const Bytes junk(100, 7);
        QVERIFY(!archive.open(std::span<const std::uint8_t>(junk)));
        // A damaged deflate stream doesn't read.
        Bytes zip = makeZip({{"a.txt", std::string(3000, 'q')}});
        zip[40] ^= 0xFF;
        zip[41] ^= 0xFF;
        QVERIFY(archive.open(std::span<const std::uint8_t>(zip)));
        QVERIFY(!archive.read(archive.entries().front()).has_value());
    }

    void xmlParser()
    {
        const auto doc = xml::parse(R"(<?xml version="1.0"?><!DOCTYPE x><!-- c -->
            <a k='v' n="1 &amp; 2"><b>x &lt;y&gt; &#65;&#x42;</b><c/><d><![CDATA[<raw> & ]]></d><!-- in --><?pi x?></a>)");
        QVERIFY2(doc.ok(), doc.error.c_str());
        QCOMPARE(doc.root.name, std::string("a"));
        QCOMPARE(doc.root.attribute("k"), std::string_view("v"));
        QCOMPARE(doc.root.attribute("n"), std::string_view("1 & 2"));
        QCOMPARE(doc.root.childText("b"), std::string_view("x <y> AB"));
        QVERIFY(doc.root.child("c"));
        QCOMPARE(doc.root.childText("d"), std::string_view("<raw> &"));
        QCOMPARE(doc.root.children.size(), std::size_t(3));
        QVERIFY(!xml::parse("<a><b></a>").ok());
        QVERIFY(!xml::parse("<a>").ok());
        QVERIFY(!xml::parse("<a k=v/>").ok());
        QVERIFY(!xml::parse("<a>&bogus;</a>").ok());
        QVERIFY(!xml::parse("").ok());
    }

    void playOrderExpandsRepeats()
    {
        // |: 0 1 :| 2
        QCOMPARE(playOrder({bar(true), bar(false, true, 2), bar()}), (std::vector<int>{0, 1, 0, 1, 2}));
        // 0 |: 1 :|x3 2
        QCOMPARE(playOrder({bar(), bar(true, true, 3), bar()}), (std::vector<int>{0, 1, 1, 1, 2}));
        // |: 0 [1. 1 :| [2. 2  3
        QCOMPARE(playOrder({bar(true), bar(false, true, 2, 0b01), bar(false, false, 0, 0b10), bar()}),
                 (std::vector<int>{0, 1, 0, 2, 3}));
        // A repeat end with no start goes back to the beginning, then to after it.
        QCOMPARE(playOrder({bar(), bar(false, true, 2), bar(), bar(false, true, 2)}),
                 (std::vector<int>{0, 1, 0, 1, 2, 3, 2, 3}));
        // Two sections one after the other.
        QCOMPARE(playOrder({bar(true, true, 2), bar(true, true, 2)}), (std::vector<int>{0, 0, 1, 1}));
        QCOMPARE(playOrder({}), std::vector<int>{});
        // An absurd count is capped.
        QCOMPARE(playOrder({bar(true, true, 1000)}).size(), std::size_t(16));
    }

    void scoreAsWritten()
    {
        const auto result = readTab(gpif(kBars, kTempo));
        QVERIFY2(result.score, result.error.c_str());
        const Score &score = *result.score;
        QCOMPARE(score.title, std::string("Practice Riff"));
        QCOMPARE(score.artist, std::string("Krate & Co"));
        QCOMPARE(score.tracks.size(), std::size_t(2));
        const ScoreTrack &g = score.tracks[0];
        QCOMPARE(g.name, std::string("Guitar"));
        QCOMPARE(g.program, 30);
        QVERIFY(!g.drums);
        QCOMPARE(g.tuning, (std::vector<int>{38, 45, 50, 55, 59, 64}));
        QVERIFY(score.tracks[1].drums);
        QVERIFY(score.tracks[1].tuning.empty());

        QCOMPARE(score.bars.size(), std::size_t(3));
        QCOMPARE(score.bars[1].time.numerator, 3);
        QCOMPARE(score.bars[1].time.tick, Tick(3840));
        QVERIFY(score.bars[0].repeatStart);
        QVERIFY(score.bars[1].repeatEnd);
        QCOMPARE(score.tempos.size(), std::size_t(3));
        QCOMPARE(score.tempos[1].bpm, 30.0);  // 60 eighths a minute
        QCOMPARE(score.tempos[2].bpm, 120.0); // 80 dotted quarters

        // Bar 0: quarter, quarter, dotted eighth, sixteenth rest.
        const auto &beats = g.bars[0].voices.at(0);
        QCOMPARE(beats.size(), std::size_t(4));
        QCOMPARE(beats[1].start, Tick(960));
        QCOMPARE(beats[2].length, Tick(720));
        QCOMPARE(beats[2].dots, 1);
        QCOMPARE(beats[3].start, Tick(2640));
        QVERIFY(beats[3].notes.empty());
        QCOMPARE(beats[0].velocity, 79); // MF
        const ScoreNote &low = beats[0].notes[0];
        QCOMPARE(low.string, 0);
        QCOMPARE(low.pitch, 38);
        QVERIFY(low.techniques & PalmMute);
        QVERIFY(beats[1].notes[0].techniques & Slide);
        QVERIFY(beats[2].notes[0].techniques & TieDestination);

        // Bar 1 (written bar 2): a grace note takes no time; triplet eighths; two voices.
        const ScoreBar &b2 = g.bars[1];
        QCOMPARE(b2.voices.size(), std::size_t(2));
        const auto &v0 = b2.voices[0];
        QCOMPARE(v0[0].start, Tick(0));
        QCOMPARE(v0[1].start, Tick(0)); // after the grace note
        QCOMPARE(v0[1].length, Tick(320));
        QCOMPARE(v0[1].tupletNum, 3);
        QCOMPARE(v0[2].start, Tick(320));
        const ScoreNote &fromTuning = v0[1].notes[0];
        QCOMPARE(fromTuning.pitch, 62); // no Midi property: G string + 7
        QVERIFY(fromTuning.techniques & HammerOn);
        QVERIFY(fromTuning.techniques & Accent);
        QCOMPARE(v0[1].notes[1].pitch, 76);  // pinch harmonic, an octave over the fretted note
        QCOMPARE(v0[2].notes[0].pitch, 83);  // natural harmonic at the 7th fret of the E string: +19
        const ScoreNote &bent = b2.voices[1][0].notes[0];
        QVERIFY(bent.techniques & Bend);
        QVERIFY(bent.techniques & Vibrato);
        QCOMPARE(bent.bend.size(), std::size_t(2));
        QCOMPARE(bent.bend[1].offset, 0.5);
        QCOMPARE(bent.bend[1].semitones, 2.0);

        // Drums share beat 4 four times (ids are shared).
        const auto &drums = score.tracks[1].bars[0].voices.at(0);
        QCOMPARE(drums.size(), std::size_t(4));
        QCOMPARE(drums[3].start, Tick(2880));
        QCOMPARE(drums[3].notes[1].pitch, 42);
        QVERIFY(score.tracks[1].bars[1].voices.empty()); // no bar for drums there
    }

    void songTimeline()
    {
        const auto result = readTab(gpif(kBars, kTempo));
        QVERIFY2(result.song, result.error.c_str());
        const Score &score = *result.score;
        const Song &song = *result.song;
        QCOMPARE(score.playOrder, (std::vector<int>{0, 1, 0, 1, 2}));
        QCOMPARE(score.playStarts, (std::vector<Tick>{0, 3840, 6720, 10560, 13440}));
        QCOMPARE(song.length, Tick(16320));
        QCOMPARE(song.tempo.ppq(), 960);
        // 4/4, 3/4, 4/4 again on the repeat, 3/4...
        QCOMPARE(song.tempo.barAt(6720).time.numerator, 4);
        QCOMPARE(song.tempo.barAt(6720).bar, 2);
        QCOMPARE(song.tempo.barAt(14000).bar, 4);
        // 90 at the start of each pass, 30 halfway through bar 2, 120 from bar 3.
        QCOMPARE(song.tempo.bpmAt(0), 90.0);
        QCOMPARE(song.tempo.bpmAt(3840 + 1440), 30.0);
        QCOMPARE(song.tempo.bpmAt(6720), 90.0);
        QCOMPARE(song.tempo.bpmAt(10560 + 1440), 30.0);
        QCOMPARE(song.tempo.bpmAt(13440), 120.0);
    }

    void songEvents()
    {
        const auto result = readTab(gpif(kBars, kTempo));
        QVERIFY2(result.song, result.error.c_str());
        const Song &song = *result.song;
        QCOMPARE(song.title, std::string("Practice Riff"));
        QCOMPARE(song.tracks.size(), std::size_t(2));
        const Track &g = song.tracks[0];
        QCOMPARE(g.name, std::string("Guitar"));
        QCOMPARE(g.program, 30);

        const auto ons = [](const Track &t, Tick from, Tick to) {
            std::vector<std::tuple<Tick, int, int>> out; // tick, channel, pitch
            for (const Event &e : t.events)
                if (e.type() == 0x90 && e.tick >= from && e.tick < to)
                    out.emplace_back(e.tick, e.channel(), e.data1);
            return out;
        };
        const auto offOf = [](const Track &t, int channel, int pitch, Tick after) {
            for (const Event &e : t.events)
                if (e.type() == 0x80 && e.channel() == channel && e.data1 == pitch && e.tick > after)
                    return e.tick;
            return Tick(-1);
        };
        // Bar 0: the low strings on channels 0 and 1; the tied note is one note, held on.
        QCOMPARE(ons(g, 0, 3840), (std::vector<std::tuple<Tick, int, int>>{{0, 0, 38}, {0, 1, 45}, {960, 1, 50}}));
        QCOMPARE(offOf(g, 1, 50, 960), Tick(960 + 960 + 720));
        // Bar 1 (written 2): grace, triplets, harmonics; voice 2's bend.
        const auto bar1 = ons(g, 3840, 6720);
        QCOMPARE(bar1.size(), std::size_t(5));
        // The dead note in bar 2 is short and soft.
        for (const Event &e : g.events)
            if (e.type() == 0x90 && e.data1 == 41) {
                QVERIFY(e.data2 < 31);
                QCOMPARE(offOf(g, 0, 41, e.tick) - e.tick, Tick(120));
            }
        // The bend: up a whole tone by half the note (bend values for a ±2 semitone range), back at its end.
        std::vector<std::pair<Tick, int>> bends;
        for (const Event &e : g.events)
            if (e.type() == 0xE0 && e.tick <= 6720) // the first pass
                bends.emplace_back(e.tick, (e.data2 << 7) | e.data1);
        QCOMPARE(bends.size(), std::size_t(3));
        QCOMPARE(bends[0], (std::pair<Tick, int>{3840, 8192}));
        QCOMPARE(bends[1], (std::pair<Tick, int>{3840 + 1440, 16383}));
        QCOMPARE(bends[2], (std::pair<Tick, int>{3840 + 2880, 8192}));
        QCOMPARE(int(g.events[0].status & 0x0F), 0);
        // Drums on channel 10, twice through the repeat and once at the end.
        const Track &d = song.tracks[1];
        QVERIFY(d.drums);
        QCOMPARE(d.noteCount, 2 * 4 * 2 + 2 * 4);
        for (const Event &e : d.events)
            QCOMPARE(e.channel(), kDrumChannel);
    }

    void errors()
    {
        const Bytes notZip{1, 2, 3, 4, 5};
        QVERIFY(!readGpFile(std::span<const std::uint8_t>(notZip)).score);
        const Bytes noScore = makeZip({{"VERSION", "8"}});
        QCOMPARE(readGpFile(std::span<const std::uint8_t>(noScore)).error, std::string("the tab file has no score in it"));
        QVERIFY(!readTab("<GPIF><Tracks>").score);
        QVERIFY(!readTab("<Other/>").score);
        QVERIFY(!readTab("<GPIF><Tracks/></GPIF>").score);
        QVERIFY(readTab(gpif(kBars), false).score); // stored works too; no tempo marks: 120
        QCOMPARE(readTab(gpif(kBars), false).song->tempo.bpmAt(0), 120.0);
    }

    void readsAFile()
    {
        QTemporaryDir dir;
        const std::string path = dir.filePath(QStringLiteral("riff.gp")).toStdString();
        const Bytes zip = makeZip({{"Content/score.gpif", gpif(kBars)}});
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(zip.data()), std::streamsize(zip.size()));
        const auto result = readGpFile(path);
        QVERIFY2(result.score, result.error.c_str());
        QVERIFY(!readGpFile(dir.filePath(QStringLiteral("missing.gp")).toStdString()).score);
    }
};

QTEST_GUILESS_MAIN(TestGpFile)
#include "tst_gpfile.moc"
