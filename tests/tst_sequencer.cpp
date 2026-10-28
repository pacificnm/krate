#include <QtTest>

#include <cmath>
#include <vector>

#include "dsp/Sequencer.h"

using namespace k303;
using Kind = SequencerEvent::Kind;

namespace {

struct Logged
{
    Kind kind;
    int step;
    long sample;
};

// Run the sequencer for `samples` ticks and log every event with the step it fell in.
std::vector<Logged> run(Sequencer &seq, long samples)
{
    std::vector<Logged> log;
    for (long i = 0; i < samples; ++i) {
        const auto ev = seq.tick();
        if (ev.kind != Kind::None)
            log.push_back({ev.kind, seq.currentStep(), i});
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
        seq.setSampleRate(48000.0);
        seq.setTempo(133.0);
        Pattern p;
        for (auto &s : p.steps)
            s = note(0);
        seq.setPattern(p);
        seq.start();

        const double sps = seq.samplesPerStep();
        QVERIFY(sps != std::floor(sps)); // genuinely fractional
        long n = 0;
        const long steps = 100000;
        for (long i = 0; n < steps; ++i) {
            if (seq.tick().kind == Kind::NoteOn) {
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
        seq.setSampleRate(48000.0);
        seq.setTempo(120.0); // 6000 samples per step
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

        const auto log = run(seq, 6 * 6000 + 1); // one pass plus the wrap
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
        seq.setSampleRate(48000.0);
        seq.setTempo(120.0);
        Pattern p;
        p.length = 2;
        p.steps[0] = note(0);
        p.steps[1] = note(7, /*slide*/ true);
        seq.setPattern(p);
        seq.start();
        const auto log = run(seq, 2 * 6000 + 1);
        QCOMPARE(log.back().step, 0);
        QCOMPARE(int(log.back().kind), int(Kind::SlideTo));
    }

    void accentAndPitchFromStep()
    {
        Sequencer seq;
        seq.setSampleRate(48000.0);
        Pattern p;
        p.length = 1;
        p.steps[0] = Step{3, 1, true, false, Gate::Note};
        seq.setPattern(p);
        seq.start();
        const auto ev = seq.tick();
        QCOMPARE(int(ev.kind), int(Kind::NoteOn));
        QCOMPARE(ev.note, 24 + 12 * 2 + 3); // D#3
        QVERIFY(ev.accent);
    }

    void stopReleasesSoundingNote()
    {
        Sequencer seq;
        seq.setSampleRate(48000.0);
        Pattern p;
        p.steps[0] = note(0);
        seq.setPattern(p);
        seq.start();
        seq.tick();
        QCOMPARE(int(seq.stop().kind), int(Kind::GateOff));
        QCOMPARE(seq.currentStep(), -1);
        QCOMPARE(int(seq.tick().kind), int(Kind::None));
        QCOMPARE(int(seq.stop().kind), int(Kind::None)); // nothing sounding
    }

    // Halving the step length mid-step keeps the fraction done: 3000 of 6000
    // samples becomes 1500 of 3000, so the next step starts 1500 samples later.
    void tempoChangeMidStep()
    {
        Sequencer seq;
        seq.setSampleRate(48000.0);
        seq.setTempo(120.0);
        Pattern p;
        for (auto &s : p.steps)
            s = note(0);
        seq.setPattern(p);
        seq.start();
        run(seq, 3000);
        seq.setTempo(240.0);
        long i = 3000;
        for (;; ++i)
            if (seq.tick().kind == Kind::NoteOn)
                break;
        QVERIFY(std::abs(i - 4500) <= 1);
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
