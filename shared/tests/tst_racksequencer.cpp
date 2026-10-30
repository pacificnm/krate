#include <QtTest>

#include <memory>
#include <vector>

#include "core/EventMerge.h"
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
        std::vector<Hit> hits;

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

private slots:
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
