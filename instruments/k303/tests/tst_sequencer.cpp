#include <QtTest>

#include <cmath>
#include <vector>

#include "core/Transport.h"
#include "dsp/Sequencer.h"

using namespace k303;
using Kind = SequencerEvent::Kind;

namespace {

// Feeds a sequencer from a real transport, one sample at a time.
struct Driver
{
    Sequencer &seq;
    double sampleRate = 48000.0;
    krate::Transport transport;
    long sample = 0;

    explicit Driver(Sequencer &s, double bpm = 120.0, double rate = 48000.0)
        : seq(s)
        , sampleRate(rate)
    {
        transport.setBpm(bpm);
        transport.setPlaying(true);
    }

    SequencerEvent tick()
    {
        const auto state = transport.advance(1, sampleRate);
        ++sample;
        return seq.tick(state.beatAtBlockStart);
    }

    double samplesPerStep() const
    {
        return sampleRate * 60.0 / (transport.bpm() * seq.stepsPerBeat());
    }
};

struct Logged
{
    Kind kind;
    int step;
    long sample;
};

// Run the sequencer for `samples` ticks and log every event with the step it fell in.
std::vector<Logged> run(Driver &d, long samples)
{
    std::vector<Logged> log;
    for (long i = 0; i < samples; ++i) {
        const long at = d.sample;
        const auto ev = d.tick();
        if (ev.kind != Kind::None)
            log.push_back({ev.kind, d.seq.currentStep(), at});
    }
    return log;
}

Step note(int n, bool slide = false, bool accent = false)
{
    return Step{std::uint8_t(n), 0, accent, slide, Gate::Note};
}

Step gate(Gate g)
{
    Step s;
    s.gate = g;
    return s;
}

} // namespace

class TestSequencer : public QObject
{
    Q_OBJECT

private slots:
    // Step n starts within one sample of n * samplesPerStep, with fractional
    // samples per step, over a long run (docs/SEQUENCER.md acceptance).
    void timingIsExactOverLongRuns()
    {
        Sequencer seq;
        Driver d(seq, 133.0);
        Pattern p;
        for (auto &s : p.steps)
            s = note(0);
        seq.setPattern(p);
        seq.start();

        const double sps = d.samplesPerStep();
        QVERIFY(sps != std::floor(sps)); // genuinely fractional
        long n = 0;
        const long steps = 100000;
        for (long i = 0; n < steps; ++i) {
            if (d.tick().kind == Kind::NoteOn) {
                const double expected = n * sps;
                if (std::abs(double(i) - expected) > 1.0)
                    QFAIL(qPrintable(QString("step %1 at sample %2, expected %3").arg(n).arg(i).arg(expected)));
                ++n;
            }
        }
    }

    void playbackRules()
    {
        Sequencer seq;
        Driver d(seq); // 120 BPM: 6000 samples per step
        Pattern p;
        p.length = 6;
        p.steps[0] = note(0);              // plain note: gate off at half step
        p.steps[1] = note(2, /*slide*/ true);
        p.steps[2] = note(4);              // slid into: SlideTo, no retrigger
        p.steps[3] = gate(Gate::Tie);      // extends step 2; gate off at its half
        p.steps[4] = gate(Gate::Rest);
        p.steps[5] = gate(Gate::Tie);      // tie after a rest: silent
        seq.setPattern(p);
        seq.start();

        const auto log = run(d, 6 * 6000 + 1); // one pass plus the wrap
        const std::vector<std::pair<Kind, int>> expected = {
            {Kind::NoteOn, 0}, {Kind::GateOff, 0},
            {Kind::NoteOn, 1},
            {Kind::SlideTo, 2},
            {Kind::GateOff, 3},
            {Kind::NoteOn, 0},
        };
        QCOMPARE(log.size(), expected.size());
        for (std::size_t i = 0; i < log.size(); ++i) {
            QCOMPARE(int(log[i].kind), int(expected[i].first));
            QCOMPARE(log[i].step, expected[i].second);
        }
        QCOMPARE(log[1].sample, 3000L); // gate length 0.5
        QCOMPARE(log[4].sample, 3 * 6000L + 3000L);
    }

    void slideWrapsAroundThePattern()
    {
        Sequencer seq;
        Driver d(seq);
        Pattern p;
        p.length = 2;
        p.steps[0] = note(0);
        p.steps[1] = note(7, /*slide*/ true);
        seq.setPattern(p);
        seq.start();
        const auto log = run(d, 2 * 6000 + 1);
        QCOMPARE(log.back().step, 0);
        QCOMPARE(int(log.back().kind), int(Kind::SlideTo));
    }

    void accentAndPitchFromStep()
    {
        Sequencer seq;
        Driver d(seq);
        Pattern p;
        p.length = 1;
        p.steps[0] = Step{3, 1, true, false, Gate::Note};
        seq.setPattern(p);
        seq.start();
        const auto ev = d.tick();
        QCOMPARE(int(ev.kind), int(Kind::NoteOn));
        QCOMPARE(ev.note, 24 + 12 * 2 + 3); // D#3
        QVERIFY(ev.accent);
    }

    void stopReleasesSoundingNote()
    {
        Sequencer seq;
        Driver d(seq);
        Pattern p;
        p.steps[0] = note(0);
        seq.setPattern(p);
        seq.start();
        d.tick();
        QCOMPARE(int(seq.stop().kind), int(Kind::GateOff));
        QCOMPARE(seq.currentStep(), -1);
        QCOMPARE(int(d.tick().kind), int(Kind::None));
        QCOMPARE(int(seq.stop().kind), int(Kind::None)); // nothing sounding
    }

    // Doubling the tempo mid-step keeps the fraction done: 3000 of 6000
    // samples becomes 1500 of 3000, so the next step starts 1500 samples later.
    void tempoChangeMidStep()
    {
        Sequencer seq;
        Driver d(seq);
        Pattern p;
        for (auto &s : p.steps)
            s = note(0);
        seq.setPattern(p);
        seq.start();
        run(d, 3000);
        d.transport.setBpm(240.0);
        long i = 3000;
        for (;; ++i)
            if (d.tick().kind == Kind::NoteOn)
                break;
        QVERIFY(std::abs(i - 4500) <= 1);
    }

    // Step order for each play mode over a 4-step pattern.
    static std::vector<int> order(Sequencer::PlayMode mode, int steps, int length = 4)
    {
        Sequencer seq;
        Driver d(seq);
        seq.setPlayMode(mode);
        Pattern p;
        p.length = std::uint8_t(length);
        for (auto &s : p.steps)
            s = note(0);
        seq.setPattern(p);
        seq.start();
        std::vector<int> result;
        int last = -1;
        for (long i = 0; int(result.size()) < steps; ++i) {
            if (d.tick().kind != Kind::None || seq.currentStep() != last) {
                if (i % 6000 == 0)
                    result.push_back(seq.currentStep());
                last = seq.currentStep();
            }
        }
        return result;
    }

    void playModeOrder()
    {
        using M = Sequencer::PlayMode;
        QCOMPARE(order(M::Forward, 9), (std::vector<int>{0, 1, 2, 3, 0, 1, 2, 3, 0}));
        QCOMPARE(order(M::Reverse, 9), (std::vector<int>{3, 2, 1, 0, 3, 2, 1, 0, 3}));
        QCOMPARE(order(M::PingPong, 10), (std::vector<int>{0, 1, 2, 3, 2, 1, 0, 1, 2, 3}));
        QCOMPARE(order(M::PingPong, 4, 1), (std::vector<int>{0, 0, 0, 0}));
        QCOMPARE(order(M::Invert, 5), (std::vector<int>{0, 1, 2, 3, 0}));

        const auto random = order(M::Random, 400);
        std::vector<int> seen(4, 0);
        for (int s : random) {
            QVERIFY(s >= 0 && s < 4);
            ++seen[std::size_t(s)];
        }
        for (int count : seen)
            QVERIFY(count > 50); // every step turns up, roughly evenly
    }

    // Invert mirrors pitch around the step's C: C stays, D# -> A, G -> F, top C -> C.
    void invertMirrorsPitch()
    {
        Sequencer seq;
        Driver d(seq);
        seq.setPlayMode(Sequencer::PlayMode::Invert);
        Pattern p;
        p.length = 4;
        p.steps[0] = note(0);
        p.steps[1] = note(3);
        p.steps[2] = note(7);
        p.steps[3] = Step{12, 1, false, false, Gate::Note};
        seq.setPattern(p);
        seq.start();
        std::vector<int> notes;
        while (notes.size() < 4) {
            const auto ev = d.tick();
            if (ev.kind == Kind::NoteOn)
                notes.push_back(ev.note);
        }
        QCOMPARE(notes, (std::vector<int>{36, 36 + 9, 36 + 5, 48}));
    }

    // Scale sets the step length: steps start every 6000 (16ths), 3000 (32nds)
    // or 4000 (16th triplets) samples at 120 BPM.
    void scaleSetsStepLength()
    {
        for (auto [perBeat, expected] : {std::pair{4, 6000L}, {8, 3000L}, {6, 4000L}}) {
            Sequencer seq;
            Driver d(seq);
            seq.setStepsPerBeat(perBeat);
            Pattern p;
            for (auto &st : p.steps)
                st = note(0);
            seq.setPattern(p);
            seq.start();
            const auto log = run(d, 4 * expected + 1);
            std::vector<long> starts;
            for (const auto &e : log)
                if (e.kind == Kind::NoteOn)
                    starts.push_back(e.sample);
            QCOMPARE(starts, (std::vector<long>{0, expected, 2 * expected, 3 * expected, 4 * expected}));
        }
    }

    // Switching from 16ths to 32nds halfway through a step keeps the half done:
    // the next step starts 1500 samples later, not at once and not after 3000.
    void scaleChangeMidStepKeepsFraction()
    {
        Sequencer seq;
        Driver d(seq);
        Pattern p;
        for (auto &st : p.steps)
            st = note(0);
        seq.setPattern(p);
        seq.start();
        run(d, 3000);
        seq.setStepsPerBeat(8);
        long i = 3000;
        for (;; ++i)
            if (d.tick().kind == Kind::NoteOn)
                break;
        QVERIFY2(std::abs(i - 4500) <= 1, qPrintable(QString::number(i)));
    }

    // Two sequencers on one transport (as in the rack) step together, even
    // when one is set to a different scale.
    void sequencersShareTheClock()
    {
        Sequencer a, b;
        b.setStepsPerBeat(8);
        Pattern p;
        for (auto &st : p.steps)
            st = note(0);
        a.setPattern(p);
        b.setPattern(p);
        a.start();
        b.start();
        krate::Transport transport;
        transport.setBpm(133.0);
        transport.setPlaying(true);
        long aSteps = 0, bSteps = 0;
        for (long i = 0; i < 48000 * 60; ++i) { // one minute
            const double beat = transport.advance(1, 48000.0).beatAtBlockStart;
            const bool aStep = a.tick(beat).kind == Kind::NoteOn;
            const bool bStep = b.tick(beat).kind == Kind::NoteOn;
            if (aStep && !bStep)
                QFAIL(qPrintable(QString("16th step at sample %1 without a 32nd step").arg(i)));
            aSteps += aStep;
            bSteps += bStep;
        }
        QVERIFY(std::abs(bSteps - 2 * aSteps) <= 1);
    }

    // --- Standalone transport --------------------------------------------------

    void transportRestartsAtBeatZero()
    {
        krate::Transport t;
        t.setBpm(120.0);
        auto s = t.advance(64, 48000.0);
        QVERIFY(!s.playing);
        QCOMPARE(s.beatAtBlockStart, 0.0);
        t.setPlaying(true);
        s = t.advance(64, 48000.0);
        QVERIFY(s.playing && s.restarted);
        QCOMPARE(s.beatAtBlockStart, 0.0);
        s = t.advance(64, 48000.0);
        QVERIFY(!s.restarted);
        QCOMPARE(s.beatAtBlockStart, 64.0 / 24000.0); // 120 BPM: 24000 samples per beat
        t.setPlaying(false);
        t.advance(64, 48000.0);
        t.setPlaying(true);
        s = t.advance(64, 48000.0);
        QVERIFY(s.restarted);
        QCOMPARE(s.beatAtBlockStart, 0.0);
    }

    // A tempo change continues from the current beat, then moves at the new rate.
    void transportTempoChangeIsContinuous()
    {
        krate::Transport t;
        t.setBpm(120.0);
        t.setPlaying(true);
        for (int i = 0; i < 375; ++i)
            t.advance(64, 48000.0); // 24000 samples = 1 beat
        t.setBpm(60.0);
        auto s = t.advance(64, 48000.0);
        QVERIFY(std::abs(s.beatAtBlockStart - 1.0) < 1e-12);
        QCOMPARE(s.bpm, 60.0);
        s = t.advance(64, 48000.0);
        QVERIFY(std::abs(s.beatAtBlockStart - (1.0 + 64.0 / 48000.0)) < 1e-12);
    }

    // Beat position comes from sample count, not a running sum: no drift over an hour.
    void transportDoesNotDrift()
    {
        krate::Transport t;
        t.setBpm(133.0);
        t.setPlaying(true);
        const long blocks = 48000L * 3600 / 64;
        krate::TransportState s;
        for (long i = 0; i <= blocks; ++i)
            s = t.advance(64, 48000.0);
        const double expected = double(blocks) * 64.0 * 133.0 / 60.0 / 48000.0;
        QVERIFY2(std::abs(s.beatAtBlockStart - expected) < 1e-6,
                 qPrintable(QString("%1 vs %2").arg(s.beatAtBlockStart, 0, 'g', 17).arg(expected, 0, 'g', 17)));
    }

    void demoPatternIsValid()
    {
        const Pattern p = demoPattern();
        QCOMPARE(int(p.length), 16);
        for (const auto &s : p.steps) {
            QVERIFY(s.note <= 12);
            QVERIFY(s.octave >= -1 && s.octave <= 1);
            const int midi = stepMidiNote(s);
            QVERIFY(midi >= 24 && midi <= 60);
        }
    }
};

QTEST_APPLESS_MAIN(TestSequencer)
#include "tst_sequencer.moc"
