#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "core/Transport.h"
#include "kobalt/app/KobaltInstrument.h"
#include "kobalt/app/SequenceFile.h"
#include "kobalt/dsp/Engine.h"

using namespace kobalt;
using namespace kobalt::persistence;
namespace p = kobalt::positions;
using Kind = SeqStep::Kind;

// The step sequencer (docs/BRIEF.md#step-sequencer): playing a pattern on the
// transport (rests, ties, legato, transposing from the keys, 64 steps), loading
// it from the keys, and sequences.json.
class TestSequencer : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 16;
    static constexpr int kStep = 6000; // a 1/16 step at 120 BPM

    static SeqStep note(int n, bool legato = false) { return {Kind::Note, std::uint8_t(n), legato}; }
    static SeqStep tie(bool legato = false) { return {Kind::Tie, 60, legato}; }

    struct Rig
    {
        Engine engine;
        krate::Transport transport;
        std::vector<krate::MidiEvent> pending;
        std::vector<int> notes;  // the voice's note at each gate opening (or note change while open)
        std::vector<int> starts;
        std::vector<int> ends;   // where the gate closed
        int frame = 0;
        bool gate = false;
        int last = -1;

        explicit Rig(const SeqPattern &pattern)
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::PlayMode, position(2, p::kPlayMode)); // Sequencer
            engine.setSequence(pattern);
            transport.setBpm(120.0);
            transport.setPlaying(true);
        }
        void key(int n, bool down) { pending.push_back({0, 3, {std::uint8_t(down ? 0x90 : 0x80), std::uint8_t(n), 100}}); }
        void run(int frames)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (int f = 0; f < frames; f += int(kBlock)) {
                const krate::TransportState state = transport.advance(kBlock, kRate);
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(),
                                     std::uint32_t(pending.size()), &state);
                pending.clear();
                const bool now = engine.voice().gate();
                const int n = engine.voice().note();
                if (now && (!gate || n != last)) {
                    notes.push_back(n);
                    starts.push_back(frame);
                } else if (!now && gate) {
                    ends.push_back(frame);
                }
                gate = now;
                last = n;
                frame += int(kBlock);
            }
        }
    };

private slots:
    // Notes play on their steps for half a step; rests are silent.
    void playsNotesAndRests()
    {
        SeqPattern pattern;
        pattern.length = 4;
        pattern.steps[0] = note(60);
        pattern.steps[1] = note(62);
        pattern.steps[3] = note(65); // step 3: a rest
        Rig rig(pattern);
        rig.run(kStep * 8 - kStep / 4);
        QCOMPARE(rig.notes, (std::vector<int>{60, 62, 65, 60, 62, 65}));
        QVERIFY(std::abs(rig.starts[2] - 3 * kStep) < int(kBlock));
        QVERIFY(std::abs(rig.ends[0] - kStep / 2) < int(kBlock));
    }

    // A tie holds the note through its step; legato runs into the next note
    // with no gap (and glides with Portamento Auto).
    void tiesAndLegato()
    {
        SeqPattern pattern;
        pattern.length = 4;
        pattern.steps[0] = note(48);
        pattern.steps[1] = tie(true); // tied, and on into step 2
        pattern.steps[2] = note(55);
        Rig rig(pattern);
        rig.engine.setParam(ParamId::PortaMode, position(1, p::kPortaMode)); // Auto
        rig.engine.setParam(ParamId::PortaTime, 0.7f);                       // 330 ms
        rig.run(2 * kStep + int(kBlock) * 4);
        QCOMPARE(rig.notes, (std::vector<int>{48, 55}));
        QVERIFY(rig.ends.empty()); // no gap from step 0 to step 2
        const double pitch = rig.engine.voice().pitch();
        QVERIFY2(pitch > 48.0 && pitch < 54.0, qPrintable(QString::number(pitch))); // gliding up
        rig.run(kStep);
        QCOMPARE(int(rig.ends.size()), 1);
        QVERIFY(std::abs(rig.ends[0] - (2 * kStep + kStep / 2)) < int(kBlock)); // step 2 closes at its gate
    }

    // Keys transpose the running pattern by their distance from middle C, and don't sound themselves.
    void keysTranspose()
    {
        SeqPattern pattern;
        pattern.length = 2;
        pattern.steps[0] = note(60);
        pattern.steps[1] = note(67);
        Rig rig(pattern);
        rig.run(kStep / 2);
        rig.key(65, true); // +5
        rig.run(2 * kStep);
        QCOMPARE(rig.notes, (std::vector<int>{60, 72, 65}));
        QCOMPARE(rig.engine.transpose(), 5);
    }

    // Up to 64 steps: the last plays, then the first again.
    void sixtyFourSteps()
    {
        SeqPattern pattern;
        pattern.length = 64;
        pattern.steps[0] = note(40);
        pattern.steps[63] = note(70);
        Rig rig(pattern);
        rig.run(65 * kStep - kStep / 4);
        QCOMPARE(rig.notes, (std::vector<int>{40, 70, 40}));
        QVERIFY(std::abs(rig.starts[1] - 63 * kStep) < int(kBlock));
    }

    // Switched off (its RUN in the rack) or suspended (a song plays it): no steps.
    void enabledAndSuspended()
    {
        SeqPattern pattern;
        pattern.steps[0] = note(60);
        for (int which = 0; which < 2; ++which) {
            Rig rig(pattern);
            if (which == 0)
                rig.engine.setSequencerEnabled(false);
            else
                rig.engine.setSequencerSuspended(true);
            rig.run(kStep * 2);
            QVERIFY(rig.notes.empty());
            QCOMPARE(rig.engine.sequencerStep(), -1);
        }
    }

    // Loading from the keys: a step at a time; a key down over the one before makes that one legato.
    void loading()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            k.controller.setValue(QStringLiteral("play_mode"), 1.0);
            QVERIFY(k.sequencer.active());
            k.sequencer.pressStep(0, false);
            QCOMPARE(k.sequencer.cursor(), 0);
            k.sequencer.loadNote(48, false);
            k.sequencer.loadNote(51, true); // slurred from 48
            k.sequencer.rest();
            k.sequencer.tie();
            k.sequencer.loadNote(55, false);
            k.sequencer.legato(); // the step just loaded runs into the next
            QCOMPARE(k.sequencer.cursor(), 5);
            QCOMPARE(k.sequencer.stepText(0), QStringLiteral("C3 legato"));
            QCOMPARE(k.sequencer.stepText(1), QStringLiteral("D#3"));
            QCOMPARE(k.sequencer.stepText(2), QStringLiteral("rest"));
            QCOMPARE(k.sequencer.stepText(3), QStringLiteral("tie"));
            QCOMPARE(k.sequencer.stepText(4), QStringLiteral("G3 legato"));
            QCOMPARE(k.sequencer.stepState(3), 2);
            QVERIFY(k.sequencer.stepLegato(4));

            k.sequencer.pressStep(4, true); // shift: step 5 is the last
            QCOMPARE(k.sequencer.length(), 5);
            QCOMPARE(k.sequencer.cursor(), 4);
            k.sequencer.loadNote(60, false); // the cursor wraps at the length
            QCOMPARE(k.sequencer.cursor(), 0);

            k.sequencer.setPatternIndex(3);
            QCOMPARE(k.sequencer.stepState(0), 0);
            QCOMPARE(k.sequencer.cursor(), -1);
            k.sequencer.setPatternIndex(0);
        }
        // Saved, and back next time.
        QVERIFY(QFile::exists(dir.filePath("data/sequences.json")));
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.sequencer.length(), 5);
        QCOMPARE(k.sequencer.stepText(4), QStringLiteral("C4"));
        QCOMPARE(k.sequencer.stepText(0), QStringLiteral("C3 legato"));
        k.sequencer.clear();
        QCOMPARE(k.sequencer.length(), 16);
        QCOMPARE(k.sequencer.stepState(0), 0);
    }

    // Loading from held keys, as the panel does: each key going down is a step;
    // one pressed over the last makes it legato.
    void loadingFromKeys()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(48000.0, 64);
        k.controller.setValue(QStringLiteral("play_mode"), 1.0);
        k.sequencer.pressStep(0, false);
        std::array<float, 64> l, r;
        const auto keys = [&](std::initializer_list<std::pair<int, bool>> changes) {
            for (const auto &[n, down] : changes) {
                if (down)
                    k.controller.noteOn(n);
                else
                    k.controller.noteOff(n);
            }
            krate::processStereo(k.engine, l.data(), r.data(), 64);
            QTest::qWait(40); // the controller polls the held keys
        };
        keys({{60, true}});
        keys({{60, false}});
        keys({{62, true}});
        keys({{64, true}}); // over 62: 62 runs into 64
        keys({{62, false}, {64, false}});
        QCOMPARE(k.sequencer.stepText(0), QStringLiteral("C4"));
        QCOMPARE(k.sequencer.stepText(1), QStringLiteral("D4 legato"));
        QCOMPARE(k.sequencer.stepText(2), QStringLiteral("E4"));
        QCOMPARE(k.sequencer.cursor(), 3);
    }

    // The generator: deterministic per seed, the length kept, notes from the
    // pool over the octaves, Density the share of steps that play.
    void generator()
    {
        const std::array<int, 3> pool{48, 51, 55};
        const SeqPattern a = generatePattern(pool, 2, 0.6, 32, 7);
        QVERIFY(a == generatePattern(pool, 2, 0.6, 32, 7));
        QVERIFY(!(a == generatePattern(pool, 2, 0.6, 32, 8)));
        QCOMPARE(int(a.length), 32);
        QVERIFY(a.steps[0].kind == Kind::Note); // it starts on a note
        int notes = 0, ties = 0, legato = 0;
        for (int s = 0; s < kSeqMaxSteps; ++s) {
            const SeqStep &step = a.steps[std::size_t(s)];
            if (s >= 32) {
                QVERIFY(step == SeqStep{}); // past the length: rests
                continue;
            }
            if (step.kind == Kind::Note) {
                ++notes;
                QVERIFY2(step.note % 12 == 0 || step.note % 12 == 3 || step.note % 12 == 7, qPrintable(QString::number(step.note)));
                QVERIFY(step.note >= 48 && step.note <= 67);
            }
            ties += step.kind == Kind::Tie ? 1 : 0;
            legato += step.legato ? 1 : 0;
            if (step.legato) // a legato step runs into a note or a tie
                QVERIFY(s + 1 < 32 && a.steps[std::size_t(s + 1)].kind != Kind::Rest);
        }
        QVERIFY(notes > 8);

        // Density: 0 plays just the first step, 1 every step; nothing to go on: the default scale.
        const SeqPattern sparse = generatePattern({}, 1, 0.0, 16, 3);
        QVERIFY(sparse.steps[0].kind == Kind::Note);
        for (int s = 1; s < 16; ++s)
            QVERIFY(sparse.steps[std::size_t(s)].kind == Kind::Rest);
        QVERIFY(std::find(kDefaultScale.begin(), kDefaultScale.end(), sparse.steps[0].note) != kDefaultScale.end());
        const SeqPattern full = generatePattern(pool, 1, 1.0, 16, 3);
        for (int s = 0; s < 16; ++s)
            QVERIFY(full.steps[std::size_t(s)].kind != Kind::Rest);
    }

    // GEN on the panel: a new pattern every press, at the pattern's length, from its own notes.
    void generateButton()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.sequencer.pressStep(11, true); // 12 steps
        k.sequencer.pressStep(0, false);
        k.sequencer.loadNote(40, false);
        k.sequencer.loadNote(47, false);
        instance->triggerAction(QStringLiteral("generate"));
        const SeqPattern first = std::as_const(k.sequencer).current();
        QCOMPARE(int(first.length), 12);
        QCOMPARE(k.sequencer.cursor(), -1);
        for (const SeqStep &step : first.steps)
            if (step.kind == Kind::Note)
                QVERIFY(step.note == 40 || step.note == 47);
        k.sequencer.generate();
        QVERIFY(!(std::as_const(k.sequencer).current() == first));
    }

    void fileRoundTripAndRepair()
    {
        SequenceBank bank;
        bank.current = 6;
        bank.patterns[6].length = 40;
        bank.patterns[6].steps[0] = note(36, true);
        bank.patterns[6].steps[1] = tie();
        bank.patterns[6].steps[39] = note(127);
        QVERIFY(sequencesFromJson(toJson(bank)) == bank);

        // Lengths and notes clamped; an unknown step is a rest.
        const QJsonObject json{{"version", 1},
                               {"current", 99},
                               {"patterns", QJsonArray{QJsonObject{
                                                {"length", 200},
                                                {"steps", QJsonArray{QJsonObject{{"note", 300}}, QJsonObject{{"x", 1}}}}}}}};
        const SequenceBank repaired = sequencesFromJson(json);
        QCOMPARE(repaired.current, kSeqPatterns - 1);
        QCOMPARE(int(repaired.patterns[0].length), kSeqMaxSteps);
        QCOMPARE(int(repaired.patterns[0].steps[0].note), 127);
        QVERIFY(repaired.patterns[0].steps[1].kind == Kind::Rest);
    }
};

QTEST_GUILESS_MAIN(TestSequencer)
#include "tst_sequencer.moc"
