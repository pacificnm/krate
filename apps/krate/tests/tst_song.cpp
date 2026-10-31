#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <span>
#include <string>
#include <vector>

#include "RackController.h"
#include "SequencerController.h"
#include "SongController.h"
#include "app/InstrumentRegistry.h"
#include "core/Instrument.h"
#include "core/Transport.h"
#include "rack/Rack.h"
#include "song/SongPlayer.h"

using namespace krate;

namespace {

using Bytes = std::vector<std::uint8_t>;

std::string fmt(float v) { return std::to_string(v); }
constexpr ParamDescriptor kFakeParams[] = {{"tone", "Tone", 0.5f, 0, &fmt}};

// An instrument with one knob ("tone"), and its own sequencer switch.
class Fake final : public IInstrument
{
public:
    void prepare(double, std::uint32_t) noexcept override {}
    std::span<const ParamDescriptor> parameters() const noexcept override { return kFakeParams; }
    float param(int) const noexcept override { return tone; }
    void setParam(int, float v) noexcept override { tone = v; }
    void setParamFromHost(int, float v) noexcept override { tone = v; }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    void process(const AudioBlock &b) noexcept override
    {
        std::fill_n(b.outputs[0], b.frames, 0.0f);
        std::fill_n(b.outputs[1], b.frames, 0.0f);
    }
    void setSequencerSuspended(bool on) noexcept override { sequencer = !on; }
    bool sequencer = true;
    float tone = 0.5f;
};

// Its instance: the default whole-sound save and load (every parameter).
class FakeInstance final : public InstrumentInstance
{
public:
    explicit FakeInstance(Fake &f) : fake(f) {}
    IInstrument &instrument() override { return fake; }
    Fake &fake;
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

InstrumentEntry entry(const char *id, const char *name, int channel, QList<int> extra = {}, bool playsNotes = true)
{
    InstrumentEntry e;
    e.playsNotes = playsNotes;
    e.id = QString::fromLatin1(id);
    e.name = QString::fromLatin1(name);
    e.panelComponent = QStringLiteral("Panel");
    e.defaultMidiChannel = channel;
    e.extraMidiChannels = extra;
    return e;
}

// A rack like the app's: Korrode, Kessel, Kalliope, Krimson (4 slots), Klick.
struct Fixture
{
    QTemporaryDir dir;
    std::array<Fake, 5> fakes;
    std::vector<std::unique_ptr<FakeInstance>> instances;
    Rack rack;
    Transport transport;
    QList<InstrumentEntry> entries{entry("korrode", "Korrode", -1), entry("kessel", "Kessel", 9), entry("kalliope", "Kalliope", 1),
                                   entry("krimson", "Krimson", 2, {3, 4, 5}), entry("klick", "Klick", -1, {}, false)};
    song::SongPlayer player;
    std::unique_ptr<RackController> rackController;
    std::unique_ptr<SongLibrary> library;
    std::unique_ptr<SongController> song;

    Fixture()
    {
        for (Fake &f : fakes) {
            rack.addDevice(f);
            instances.push_back(std::make_unique<FakeInstance>(f));
        }
        rackController = std::make_unique<RackController>(rack, transport, entries, nullptr);
        make();
    }
    // A fresh controller and library connection, as when the rack starts again.
    void make()
    {
        song.reset();
        library.reset();
        library = std::make_unique<SongLibrary>(dir.filePath(QStringLiteral("library")));
        std::vector<InstrumentInstance *> list;
        for (const auto &i : instances)
            list.push_back(i.get());
        song = std::make_unique<SongController>(player, *rackController, entries, list, *library, nullptr);
    }
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
        QCOMPARE(f.song->instrumentChoices(), (QStringList{"—", "Korrode", "Kessel", "Kalliope", "Krimson A", "Krimson B",
                                                         "Krimson C", "Krimson D"}));
        QCOMPARE(f.song->choices()[1].device, 1);
        QVERIFY(f.song->choices()[1].drumMap);   // Kessel
        QCOMPARE(f.song->choices()[5].channel, 4); // Krimson C on channel 5
        QCOMPARE(f.song->choices()[0].channel, 0); // Korrode (no channel of its own)
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
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("Korrode"));
        QCOMPARE(f.track(2).value("choice").toInt(), f.choice("Kessel"));
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
        QCOMPARE(f.song->files(), QStringLiteral("Practice Riff.gp + Practice Riff.mid")); // the library's copies
        QVERIFY(f.song->songId() > 0);
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
        // The view follows a clicked track; picking "my track" brings it into view too.
        QCOMPARE(f.song->viewTrack(), 0); // the guitar is yours, and in view
        f.song->setViewTrack(1);
        QVERIFY(f.track(1).value("viewed").toBool());
        QVERIFY(!f.track(0).value("viewed").toBool());
        QVERIFY(f.track(1).value("hasTab").toBool());
        f.song->setMyTrack(0);
        QCOMPARE(f.song->viewTrack(), 1); // already yours: nothing moves
        f.song->setMyTrack(1);
        QCOMPARE(f.song->viewTrack(), 1);
        f.song->setViewTrack(9); // no such track
        QCOMPARE(f.song->viewTrack(), 1);
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

    // S4's songs.json comes into the library once, with its last song's setup.
    void importsSongsJson()
    {
        Fixture f;
        write(f.path("band.mid"), midiFile({{"Lead Guitar", 0, 64}, {"Bass", 1, 33}}));
        const QString json = f.path("songs.json");
        QFile out(json);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QJsonDocument(QJsonObject{
                                    {"last", QJsonObject{{"tab", ""}, {"midi", f.path("band.mid")}}},
                                    {"songs", QJsonObject{{f.path("band.mid"),
                                                           QJsonObject{{"myTrack", 1},
                                                                       {"tracks", QJsonArray{QJsonObject{{"instrument", "kalliope"}},
                                                                                             QJsonObject{{"instrument", "krimson:C"},
                                                                                                         {"level", 0.4}}}}}}}},
                                })
                      .toJson());
        out.close();
        f.song->importSongsJson(json);
        QVERIFY(f.song->loaded());
        QVERIFY(f.song->songId() > 0);
        QCOMPARE(f.track(0).value("choice").toInt(), f.choice("Kalliope"));
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("Krimson C"));
        QCOMPARE(f.track(1).value("level").toDouble(), 0.4);
        QCOMPARE(f.song->myTrack(), 1);
        QVERIFY(!QFile::exists(json));
        QVERIFY(QFile::exists(json + ".old"));
        // Next start: from the library.
        f.make();
        f.song->reopenLast();
        QCOMPARE(f.track(1).value("choice").toInt(), f.choice("Krimson C"));
    }

    // SAVE keeps the instruments' sounds and the mixer with the song; opening
    // it puts them on, closing (or another song, or quitting) puts yours back.
    void soundsComeAndGo()
    {
        Fixture f;
        write(f.path("a.mid"), midiFile({{"Bass", 1, 33}}));
        write(f.path("b.mid"), midiFile({{"Drums", 9, 36}}));
        Fake &krimson = f.fakes[3];
        RackController &mixer = *f.rackController;
        krimson.tone = 0.2f; // your sound, before any song
        mixer.setFader(3, 0.6);

        f.song->openPaths({f.path("a.mid")});
        QVERIFY(!f.song->soundsSaved());
        krimson.tone = 0.9f; // the song's sound
        mixer.setFader(3, 0.3);
        mixer.setPan(3, -0.5);
        f.song->saveSounds();
        QVERIFY(f.song->soundsSaved());
        krimson.tone = 0.7f; // experimenting, not saved

        f.song->close();
        QCOMPARE(krimson.tone, 0.2f); // yours again
        QCOMPARE(mixer.fader(3), 0.6);
        QCOMPARE(mixer.pan(3), 0.0);

        f.song->openPaths({f.path("a.mid")});
        QVERIFY(f.song->soundsSaved());
        QCOMPARE(krimson.tone, 0.9f); // the song's, as saved
        QCOMPARE(mixer.fader(3), 0.3);
        QCOMPARE(mixer.pan(3), -0.5);

        f.song->openPaths({f.path("b.mid")}); // another song, without saved sounds: yours
        QVERIFY(!f.song->soundsSaved());
        QCOMPARE(krimson.tone, 0.2f);
        QCOMPARE(mixer.fader(3), 0.6);

        f.song->openPaths({f.path("a.mid")});
        QCOMPARE(krimson.tone, 0.9f);
        f.make(); // quitting with the song open: yours come back on the way out
        QCOMPARE(krimson.tone, 0.2f);
        QCOMPARE(mixer.fader(3), 0.6);
        // ...and the song (reopening next time) still has its own.
        f.song->reopenLast();
        QCOMPARE(krimson.tone, 0.9f);
    }

    // A song only takes back what it changed: an instrument it has no sound for
    // (one added to the rack after the song was saved) keeps your changes.
    void songsLeaveOtherInstrumentsAlone()
    {
        Fixture f;
        write(f.path("a.mid"), midiFile({{"Bass", 1, 33}}));
        Fake &krimson = f.fakes[3], &newcomer = f.fakes[4];
        f.song->openPaths({f.path("a.mid")});
        krimson.tone = 0.9f;
        f.song->saveSounds();
        // As if the song had been saved before `newcomer` existed: no sound for it.
        QJsonObject sounds = f.library->sounds(f.song->songId());
        sounds.remove(QStringLiteral("klick"));
        f.library->saveSounds(f.song->songId(), sounds, f.library->mixer(f.song->songId()));
        f.song->close();
        krimson.tone = 0.2f;
        newcomer.tone = 0.1f;

        f.song->openPaths({f.path("a.mid")});
        QCOMPARE(krimson.tone, 0.9f);
        newcomer.tone = 0.6f; // e.g. picking a cabinet with the song open
        f.song->close();
        QCOMPARE(krimson.tone, 0.2f);  // the song's goes back
        QCOMPARE(newcomer.tone, 0.6f); // yours stays

        f.song->openPaths({f.path("a.mid")});
        newcomer.tone = 0.7f;
        f.make(); // quitting with the song open
        QCOMPARE(krimson.tone, 0.2f);
        QCOMPARE(newcomer.tone, 0.7f);
    }

    // The library list: artists, albums, songs in track order; editing and removing.
    void libraryList()
    {
        Fixture f;
        write(f.path("Zed_Band-Second_Song-01-02-2026.mid"), midiFile({{"Bass", 1, 33}}));
        write(f.path("Alpha-First.mid"), midiFile({{"Drums", 9, 36}}));
        f.song->openPaths({f.path("Zed_Band-Second_Song-01-02-2026.mid")});
        QCOMPARE(f.song->artist(), QStringLiteral("Zed Band"));
        QCOMPARE(f.song->title(), QStringLiteral("Second Song"));
        f.song->setDetails("Zed Band", "Demo Tape", "Second Song", 2);
        f.song->openPaths({f.path("Alpha-First.mid")});
        f.song->saveSounds();
        QSignalSpy changed(f.song.get(), &SongController::libraryChanged);
        const QVariantList rows = f.song->libraryRows({});
        QStringList shape;
        for (const QVariant &r : rows)
            shape << r.toMap().value("kind").toString() + ":" + r.toMap().value("text").toString();
        QCOMPARE(shape, (QStringList{"artist:Alpha", "album:Unknown album", "song:First", "artist:Zed Band", "album:Demo Tape",
                                     "song:Second Song"}));
        const QVariantMap first = rows[2].toMap(), second = rows[5].toMap();
        QVERIFY(first.value("open").toBool());
        QVERIFY(first.value("soundsSaved").toBool());
        QCOMPARE(second.value("track").toInt(), 2);
        QVERIFY(!second.value("open").toBool());
        QCOMPARE(f.song->libraryRows("demo").size(), 3); // artist, album, song
        // Click a song: it opens.
        f.song->openSong(second.value("id").toLongLong());
        QCOMPARE(f.song->title(), QStringLiteral("Second Song"));
        QCOMPARE(f.song->album(), QStringLiteral("Demo Tape"));
        // Remove the open song: closed, gone, its files too.
        const QString file = f.song->files();
        f.song->removeFromLibrary(f.song->songId());
        QVERIFY(!f.song->loaded());
        QCOMPARE(f.song->libraryRows({}).size(), 3);
        QVERIFY(changed.count() >= 2);
        QVERIFY(!file.isEmpty());
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

    // Saved sequences (docs/SEQUENCER.md#saved-sequences): SAVE AS keeps the
    // sequencer and every sound; edits mark it changed until SAVE; opening one
    // puts its notes and sounds back; CLOSE goes back to what was there before
    // the first one opened; rename, delete; still there after a restart.
    void savedSequences()
    {
        Fixture f;
        krate::seq::Sequencer seq;
        SequencerController c(seq, noteTargets(f.entries), f.path("sequencer.json"));
        c.load();
        std::vector<InstrumentInstance *> list;
        for (const auto &i : f.instances)
            list.push_back(i.get());
        QStringList ids;
        for (const auto &e : f.entries)
            ids << e.id;
        const RackSounds sounds(*f.rackController, ids, list);
        c.setLibrary(f.library.get(), &sounds);

        f.fakes[0].tone = 0.2f;
        c.setTarget(0, 1); // Korrode
        c.addNote(0, 40, 24, 100);
        QVERIFY(c.modified());
        QVERIFY(!c.saveSequence()); // none open yet: it needs a name
        QVERIFY(!c.saveSequenceAs(QStringLiteral("  ")));
        QVERIFY(c.saveSequenceAs(QStringLiteral("Groove")));
        const int id = c.sequenceId();
        QVERIFY(id > 0);
        QCOMPARE(c.sequenceName(), QStringLiteral("Groove"));
        QVERIFY(!c.modified());
        QCOMPARE(c.savedSequences().size(), 1);

        c.addNote(48, 43, 24, 100);
        QVERIFY(c.modified());
        c.setPattern(5); // choosing a pattern is no change
        c.setPattern(0);
        QVERIFY(c.saveSequence());
        QVERIFY(!c.modified());

        // Something else now: other notes, another sound.
        c.closeSequence(); // never opened from the library: just let go
        QCOMPARE(c.sequenceId(), 0);
        c.clearPattern();
        f.fakes[0].tone = 0.9f;
        QVERIFY(c.openSequence(id));
        QCOMPARE(c.sequenceName(), QStringLiteral("Groove"));
        QCOMPARE(c.notes().size(), 2);
        QCOMPARE(f.fakes[0].tone, 0.2f); // its sound
        QVERIFY(!c.modified());

        c.closeSequence(); // back to before it opened
        QCOMPARE(c.sequenceId(), 0);
        QCOMPARE(c.notes().size(), 0);
        QCOMPARE(f.fakes[0].tone, 0.9f);

        QVERIFY(c.renameSequence(id, QStringLiteral("Groove 2")));
        f.make(); // the rack starts again
        c.setLibrary(f.library.get(), &sounds);
        QCOMPARE(c.savedSequences().size(), 1);
        QCOMPARE(c.savedSequences()[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Groove 2"));
        QVERIFY(c.openSequence(id));
        QVERIFY(c.deleteSequence(id));
        QCOMPARE(c.sequenceId(), 0);
        QVERIFY(c.modified()); // still on screen, saved nowhere
        QCOMPARE(c.notes().size(), 2);
        QCOMPARE(c.savedSequences().size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestSong)
#include "tst_song.moc"
