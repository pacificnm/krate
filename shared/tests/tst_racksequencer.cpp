#include <QtTest>

#include <memory>
#include <vector>

#include "core/EventMerge.h"
#include "core/Transport.h"
#include "seq/Sequencer.h"

using namespace krate;
using namespace krate::seq;

// The rack sequencer's player (docs/SEQUENCER.md): notes on the exact sample,
// loops, cued switches, chords, and nothing left hanging.
class TestRackSequencer : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Hit
    {
        std::uint64_t frame; // from play
        int device;
        std::uint8_t status, key, velocity;
        bool sequenced;
    };

    // Runs the transport for `frames` from `start` (a block counter kept by the
    // caller), playing; returns every event with its absolute frame.
    struct Run
    {
        Sequencer &seq;
        double bpm = 120.0;
        std::uint64_t at = 0;
        bool playing = true;
        std::vector<Hit> hits{};

        void blocks(std::uint64_t frames)
        {
            std::array<DeviceEvent, 256> out{};
            for (std::uint64_t end = at + frames; at < end; at += kBlock) {
                TransportState t;
                t.playing = playing;
                t.restarted = playing && at == 0;
                t.bpm = bpm;
                t.beatAtBlockStart = double(at) * bpm / 60.0 / kRate;
                const std::uint32_t n = seq.render(t, kBlock, kRate, out.data(), std::uint32_t(out.size()));
                for (std::uint32_t i = 0; i < n; ++i) {
                    const MidiEvent &e = out[i].event;
                    hits.push_back({at + e.frame, out[i].device, e.data[0], e.data[1], e.data[2], e.sequenced});
                }
            }
        }
        std::vector<Hit> ons() const
        {
            std::vector<Hit> r;
            for (const Hit &h : hits)
                if ((h.status & 0xF0) == 0x90)
                    r.push_back(h);
            return r;
        }
        std::vector<Hit> offs() const
        {
            std::vector<Hit> r;
            for (const Hit &h : hits)
                if ((h.status & 0xF0) == 0x80)
                    r.push_back(h);
            return r;
        }
    };

    static std::unique_ptr<Bank> bankWith(std::initializer_list<Note> notes, int track = 0, std::uint16_t length = 384)
    {
        auto bank = std::make_unique<Bank>();
        bank->patterns[0].length = length;
        for (const Note &n : notes)
            bank->patterns[0].tracks[std::size_t(track)].add(n);
        return bank;
    }

    // An arrangement: one lane's clips.
    static std::unique_ptr<Arrangement> songWith(std::initializer_list<Clip> clips, int lane = 0)
    {
        auto song = std::make_unique<Arrangement>();
        for (const Clip &c : clips)
            song->lanes[std::size_t(lane)].push_back(c);
        return song;
    }

private slots:
    // SONG mode (docs/DAW.md, D3): a one-beat pattern in a clip on bars 2-3
    // plays on each of its 8 beats and nowhere else; a note longer than its
    // clip ends at the clip's end.
    void patternClipsAtTheirBars()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{0, 48, 60, 100}}, 0, 96));
        seq.setArrangement(songWith({{1, 2, 0, -1}}));
        seq.setSongMode(true);
        Run run{seq};
        run.blocks(24000 * 16);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(8));
        for (std::size_t i = 0; i < ons.size(); ++i)
            QCOMPARE(ons[i].frame, std::uint64_t(24000 * (4 + i)));

        Sequencer cut;
        cut.setTarget(0, {0, 0});
        cut.setBank(bankWith({{0, 96 * 8, 60, 100}}, 0, 384)); // two bars long, in a one-bar clip
        cut.setArrangement(songWith({{0, 1, 0, -1}}));
        cut.setSongMode(true);
        Run r2{cut};
        r2.blocks(24000 * 6);
        QCOMPARE(r2.ons().size(), std::size_t(1));
        QCOMPARE(r2.offs().size(), std::size_t(1));
        QCOMPARE(r2.offs()[0].frame, std::uint64_t(96000)); // bar 1's end
    }

    // A clip starting partway in (D4: trimmed or split): a one-bar pattern
    // with notes on beats 1 and 3, offset by a bar into a 2-bar pattern, plays
    // the pattern's second bar first; a MIDI clip skips the notes before.
    void offsetClips()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setTarget(1, {1, 0});
        seq.setBank(bankWith({{0, 24, 60, 100}, {384, 24, 62, 100}}, 0, 768)); // bar 1: 60, bar 2: 62
        auto song = std::make_unique<Arrangement>();
        song->lanes[0].push_back({1, 2, 0, -1, 1}); // bars 2-3, starting at the pattern's bar 2
        song->midi.push_back({{{0, 24, 70, 100}, {384 + 96, 24, 72, 100}}});
        song->lanes[1].push_back({4, 1, -1, 0, 1}); // bar 5: the clip's second bar only
        seq.setArrangement(std::move(song));
        seq.setSongMode(true);
        Run run{seq};
        run.blocks(24000 * 24);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(3));
        QCOMPARE(ons[0].frame, std::uint64_t(24000 * 4)); // bar 2: the pattern's bar 2
        QCOMPARE(ons[0].key, std::uint8_t(62));
        QCOMPARE(ons[1].frame, std::uint64_t(24000 * 8)); // bar 3: round to its bar 1
        QCOMPARE(ons[1].key, std::uint8_t(60));
        QCOMPARE(ons[2].frame, std::uint64_t(24000 * 17)); // bar 5, beat 2
        QCOMPARE(ons[2].key, std::uint8_t(72));
    }

    // A MIDI clip's own notes, from its start; lanes go to their own targets;
    // a muted lane is silent.
    void midiClips()
    {
        Sequencer seq;
        seq.setTarget(0, {2, 0});
        seq.setTarget(1, {5, 1});
        seq.setBank(std::make_unique<Bank>());
        auto song = std::make_unique<Arrangement>();
        song->midi.push_back({{{0, 48, 64, 90}, {96 * 5, 48, 67, 80}}});
        song->lanes[0].push_back({2, 4, -1, 0});
        song->lanes[1].push_back({0, 1, -1, 0});
        seq.setArrangement(std::move(song));
        seq.setSongMode(true);
        seq.setMute(1, true);
        Run run{seq};
        run.blocks(24000 * 20);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(2));
        QCOMPARE(ons[0].frame, std::uint64_t(24000 * 8));
        QCOMPARE(ons[0].key, std::uint8_t(64));
        QCOMPARE(ons[0].device, 2);
        QCOMPARE(ons[1].frame, std::uint64_t(24000 * 13));
        QCOMPARE(ons[1].velocity, std::uint8_t(80));
    }

    // A jump (the transport looping) ends what was sounding; play from a bar
    // inside a clip picks the pattern up there.
    void jumpsEndNotes()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{0, 96 * 6, 60, 100}}, 0, 768)); // a note of 6 beats, still sounding at the wrap
        seq.setArrangement(songWith({{0, 4, 0, -1}}));
        seq.setSongMode(true);
        Transport transport;
        transport.setLoop(true, 0, 1);
        transport.setPlaying(true);
        std::array<DeviceEvent, 256> out{};
        std::vector<Hit> hits;
        std::uint64_t at = 0;
        while (at < 96000 * 2 + 10) {
            const std::uint32_t n = transport.framesUntilLoop(kBlock, kRate);
            const TransportState t = transport.advance(n, kRate);
            const std::uint32_t count = seq.render(t, n, kRate, out.data(), std::uint32_t(out.size()));
            for (std::uint32_t i = 0; i < count; ++i)
                hits.push_back({at + out[i].event.frame, out[i].device, out[i].event.data[0], out[i].event.data[1],
                                out[i].event.data[2], true});
            at += n;
        }
        // On at 0; at the wrap (96000) it ends, then starts again; the same at 192000.
        QCOMPARE(hits.size(), std::size_t(5));
        QCOMPARE(hits[1].frame, std::uint64_t(96000));
        QCOMPARE(hits[1].status, std::uint8_t(0x80));
        QCOMPARE(hits[2].frame, std::uint64_t(96000));
        QCOMPARE(hits[2].status, std::uint8_t(0x90));
        QCOMPARE(hits[3].frame, std::uint64_t(192000));
        QCOMPARE(hits[3].status, std::uint8_t(0x80));

        // Play from bar 3 (beat 8): the one-bar pattern's note at bar 3's start.
        Sequencer from;
        from.setTarget(0, {0, 0});
        from.setBank(bankWith({{96, 24, 62, 100}}, 0, 384));
        from.setArrangement(songWith({{0, 4, 0, -1}}));
        from.setSongMode(true);
        Transport t2;
        t2.setStartBeat(8.0);
        t2.setPlaying(true);
        std::vector<std::uint64_t> ons;
        for (std::uint64_t f = 0; f < 96000 * 3; f += kBlock) {
            const TransportState t = t2.advance(kBlock, kRate);
            const std::uint32_t count = from.render(t, kBlock, kRate, out.data(), std::uint32_t(out.size()));
            for (std::uint32_t i = 0; i < count; ++i)
                if ((out[i].event.data[0] & 0xF0) == 0x90)
                    ons.push_back(f + out[i].event.frame);
        }
        QCOMPARE(ons.size(), std::size_t(2)); // bars 3 and 4, then the clip ends
        QCOMPARE(ons[0], std::uint64_t(24000));
    }

    // A 16th note on step 2 at 120 BPM: on at 6000, off at 12000 (exact); at
    // 97 BPM a 16th-triplet step lands where the arithmetic says.
    void notesOnTheExactSample()
    {
        Sequencer seq;
        seq.setTarget(0, {3, 2});
        seq.setBank(bankWith({{24, 24, 60, 100}}));
        Run run{seq};
        run.blocks(20000);
        QCOMPARE(run.ons().size(), std::size_t(1));
        QCOMPARE(run.ons()[0].frame, std::uint64_t(6000));
        QCOMPARE(run.ons()[0].device, 3);
        QCOMPARE(run.ons()[0].status, std::uint8_t(0x92));
        QCOMPARE(run.ons()[0].velocity, std::uint8_t(100));
        QVERIFY(run.ons()[0].sequenced); // played as written, past any key-octave shift
        QCOMPARE(run.offs().size(), std::size_t(1));
        QCOMPARE(run.offs()[0].frame, std::uint64_t(12000));

        Sequencer slow;
        slow.setTarget(0, {0, 0});
        slow.setBank(bankWith({{16, 8, 60, 100}}));
        Run r2{slow, 97.0};
        r2.blocks(10000);
        QCOMPARE(r2.ons()[0].frame, std::uint64_t(16.0 / 96.0 * 60.0 / 97.0 * kRate)); // 4948
    }

    // A one-beat pattern loops: its note plays on every beat.
    void loops()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{0, 48, 60, 100}}, 0, 96));
        Run run{seq};
        run.blocks(24000 * 3 + 100);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(4));
        for (std::size_t i = 0; i < ons.size(); ++i)
            QCOMPARE(ons[i].frame, std::uint64_t(24000 * i));
    }

    // Chords: three notes on one step start on the same sample.
    void chords()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{48, 24, 60, 100}, {48, 24, 64, 100}, {48, 24, 67, 100}}));
        Run run{seq};
        run.blocks(20000);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(3));
        for (const Hit &h : ons)
            QCOMPARE(h.frame, std::uint64_t(12000));
        QCOMPARE(run.offs().size(), std::size_t(3));
    }

    // A note repeated on the next step: the off comes before the new on (same sample).
    void repeatedNoteOffFirst()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{0, 24, 60, 100}, {24, 24, 60, 100}}));
        Run run{seq};
        run.blocks(13000);
        std::vector<Hit> at6000;
        for (const Hit &h : run.hits)
            if (h.frame == 6000)
                at6000.push_back(h);
        QCOMPARE(at6000.size(), std::size_t(2));
        QCOMPARE(at6000[0].status & 0xF0, 0x80);
        QCOMPARE(at6000[1].status & 0xF0, 0x90);
    }

    // Choosing a pattern while playing switches at the end of the current one.
    void cuedSwitchAtLoopEnd()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        auto bank = bankWith({{0, 24, 60, 100}}, 0, 96);
        bank->patterns[1].length = 96;
        bank->patterns[1].tracks[0].add({0, 24, 72, 100});
        seq.setBank(std::move(bank));
        Run run{seq};
        run.blocks(12000); // half way through the first pass
        seq.cue(1);
        QCOMPARE(seq.playingPattern(), 0);
        run.blocks(24000 * 2);
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(3));
        QCOMPARE(ons[0].key, std::uint8_t(60));
        QCOMPARE(ons[1].key, std::uint8_t(72)); // at the loop end, not before
        QCOMPARE(ons[1].frame, std::uint64_t(24000));
        QCOMPARE(seq.playingPattern(), 1);
    }

    // Stop, mute, retarget and RUN off each end what is sounding; nothing plays while off.
    void nothingHangs()
    {
        const auto longNote = [] { return bankWith({{0, 4 * 96, 60, 100}}); };
        { // stop
            Sequencer seq;
            seq.setTarget(0, {1, 0});
            seq.setBank(longNote());
            Run run{seq};
            run.blocks(1000);
            run.playing = false;
            run.blocks(64);
            QCOMPARE(run.offs().size(), std::size_t(1));
            QCOMPARE(run.offs()[0].frame, std::uint64_t(1024));
        }
        { // mute
            Sequencer seq;
            seq.setTarget(0, {1, 0});
            seq.setBank(longNote());
            Run run{seq};
            run.blocks(1000);
            seq.setMute(0, true);
            run.blocks(64);
            QCOMPARE(run.offs().size(), std::size_t(1));
        }
        { // retarget: the off goes to the old instrument
            Sequencer seq;
            seq.setTarget(0, {1, 0});
            seq.setBank(longNote());
            Run run{seq};
            run.blocks(1000);
            seq.setTarget(0, {2, 0});
            run.blocks(64);
            QCOMPARE(run.offs().size(), std::size_t(1));
            QCOMPARE(run.offs()[0].device, 1);
        }
        { // RUN off: silent from then on
            Sequencer seq;
            seq.setTarget(0, {1, 0});
            seq.setBank(bankWith({{0, 24, 60, 100}}, 0, 96));
            Run run{seq};
            run.blocks(1000);
            seq.setRunning(false);
            run.blocks(24000 * 2);
            QCOMPARE(run.ons().size(), std::size_t(1));
            QCOMPARE(run.offs().size(), std::size_t(1));
        }
        { // no target: silent
            Sequencer seq;
            seq.setBank(bankWith({{0, 24, 60, 100}}));
            Run run{seq};
            run.blocks(24000);
            QVERIFY(run.hits.empty());
        }
    }

    // A new bank while playing: sounding notes still end; the new notes play.
    void bankSwap()
    {
        Sequencer seq;
        seq.setTarget(0, {0, 0});
        seq.setBank(bankWith({{0, 48, 60, 100}}, 0, 96));
        Run run{seq};
        run.blocks(1000);
        seq.setBank(bankWith({{0, 48, 64, 100}}, 0, 96));
        seq.collectGarbage();
        run.blocks(24000 * 2);
        seq.collectGarbage(); // the first bank comes back for deletion
        const auto ons = run.ons();
        QCOMPARE(ons.size(), std::size_t(3));
        QCOMPARE(ons[1].key, std::uint8_t(64));
        QCOMPARE(run.offs().size(), std::size_t(2)); // the third ends half a beat after it starts, past the run
    }

    // CHAIN: the chain's patterns in order, each its number of times, then
    // around again; switched on while playing, it starts at the next pass.
    void chain()
    {
        // Patterns 0, 1, 2 are one beat long, each with one note: 60, 62, 64.
        const auto makeBank = [] {
            auto bank = std::make_unique<Bank>();
            for (int p = 0; p < 3; ++p) {
                bank->patterns[std::size_t(p)].length = 96;
                bank->patterns[std::size_t(p)].tracks[0].add({0, 24, std::uint8_t(60 + 2 * p), 100});
            }
            bank->chain[0] = {2, 2};
            bank->chain[1] = {0, 1};
            bank->chainLength = 2;
            return bank;
        };
        const auto keys = [](const Run &run) {
            std::vector<int> k;
            for (const Hit &h : run.ons())
                k.push_back(h.key);
            return k;
        };
        {
            Sequencer seq;
            seq.setTarget(0, {0, 0});
            seq.setChainMode(true);
            seq.setBank(makeBank());
            Run run{seq};
            run.blocks(24000 * 6 + 100);
            QCOMPARE(keys(run), (std::vector<int>{64, 64, 60, 64, 64, 60, 64}));
            QCOMPARE(seq.chainStep(), 0);
        }
        { // switched on mid-play: the chain starts at the next pass
            Sequencer seq;
            seq.setTarget(0, {0, 0});
            seq.cue(1);
            seq.setBank(makeBank());
            Run run{seq};
            run.blocks(12000);
            QCOMPARE(seq.chainStep(), -1);
            seq.setChainMode(true);
            run.blocks(24000 * 3);
            QCOMPARE(keys(run), (std::vector<int>{62, 64, 64, 60}));
        }
        { // an empty chain plays the cued pattern
            Sequencer seq;
            seq.setTarget(0, {0, 0});
            seq.setChainMode(true);
            auto bank = makeBank();
            bank->chainLength = 0;
            seq.setBank(std::move(bank));
            seq.cue(1);
            Run run{seq};
            run.blocks(24000 * 2 + 100);
            QCOMPARE(keys(run), (std::vector<int>{62, 62, 62}));
        }
    }

    // RANGE: patterns from..to in order, then around again; switched on while
    // playing, from the next pass (outside the range: from its first).
    void range()
    {
        const auto makeBank = [] {
            auto bank = std::make_unique<Bank>();
            for (int p = 0; p < 6; ++p) {
                bank->patterns[std::size_t(p)].length = 96;
                bank->patterns[std::size_t(p)].tracks[0].add({0, 24, std::uint8_t(60 + p), 100});
            }
            return bank;
        };
        const auto keys = [](const Run &run) {
            std::vector<int> k;
            for (const Hit &h : run.ons())
                k.push_back(h.key);
            return k;
        };
        {
            Sequencer seq;
            seq.setTarget(0, {0, 0});
            seq.setRangeMode(true);
            seq.setRange(4, 1); // either order
            seq.setBank(makeBank());
            Run run{seq};
            run.blocks(24000 * 5 + 100);
            QCOMPARE(keys(run), (std::vector<int>{61, 62, 63, 64, 61, 62}));
        }
        { // on mid-play, from pattern 5 (outside 1..2): the range from its start at the next pass
            Sequencer seq;
            seq.setTarget(0, {0, 0});
            seq.cue(5);
            seq.setRange(1, 2);
            seq.setBank(makeBank());
            Run run{seq};
            run.blocks(12000);
            seq.setRangeMode(true);
            run.blocks(24000 * 3);
            QCOMPARE(keys(run), (std::vector<int>{65, 61, 62, 61}));
        }
    }

    // The merger: both sources' events, by frame, earlier source first on ties.
    void mergeByFrame()
    {
        struct Fixed final : IEventSource
        {
            std::vector<DeviceEvent> events;
            void beforeTransport(Transport &) noexcept override {}
            std::uint32_t render(const TransportState &, std::uint32_t, double, DeviceEvent *out,
                                 std::uint32_t capacity) noexcept override
            {
                std::uint32_t n = 0;
                for (const DeviceEvent &e : events)
                    if (n < capacity)
                        out[n++] = e;
                return n;
            }
        };
        Fixed a, b;
        a.events = {{0, {5, 3, {0x90, 1, 1}}}, {0, {20, 3, {0x90, 2, 1}}}};
        b.events = {{1, {5, 3, {0x90, 3, 1}}}, {1, {10, 3, {0x90, 4, 1}}}};
        EventMerge<2> merge({&a, &b});
        std::array<DeviceEvent, 16> out{};
        const std::uint32_t n = merge.render({}, 64, kRate, out.data(), std::uint32_t(out.size()));
        QCOMPARE(n, 4u);
        QCOMPARE(out[0].event.data[1], std::uint8_t(1));
        QCOMPARE(out[1].event.data[1], std::uint8_t(3));
        QCOMPARE(out[2].event.data[1], std::uint8_t(4));
        QCOMPARE(out[3].event.data[1], std::uint8_t(2));
    }
};

QTEST_GUILESS_MAIN(TestRackSequencer)
#include "tst_racksequencer.moc"
