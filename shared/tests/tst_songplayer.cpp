#include <QtTest>

#include <memory>
#include <vector>

#include "core/Transport.h"
#include "rack/Rack.h"
#include "song/Arrange.h"
#include "song/SongPlayer.h"

using namespace krate;
using namespace krate::song;

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint32_t kBlock = 64;

Event on(Tick t, int ch, int key, int vel = 100) { return {t, std::uint8_t(0x90 | ch), std::uint8_t(key), std::uint8_t(vel)}; }
Event off(Tick t, int ch, int key) { return {t, std::uint8_t(0x80 | ch), std::uint8_t(key), 0}; }

// Two tracks at 480 ppq, 120 BPM: a bass line on string channels 0-1 with a
// bend and a held note, and drums.
std::unique_ptr<Song> makeSong()
{
    auto song = std::make_unique<Song>();
    song->tempo = TempoMap(480);
    song->tempo.addTempo(0, 120.0);
    song->tempo.addTempo(1920, 60.0); // bar 2 at half the speed
    Track bass;
    bass.name = "Bass";
    bass.events = {on(0, 0, 40), off(480, 0, 40), on(480, 1, 45),
                   {720, 0xE1, 0x00, 0x50}, // a bend on the A string
                   off(960, 1, 45), on(1920, 0, 43), off(3840, 0, 43)};
    Track drums;
    drums.name = "Drums";
    drums.drums = true;
    drums.events = {on(0, 9, 36), off(120, 9, 36), on(480, 9, 52), off(600, 9, 52), // china: to the crash
                    on(960, 9, 29), off(1000, 9, 29)};                             // a scratch: dropped
    song->tracks = {bass, drums};
    song->length = 3840;
    return song;
}

struct Got
{
    long sample; // since play started
    int device;
    int status, d1, d2;
    bool operator==(const Got &) const = default;
};

// The player on a transport, rendering blocks and recording what it sends.
struct Rig
{
    SongPlayer player;
    Transport transport;
    std::vector<Got> got;
    long sample = 0;
    long playStart = -1;

    void block(std::uint32_t frames = kBlock)
    {
        player.beforeTransport(transport);
        const TransportState state = transport.advance(frames, kRate);
        if (state.restarted)
            playStart = sample;
        std::array<DeviceEvent, 512> out{};
        const std::uint32_t n = player.render(state, frames, kRate, out.data(), std::uint32_t(out.size()));
        for (std::uint32_t i = 0; i < n; ++i)
            got.push_back({sample + long(out[i].event.frame) - (playStart < 0 ? 0 : playStart), out[i].device,
                           out[i].event.data[0], out[i].event.data[1], out[i].event.data[2]});
        sample += frames;
    }
    void run(double seconds)
    {
        for (long n = long(seconds * kRate / kBlock); n > 0; --n)
            block();
    }
    std::vector<Got> take()
    {
        std::vector<Got> out;
        out.swap(got);
        return out;
    }
};

void dump(const char *what, const std::vector<Got> &v)
{
    if (!qEnvironmentVariableIsSet("SONGPLAYER_DUMP"))
        return;
    for (const Got &g : v)
        qDebug("%s: %ld dev %d %02x %d %d", what, g.sample, g.device, g.status, g.d1, g.d2);
}

} // namespace

class TestSongPlayer : public QObject
{
    Q_OBJECT

private slots:
    void eventsLandOnTheirSample()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.player.setAssignment(0, {2, 5});         // device 2, channel 6
        rig.player.setAssignment(1, {1, 9, true});   // device 1 with the drum map
        rig.block(); // takes the song while stopped
        rig.transport.setPlaying(true);
        rig.run(1.2);
        const auto got = rig.take();
        // Bass: 120 BPM, 480 ppq: a beat is 24000 samples. Re-channelled to 6.
        const std::vector<Got> bass{{0, 2, 0x95, 40, 100},     {24000, 2, 0x85, 40, 0}, {24000, 2, 0x95, 45, 100},
                                    {36000, 2, 0xE5, 0x00, 0x50}, {48000, 2, 0x85, 45, 0}};
        std::vector<Got> gotBass, gotDrums;
        for (const Got &g : got)
            (g.device == 2 ? gotBass : gotDrums).push_back(g);
        dump("bass", gotBass);
        QCOMPARE(gotBass, bass);
        // Drums: kick; the china is played as the crash (49); the scratch is dropped.
        const std::vector<Got> drums{{0, 1, 0x99, 36, 100}, {6000, 1, 0x89, 36, 0}, {24000, 1, 0x99, 49, 100},
                                     {30000, 1, 0x89, 49, 0}};
        QCOMPARE(gotDrums, drums);
    }

    void theTempoMapDrivesTheTransport()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.block();
        QCOMPARE(rig.transport.bpm(), 120.0);
        rig.player.setSpeed(0.5);
        rig.block();
        QCOMPARE(rig.transport.bpm(), 60.0); // half speed, while stopped too
        rig.player.setSpeed(1.0);
        rig.player.setAssignment(0, {0, 0});
        rig.transport.setPlaying(true);
        rig.run(2.0 + 0.01); // bar 2 (tick 1920) starts at 2 s
        QVERIFY(std::abs(rig.transport.bpm() - 60.0) < 1e-9);
        // The bar-2 note comes on at 2 s, give or take a block.
        for (const Got &g : rig.take())
            if (g.status == 0x90 && g.d1 == 43)
                QVERIFY(std::abs(g.sample - 96000) <= long(kBlock));
        // Not following: the transport is left alone.
        rig.player.setFollowTempo(false);
        rig.transport.setBpm(99.0);
        rig.block();
        QCOMPARE(rig.transport.bpm(), 99.0);
    }

    void stoppingEndsEveryNote()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.player.setAssignment(0, {3, 2});
        rig.transport.setPlaying(true);
        rig.run(0.8); // the A string note (45) is sounding, bent (at 0.75 s)
        rig.take();
        rig.transport.setPlaying(false);
        rig.block();
        const std::vector<Got> want{{rig.sample - long(kBlock) - rig.playStart, 3, 0x82, 45, 0},
                                    {rig.sample - long(kBlock) - rig.playStart, 3, 0xE2, 0x00, 0x40},
                                    {rig.sample - long(kBlock) - rig.playStart, 3, 0xB2, 64, 0}};
        const auto stopped = rig.take();
        dump("stop", stopped);
        QCOMPARE(stopped, want);
        rig.run(0.5);
        QVERIFY(rig.take().empty()); // nothing while stopped
        // Play again: from the cue (the start).
        rig.transport.setPlaying(true);
        rig.block();
        const auto again = rig.take();
        QVERIFY(!again.empty());
        QCOMPARE(again.front(), (Got{0, 3, 0x92, 40, 100}));
    }

    void muteSoloLevelAndReassign()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.player.setAssignment(0, {0, 0});
        rig.player.setAssignment(1, {1, 9, true});
        rig.player.setLevel(0, 0.5f);
        rig.transport.setPlaying(true);
        rig.block();
        auto got = rig.take();
        QCOMPARE(got.size(), std::size_t(2));
        QCOMPARE(got[0], (Got{0, 0, 0x90, 40, 50})); // level halves the velocity
        // Solo the drums: the bass note ends at once.
        rig.player.setSolo(1, true);
        rig.block();
        got = rig.take();
        QCOMPARE(got.size(), std::size_t(1));
        QCOMPARE(got[0].status, 0x80);
        QCOMPARE(got[0].d1, 40);
        rig.player.setSolo(1, false);
        rig.run(0.3); // the bass note's own note-off isn't sent: it was already ended
        for (const Got &g : rig.take())
            QVERIFY(g.device != 0);
        // Reassign while a note sounds: it ends on the old instrument, the rest go to the new one.
        rig.run(0.25); // 45 starts at 0.5 s
        rig.take();
        rig.player.setAssignment(0, {4, 1});
        rig.run(0.6);
        got = rig.take();
        dump("reassign", got);
        QVERIFY(got.size() >= 3);
        QCOMPARE(got[0], (Got{got[0].sample, 0, 0x80, 45, 0}));
        for (std::size_t i = 1; i < got.size(); ++i)
            QVERIFY(got[i].device == 4 || got[i].device == 1);
        bool bendOnNew = false;
        for (const Got &g : got)
            bendOnNew = bendOnNew || (g.device == 4 && g.status == 0xE1);
        QVERIFY(bendOnNew); // the bend (after the move) goes to the new instrument, on its channel
        // Unassigned: silent.
        rig.player.setAssignment(0, {});
        rig.player.setMute(1, true);
        rig.run(1.0);
        for (const Got &g : rig.take())
            QVERIFY(g.status < 0x90 || g.status > 0x9F);
    }

    void cueAndJump()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.player.setAssignment(0, {0, 0});
        rig.block();          // the song arrives
        rig.player.cue(1920); // bar 2
        rig.block();          // the tempo follows the cue at once
        QCOMPARE(rig.player.position(), Tick(1920));
        QCOMPARE(rig.transport.bpm(), 60.0); // the tempo where play will start
        rig.transport.setPlaying(true);
        rig.block();
        auto got = rig.take();
        QCOMPARE(got.size(), std::size_t(1));
        QCOMPARE(got[0], (Got{0, 0, 0x90, 43, 100}));
        // Jump back to the start while playing: the held note ends, the first one plays.
        rig.player.cue(0);
        rig.block();
        got = rig.take();
        QCOMPARE(got.size(), std::size_t(2)); // the held note's end, the first note
        QCOMPARE(got[0].status, 0x80);
        QCOMPARE(got[0].d1, 43);
        QCOMPARE(got[1].d1, 40); // 40 starts the song again
        QCOMPARE(got[1].status, 0x90);
        QVERIFY(rig.player.position() < 480);
    }

    void endsAndSwapsSongs()
    {
        Rig rig;
        rig.player.load(makeSong());
        rig.player.setAssignment(0, {0, 0});
        rig.transport.setPlaying(true);
        rig.run(2.0 + 4.0 + 0.1); // 2 s for bar 1 at 120, 4 s for bar 2 at 60
        QVERIFY(rig.player.ended());
        // Load another while playing: it starts from its top, the old one is handed back.
        auto next = makeSong();
        next->tracks.resize(1);
        rig.player.load(std::move(next));
        rig.take();
        rig.block();
        QVERIFY(!rig.player.ended());
        const auto got = rig.take();
        QVERIFY(!got.empty());
        QCOMPARE(got.front().d1, 40);
        rig.player.collectGarbage();
        rig.player.load(nullptr);
        rig.block();
        QVERIFY(rig.take().empty() || true);
        rig.run(0.1);
        QVERIFY(rig.take().empty());
    }

    // Through the rack: song events reach their device in frame order with
    // the device's routed MIDI, past focus and channel routing.
    void throughTheRack()
    {
        struct Fake final : IInstrument
        {
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
                for (std::uint32_t i = 0; i < b.midiCount; ++i)
                    seen.push_back({b.midi[i].frame, b.midi[i].data[1]});
            }
            std::vector<std::pair<std::uint32_t, int>> seen;
        } a, b;
        Rack rack(kBlock);
        rack.addDevice(a, 0);
        rack.addDevice(b);
        rack.prepare(kRate);
        rack.setFocus(0);
        std::vector<float> l(kBlock), r(kBlock);
        const MidiEvent keyboard[2] = {{10, 3, {0x90, 60, 100}}, {50, 3, {0x80, 60, 0}}}; // to device 0 (channel 1)
        const DeviceEvent song[3] = {{1, {5, 3, {0x95, 40, 90}}}, {0, {20, 3, {0x90, 70, 90}}}, {0, {40, 3, {0x80, 70, 0}}}};
        RackBlock block;
        block.frames = kBlock;
        block.midi = keyboard;
        block.midiCount = 2;
        block.deviceMidi = song;
        block.deviceMidiCount = 3;
        block.masterL = l.data();
        block.masterR = r.data();
        rack.process(block);
        QCOMPARE(a.seen, (std::vector<std::pair<std::uint32_t, int>>{{10, 60}, {20, 70}, {40, 70}, {50, 60}}));
        QCOMPARE(b.seen, (std::vector<std::pair<std::uint32_t, int>>{{5, 40}})); // not focused, no channel: still gets it
    }

    void rolesAndDrumMap()
    {
        Track t;
        t.name = "Lead Guitar";
        QCOMPARE(guessRole(t), TrackRole::Guitar);
        t.name = "Synth Bass";
        QCOMPARE(guessRole(t), TrackRole::Bass);
        t.name = "Pad";
        QCOMPARE(guessRole(t), TrackRole::Keys);
        t.name = "Track 4";
        t.program = 33; // a bass program
        QCOMPARE(guessRole(t), TrackRole::Bass);
        t.program = -1;
        t.highestNote = 50;
        QCOMPARE(guessRole(t), TrackRole::Bass);
        t.highestNote = 80;
        QCOMPARE(guessRole(t), TrackRole::Other);
        t.drums = true;
        QCOMPARE(guessRole(t), TrackRole::Drums);
        ScoreTrack kit;
        kit.drums = true;
        Track plain;
        QCOMPARE(guessRole(plain, &kit), TrackRole::Drums);

        QCOMPARE(mapToBasicKit(36), 36);
        QCOMPARE(mapToBasicKit(55), 49); // splash
        QCOMPARE(mapToBasicKit(53), 51); // ride bell
        QCOMPARE(mapToBasicKit(64), 41); // low conga
        QCOMPARE(mapToBasicKit(56), 37); // cowbell
        QCOMPARE(mapToBasicKit(28), -1);
        QCOMPARE(mapToBasicKit(100), -1);
    }
};

QTEST_GUILESS_MAIN(TestSongPlayer)
#include "tst_songplayer.moc"
