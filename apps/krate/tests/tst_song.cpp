#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <span>
#include <string>
#include <vector>

#include "RackController.h"
#include "SongController.h"
#include "app/InstrumentRegistry.h"
#include "core/Instrument.h"
#include "core/Transport.h"
#include "rack/Rack.h"
#include "song/SongPlayer.h"

using namespace krate;

namespace {

using Bytes = std::vector<std::uint8_t>;

class Fake final : public IInstrument
{
public:
    void prepare(double, std::uint32_t) noexcept override {}
    std::span<const ParamDescriptor> parameters() const noexcept override { return {}; }
    float param(int) const noexcept override { return 0.0f; }
    void setParam(int, float) noexcept override {}
    void setParamFromHost(int, float) noexcept override {}
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void process(const AudioBlock &b) noexcept override
    {
        std::fill_n(b.outputs[0], b.frames, 0.0f);
        std::fill_n(b.outputs[1], b.frames, 0.0f);
    }
    void setSequencerEnabled(bool on) noexcept override { sequencer = on; }
    bool sequencer = true;
};

void be(Bytes &out, std::uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; --i)
        out.push_back(std::uint8_t(v >> (8 * i)));
}

// A format 1 MIDI file: one track per (name, channel, first note), 480 ppq,
// four quarter notes each.
Bytes midiFile(const std::vector<std::tuple<std::string, int, int>> &tracks)
{
    Bytes out{'M', 'T', 'h', 'd'};
    be(out, 6, 4);
    be(out, 1, 2);
    be(out, std::uint32_t(tracks.size()), 2);
    be(out, 480, 2);
    for (const auto &[name, channel, note] : tracks) {
        Bytes body{0, 0xFF, 0x03, std::uint8_t(name.size())};
        body.insert(body.end(), name.begin(), name.end());
        for (int i = 0; i < 4; ++i) {
            const std::uint8_t on = std::uint8_t(0x90 | channel), off = std::uint8_t(0x80 | channel);
            body.insert(body.end(), {0, on, std::uint8_t(note + i), 100, 0x83, 0x60, off, std::uint8_t(note + i), 0});
        }
        body.insert(body.end(), {0, 0xFF, 0x2F, 0});
        out.insert(out.end(), {'M', 'T', 'r', 'k'});
        be(out, std::uint32_t(body.size()), 4);
        out.insert(out.end(), body.begin(), body.end());
    }
    return out;
}

void le(Bytes &o, std::uint32_t v, int n)
{
    for (int i = 0; i < n; ++i)
        o.push_back(std::uint8_t(v >> (8 * i)));
}

// A .gp file (stored zip) with a guitar and a bass, one 4/4 bar each.
Bytes tabFile()
{
    const std::string gpif = R"(<GPIF><Score><Title><![CDATA[Practice Riff]]></Title><Artist><![CDATA[Krate]]></Artist></Score>
<MasterTrack><Automations><Automation><Type>Tempo</Type><Bar>0</Bar><Position>0</Position><Value>100 2</Value></Automation></Automations></MasterTrack>
<Tracks>
<Track id="0"><Name>Guitar</Name><InstrumentSet><Name>Electric Guitar</Name><Type>electricGuitar</Type></InstrumentSet>
<Staves><Staff><Properties><Property name="Tuning"><Pitches>40 45 50 55 59 64</Pitches></Property></Properties></Staff></Staves></Track>
<Track id="1"><Name>Bass</Name><InstrumentSet><Name>Electric Bass</Name><Type>electricBass</Type></InstrumentSet>
<Staves><Staff><Properties><Property name="Tuning"><Pitches>28 33 38 43</Pitches></Property></Properties></Staff></Staves></Track>
</Tracks>
<MasterBars><MasterBar><Time>4/4</Time><Bars>0 1</Bars></MasterBar></MasterBars>
<Bars><Bar id="0"><Voices>0 -1 -1 -1</Voices></Bar><Bar id="1"><Voices>1 -1 -1 -1</Voices></Bar></Bars>
<Voices><Voice id="0"><Beats>0</Beats></Voice><Voice id="1"><Beats>1 1</Beats></Voice></Voices>
<Beats><Beat id="0"><Rhythm ref="0"/><Notes>0</Notes></Beat><Beat id="1"><Rhythm ref="1"/><Notes>1</Notes></Beat></Beats>
<Notes><Note id="0"><Properties><Property name="String"><String>5</String></Property><Property name="Fret"><Fret>0</Fret></Property></Properties></Note>
<Note id="1"><Properties><Property name="String"><String>0</String></Property><Property name="Fret"><Fret>0</Fret></Property></Properties></Note></Notes>
<Rhythms><Rhythm id="0"><NoteValue>Whole</NoteValue></Rhythm><Rhythm id="1"><NoteValue>Half</NoteValue></Rhythm></Rhythms>
</GPIF>)";
    const std::string name = "Content/score.gpif";
    Bytes o;
    le(o, 0x04034b50, 4);
    le(o, 20, 2);
    le(o, 0, 2);
    le(o, 0, 2);
    le(o, 0, 4);
    le(o, 0, 4);
    le(o, std::uint32_t(gpif.size()), 4);
    le(o, std::uint32_t(gpif.size()), 4);
    le(o, std::uint32_t(name.size()), 2);
    le(o, 0, 2);
    o.insert(o.end(), name.begin(), name.end());
    o.insert(o.end(), gpif.begin(), gpif.end());
    const auto dir = std::uint32_t(o.size());
    le(o, 0x02014b50, 4);
    le(o, 20, 2);
    le(o, 20, 2);
    le(o, 0, 2);
    le(o, 0, 2);
    le(o, 0, 4);
    le(o, 0, 4);
    le(o, std::uint32_t(gpif.size()), 4);
    le(o, std::uint32_t(gpif.size()), 4);
    le(o, std::uint32_t(name.size()), 2);
    for (int i = 0; i < 4; ++i)
        le(o, 0, 2);
    le(o, 0, 4);
    le(o, 0, 4);
    o.insert(o.end(), name.begin(), name.end());
    const auto dirSize = std::uint32_t(o.size()) - dir;
    le(o, 0x06054b50, 4);
    le(o, 0, 2);
    le(o, 0, 2);
    le(o, 1, 2);
    le(o, 1, 2);
    le(o, dirSize, 4);
    le(o, dir, 4);
    le(o, 0, 2);
    return o;
}

void write(const QString &path, const Bytes &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(reinterpret_cast<const char *>(data.data()), qint64(data.size()));
}

InstrumentEntry entry(const char *id, const char *name, int channel, QList<int> extra = {})
{
    InstrumentEntry e;
    e.id = QString::fromLatin1(id);
    e.name = QString::fromLatin1(name);
    e.defaultMidiChannel = channel;
    e.extraMidiChannels = extra;
    return e;
}

// A rack like the app's: k303, K909, Kalliope, Krimson (4 slots), Klick.
struct Fixture
{
    QTemporaryDir dir;
    std::array<Fake, 5> fakes;
    Rack rack;
    Transport transport;
    QList<InstrumentEntry> entries{entry("k303", "k303", -1), entry("k909", "K909", 9), entry("kalliope", "Kalliope", 1),
                                   entry("krimson", "Krimson", 2, {3, 4, 5}), entry("klick", "Klick", -1)};
    song::SongPlayer player;
    std::unique_ptr<RackController> rackController;
    std::unique_ptr<SongController> song;

    Fixture()
    {
        for (Fake &f : fakes)
            rack.addDevice(f);
        rackController = std::make_unique<RackController>(rack, transport, entries, nullptr);
        make();
    }
    void make()
    {
        song.reset();
        std::vector<IInstrument *> instruments;
        for (Fake &f : fakes)
            instruments.push_back(&f);
        song = std::make_unique<SongController>(player, *rackController, entries, instruments, settings(), nullptr);
    }
    QString settings() const { return dir.filePath(QStringLiteral("songs.json")); }
    QString path(const char *name) const { return dir.filePath(QString::fromLatin1(name)); }
    QVariantMap track(int t) const { return song->tracks().value(t).toMap(); }
    int choice(const char *label) const { return int(song->instrumentChoices().indexOf(QString::fromLatin1(label))); }
};

} // namespace

class TestSong : public QObject
{
    Q_OBJECT

private slots:
    void instrumentChoices()
    {
        Fixture f;
        QCOMPARE(f.song->instrumentChoices(), (QStringList{"—", "k303", "K909", "Kalliope", "Krimson A", "Krimson B",
                                                         "Krimson C", "Krimson D"}));
        QCOMPARE(f.song->choices()[1].device, 1);
        QVERIFY(f.song->choices()[1].drumMap);   // K909
        QCOMPARE(f.song->choices()[5].channel, 4); // Krimson C on channel 5
        QCOMPARE(f.song->choices()[0].channel, 0); // k303 (no channel of its own)
    }

    void opensAMidiFileAndSuggests()
    {
        Fixture f;
        write(f.path("band.mid"), midiFile({{"Lead Guitar", 0, 64}, {"Bass", 1, 33}, {"Drums", 9, 36}, {"Pad", 2, 60}}));
        f.song->openPaths({f.path("band.mid")});
        QVERIFY(f.song->loaded());
        QVERIFY(f.song->hasMidi());
        QVERIFY(!f.song->hasTab());
        QCOMPARE(f.song->title(), QStringLiteral("band"));
        QCOMPARE(f.song->tracks().size(), 4);
        QCOMPARE(f.track(0).value("choice").toInt(), 0); // the guitar is yours
        QCOMPARE(f.song->myTrack(), 0);
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("k303"));
        QCOMPARE(f.track(2).value("choice").toInt(), f.choice("K909"));
        QCOMPARE(f.track(3).value("choice").toInt(), f.choice("Kalliope"));
        QCOMPARE(f.player.assignment(2), (song::Assignment{1, 9, true}));
        QCOMPARE(f.player.assignment(0), song::Assignment{});
        // The instruments' own sequencers are off while a song is open.
        for (const Fake &fake : f.fakes)
            QVERIFY(!fake.sequencer);
        f.song->close();
        QVERIFY(!f.song->loaded());
        for (const Fake &fake : f.fakes)
            QVERIFY(fake.sequencer);
        QCOMPARE(f.player.assignment(1), song::Assignment{});
    }

    void tabFindsItsMidiFile()
    {
        Fixture f;
        write(f.path("riff.gp"), tabFile());
        write(f.path("riff.mid"), midiFile({{"Guitar", 0, 64}, {"Bass", 1, 28}}));
        f.song->open({QUrl::fromLocalFile(f.path("riff.gp"))});
        QVERIFY(f.song->hasTab());
        QVERIFY(f.song->hasMidi());
        QCOMPARE(f.song->title(), QStringLiteral("Practice Riff"));
        QCOMPARE(f.song->artist(), QStringLiteral("Krate"));
        QCOMPARE(f.song->files(), QStringLiteral("riff.gp + riff.mid"));
        QCOMPARE(f.song->barCount(), 1);
        QCOMPARE(f.track(0).value("tuning").toString(), QStringLiteral("E A D G B E"));
        QCOMPARE(f.track(1).value("strings").toInt(), 4);
        // Both tracks line up: they play from the MIDI file (four notes each), on the tab's timeline.
        QVERIFY(f.track(1).value("canMidi").toBool());
        QVERIFY(f.track(1).value("fromMidi").toBool());
        QCOMPARE(f.song->song()->tracks[1].noteCount, 4);
        QCOMPARE(f.song->song()->tempo.ppq(), 960);
        QCOMPARE(f.song->song()->tracks[1].events[1].tick, song::Tick(960)); // a MIDI quarter, in the tab's ticks
        // A click on a bar in the tab view cues it; a bar that isn't there does nothing.
        f.song->jumpToBar(0);
        f.song->jumpToBar(7);
        QVERIFY(f.song->score());
        QCOMPARE(f.song->score()->playStarts.front(), song::Tick(0));
        // From the tab instead: its two half notes.
        f.song->setFromMidi(1, false);
        QCOMPARE(f.song->song()->tracks[1].noteCount, 2);
        QVERIFY(!f.track(1).value("fromMidi").toBool());
    }

    void settingsAreRemembered()
    {
        Fixture f;
        write(f.path("band.mid"), midiFile({{"Lead Guitar", 0, 64}, {"Bass", 1, 33}, {"Drums", 9, 36}}));
        f.song->openPaths({f.path("band.mid")});
        f.song->setInstrument(1, f.choice("Krimson D"));
        f.song->setLevel(2, 0.25);
        f.song->setMute(2, true);
        f.song->setSolo(1, true);
        f.song->setMyTrack(1);
        QCOMPARE(f.player.assignment(1), (song::Assignment{3, 5, false}));
        QCOMPARE(f.player.level(2), 0.25f);
        QVERIFY(f.player.mute(2));
        // A new rack session: the last song reopens as it was left.
        f.make();
        QVERIFY(!f.song->loaded());
        f.song->reopenLast();
        QVERIFY(f.song->loaded());
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("Krimson D"));
        QCOMPARE(f.track(2).value("level").toDouble(), 0.25);
        QVERIFY(f.track(2).value("mute").toBool());
        QVERIFY(f.track(1).value("solo").toBool());
        QCOMPARE(f.song->myTrack(), 1);
        QVERIFY(f.player.solo(1));
        // Closed: nothing reopens next time, but the song's settings stay for when it's opened again.
        f.song->close();
        f.make();
        f.song->reopenLast();
        QVERIFY(!f.song->loaded());
        f.song->openPaths({f.path("band.mid")});
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("Krimson D"));
    }

    void badFilesKeepTheSong()
    {
        Fixture f;
        write(f.path("band.mid"), midiFile({{"Bass", 1, 33}}));
        f.song->openPaths({f.path("band.mid")});
        write(f.path("broken.gp"), Bytes{1, 2, 3});
        QSignalSpy errors(f.song.get(), &SongController::errorChanged);
        f.song->openPaths({f.path("broken.gp")});
        QCOMPARE(errors.count(), 1);
        QVERIFY(f.song->error().contains(QStringLiteral("broken.gp")));
        QCOMPARE(f.song->files(), QStringLiteral("band.mid")); // still open
        f.song->openPaths({f.path("notes.txt")});
        QVERIFY(!f.song->error().isEmpty());
        QVERIFY(f.song->loaded());
    }

    void theEndStopsTheRack()
    {
        Fixture f;
        write(f.path("band.mid"), midiFile({{"Bass", 1, 33}}));
        f.song->openPaths({f.path("band.mid")});
        f.rackController->setPlaying(true);
        std::array<DeviceEvent, 256> out{};
        for (int i = 0; i < 2000 && !f.player.ended(); ++i) { // the audio thread's part
            f.player.beforeTransport(f.transport);
            const TransportState state = f.transport.advance(512, 48000.0);
            f.player.render(state, 512, 48000.0, out.data(), std::uint32_t(out.size()));
        }
        QVERIFY(f.player.ended());
        f.song->pollPosition();
        QVERIFY(!f.rackController->playing());
        // Back at the top once the audio thread has taken the cue.
        f.player.beforeTransport(f.transport);
        f.player.render(f.transport.advance(512, 48000.0), 512, 48000.0, out.data(), std::uint32_t(out.size()));
        f.song->pollPosition();
        QCOMPARE(f.song->bar(), 1);
    }
};

QTEST_GUILESS_MAIN(TestSong)
#include "tst_song.moc"
