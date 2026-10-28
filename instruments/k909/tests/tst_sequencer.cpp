#include <QtTest>

#include <vector>

#include "core/Transport.h"
#include "k909/dsp/Engine.h"
#include "k909/dsp/Sequencer.h"

using namespace k909;

namespace {

constexpr double kRate = 48000.0;
constexpr double kBpm = 120.0;
constexpr int kFramesPerStep = 6000; // 1/16 at 120 BPM, 48 kHz

struct Fired
{
    int frame;
    Hits hits;
};

// Runs the sequencer for `frames` samples from beat 0; returns every sample that fired.
std::vector<Fired> run(Sequencer &seq, int frames, double bpm = kBpm)
{
    std::vector<Fired> fired;
    seq.start();
    for (int i = 0; i < frames; ++i) {
        const Hits h = seq.tick(double(i) * bpm / 60.0 / kRate);
        if (h.on)
            fired.push_back({i, h});
    }
    return fired;
}

Pattern everyStep(DrumId drum)
{
    Pattern p;
    p.tracks[std::size_t(drum)].fill(StepState::On);
    return p;
}

constexpr std::uint16_t bit(DrumId d) { return std::uint16_t(1u << int(d)); }

} // namespace

class TestSequencer : public QObject
{
    Q_OBJECT

private slots:
    void stepsLandOnTheirSample()
    {
        Sequencer seq;
        seq.setPattern(everyStep(DrumId::Bd));
        const auto fired = run(seq, 16 * kFramesPerStep + 10);
        QCOMPARE(int(fired.size()), 17); // 16 steps and the first of the next pass
        for (int i = 0; i < 17; ++i)
            QCOMPARE(fired[std::size_t(i)].frame, i * kFramesPerStep);
    }

    void accentAndState()
    {
        Pattern p;
        p.tracks[std::size_t(DrumId::Bd)][0] = StepState::Accent;
        p.tracks[std::size_t(DrumId::Sd)][0] = StepState::On;
        p.tracks[std::size_t(DrumId::Sd)][2] = StepState::Accent;
        Sequencer seq;
        seq.setPattern(p);
        const auto fired = run(seq, 4 * kFramesPerStep);
        QCOMPARE(int(fired.size()), 2);
        QCOMPARE(fired[0].hits.on, std::uint16_t(bit(DrumId::Bd) | bit(DrumId::Sd)));
        QCOMPARE(fired[0].hits.accent, bit(DrumId::Bd));
        QCOMPARE(fired[1].frame, 2 * kFramesPerStep);
        QCOMPARE(fired[1].hits.accent, bit(DrumId::Sd));
    }

    void closedHatWinsOverOpenHat()
    {
        Pattern p;
        p.tracks[std::size_t(DrumId::Ch)][0] = StepState::On;
        p.tracks[std::size_t(DrumId::Oh)][0] = StepState::Accent;
        p.tracks[std::size_t(DrumId::Oh)][1] = StepState::On;
        Sequencer seq;
        seq.setPattern(p);
        const auto fired = run(seq, 2 * kFramesPerStep);
        QCOMPARE(fired[0].hits.on, bit(DrumId::Ch));
        QCOMPARE(fired[0].hits.accent, std::uint16_t(0));
        QCOMPARE(fired[1].hits.on, bit(DrumId::Oh));
    }

    // Shuffle delays every second step by up to a third of a step.
    void shuffleDelaysOffBeats_data()
    {
        QTest::addColumn<int>("amount");
        QTest::addColumn<int>("delay");
        QTest::newRow("straight") << 0 << 0;
        QTest::newRow("3") << 3 << kFramesPerStep / 5; // 3/5 of a third
        QTest::newRow("full") << 5 << kFramesPerStep / 3;
    }
    void shuffleDelaysOffBeats()
    {
        QFETCH(int, amount);
        QFETCH(int, delay);
        Sequencer seq;
        seq.setPattern(everyStep(DrumId::Ch));
        seq.setShuffle(amount);
        const auto fired = run(seq, 4 * kFramesPerStep);
        QCOMPARE(int(fired.size()), 4);
        QCOMPARE(fired[0].frame, 0);
        QCOMPARE(fired[1].frame, kFramesPerStep + delay);
        QCOMPARE(fired[2].frame, 2 * kFramesPerStep);
        QCOMPARE(fired[3].frame, 3 * kFramesPerStep + delay);
    }

    void lengthWraps()
    {
        Pattern p = everyStep(DrumId::Bd);
        p.length = 3;
        Sequencer seq;
        seq.setPattern(p);
        seq.start();
        std::vector<int> steps;
        for (int i = 0; i < 7 * kFramesPerStep; ++i) {
            const Hits h = seq.tick(double(i) * kBpm / 60.0 / kRate);
            if (h.on)
                steps.push_back(seq.currentStep());
        }
        QCOMPARE(steps, (std::vector<int>{0, 1, 2, 0, 1, 2, 0}));
    }

    void stepsPerBeat()
    {
        Pattern p = everyStep(DrumId::Bd);
        p.stepsPerBeat = 3; // 1/8 triplets: 8000 frames apart
        Sequencer seq;
        seq.setPattern(p);
        const auto fired = run(seq, 3 * 8000);
        QCOMPARE(int(fired.size()), 3);
        QCOMPARE(fired[1].frame, 8000);
        QCOMPARE(fired[2].frame, 16000);
    }

    void editWhilePlaying()
    {
        Sequencer seq;
        seq.setPattern(Pattern{});
        seq.start();
        for (int i = 0; i < kFramesPerStep / 2; ++i)
            QVERIFY(!seq.tick(double(i) * kBpm / 60.0 / kRate).on);
        seq.setStep(0, int(DrumId::Sd), 1, StepState::On);
        bool played = false;
        for (int i = kFramesPerStep / 2; i < 2 * kFramesPerStep; ++i)
            if (seq.tick(double(i) * kBpm / 60.0 / kRate).on) {
                QCOMPARE(i, kFramesPerStep);
                played = true;
            }
        QVERIFY(played);
    }

    // A pattern chosen while playing takes over when the next pass begins.
    void patternChangeWaitsForThePass()
    {
        Pattern a = everyStep(DrumId::Bd);
        a.length = 4;
        Pattern b = everyStep(DrumId::Sd);
        Sequencer seq;
        seq.setPattern(a, 0);
        seq.start();
        std::vector<std::pair<int, std::uint16_t>> fired;
        for (int i = 0; i < 6 * kFramesPerStep; ++i) {
            if (i == kFramesPerStep + 10) {
                seq.queuePattern(b, 1);
                QCOMPARE(seq.queuedSlot(), 1);
                QCOMPARE(seq.slot(), 0);
            }
            const Hits h = seq.tick(double(i) * kBpm / 60.0 / kRate);
            if (h.on)
                fired.push_back({seq.currentStep(), h.on});
        }
        // Steps 0..3 of A (bass drum), then B from its first step (snare).
        QCOMPARE(int(fired.size()), 6);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(fired[std::size_t(i)].second, bit(DrumId::Bd));
        QCOMPARE(fired[4], (std::pair<int, std::uint16_t>{0, bit(DrumId::Sd)}));
        QCOMPARE(fired[5], (std::pair<int, std::uint16_t>{1, bit(DrumId::Sd)}));
        QCOMPARE(seq.slot(), 1);
        QCOMPARE(seq.queuedSlot(), -1);
    }

    // Edits name their slot: the waiting pattern can be edited before it plays.
    void editsReachTheQueuedPattern()
    {
        Sequencer seq;
        seq.setPattern(Pattern{}, 0);
        seq.start();
        seq.tick(0.0);
        seq.queuePattern(Pattern{}, 3);
        seq.setStep(3, int(DrumId::Cp), 0, StepState::Accent);
        seq.setStep(5, int(DrumId::Bd), 0, StepState::On); // neither playing nor queued: ignored
        QVERIFY(seq.pattern() == Pattern{});
        Hits first;
        for (int i = 1; i <= 16 * kFramesPerStep; ++i)
            if (const Hits h = seq.tick(double(i) * kBpm / 60.0 / kRate); h.on) {
                first = h;
                break;
            }
        QCOMPARE(first.accent, bit(DrumId::Cp));
        QCOMPARE(seq.slot(), 3);
    }

    // Choosing the playing pattern again cancels a waiting change.
    void reselectingCancelsTheChange()
    {
        Sequencer seq;
        seq.setPattern(Pattern{}, 0);
        seq.start();
        seq.tick(0.0);
        seq.queuePattern(everyStep(DrumId::Bd), 2);
        seq.queuePattern(Pattern{}, 0);
        QCOMPARE(seq.queuedSlot(), -1);
        QCOMPARE(seq.slot(), 0);
    }

    // A new pattern in another play order starts from that order's first step.
    void newOrderStartsFromItsFirstStep()
    {
        Pattern a = everyStep(DrumId::Bd);
        a.length = 2;
        Pattern b = everyStep(DrumId::Sd);
        b.length = 4;
        b.order = std::uint8_t(krate::StepClock::Order::Reverse);
        Sequencer seq;
        seq.setPattern(a, 0);
        seq.start();
        std::vector<int> steps;
        for (int i = 0; i < 6 * kFramesPerStep; ++i) {
            if (i == 10)
                seq.queuePattern(b, 1);
            if (seq.tick(double(i) * kBpm / 60.0 / kRate).on)
                steps.push_back(seq.currentStep());
        }
        QCOMPARE(steps, (std::vector<int>{0, 1, 3, 2, 1, 0}));
    }

    void stoppedChangeIsImmediate()
    {
        Sequencer seq;
        seq.queuePattern(everyStep(DrumId::Rd), 6);
        QCOMPARE(seq.slot(), 6);
        QCOMPARE(seq.queuedSlot(), -1);
    }

    void factoryPatternsAreDistinctAndValid()
    {
        for (int i = 0; i < kFactoryPatternCount; ++i) {
            const Pattern p = factoryPattern(i);
            QCOMPARE(int(p.length), 16);
            QVERIFY(p != Pattern{});
            for (int j = 0; j < i; ++j)
                QVERIFY(p != factoryPattern(j));
        }
    }

    // Through the engine and the transport: steps sound on their exact sample,
    // edits arrive through post(), and stopping clears the playhead.
    void engineFollowsTheTransport()
    {
        Engine engine;
        engine.prepare(kRate, 64);
        engine.setPattern(Pattern{});
        Command set;
        set.kind = Command::Kind::SetStep;
        set.drum = std::uint8_t(DrumId::Bd);
        set.step = 4; // beat 2: frame 24000
        set.state = StepState::On;
        QVERIFY(engine.post(set));

        krate::Transport transport;
        transport.setBpm(kBpm);
        transport.setPlaying(true);
        std::vector<float> out;
        std::vector<float> l(64), r(64);
        for (int block = 0; block < 30000 / 64; ++block) {
            const krate::TransportState state = transport.advance(64, kRate);
            krate::processStereo(engine, l.data(), r.data(), 64, nullptr, 0, &state);
            out.insert(out.end(), l.begin(), l.end());
        }
        QCOMPARE(engine.currentStep(), 4);
        for (int i = 0; i < 24000; ++i)
            QCOMPARE(out[std::size_t(i)], 0.0f);
        QVERIFY(out[24000] != 0.0f);

        transport.setPlaying(false);
        const krate::TransportState state = transport.advance(64, kRate);
        krate::processStereo(engine, l.data(), r.data(), 64, nullptr, 0, &state);
        QCOMPARE(engine.currentStep(), -1);
    }
};

QTEST_GUILESS_MAIN(TestSequencer)
#include "tst_sequencer.moc"
