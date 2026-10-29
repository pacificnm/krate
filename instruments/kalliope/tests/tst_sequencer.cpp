#include <QtTest>

#include <algorithm>
#include <utility>
#include <array>
#include <vector>

#include <QTemporaryDir>

#include "core/Transport.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "kalliope/dsp/Engine.h"
#include "kalliope/dsp/Sequence.h"

using namespace kalliope;

// The step sequencer and the pattern generator (docs/SEQUENCER.md).
class TestSequencer : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 16;
    static constexpr int kStep = 6000; // a 1/16 step at 120 BPM

    static SeqStep notes(std::initializer_list<int> list)
    {
        SeqStep s;
        for (int n : list)
            s.notes[std::size_t(s.count++)] = std::uint8_t(n);
        return s;
    }
    static SeqStep tie()
    {
        SeqStep s;
        s.tie = true;
        return s;
    }

    struct Rig
    {
        Engine engine;
        krate::Transport transport;
        std::vector<krate::MidiEvent> pending;

        explicit Rig(const SeqPattern &pattern)
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::Chorus, 0.0f);
            engine.setParam(ParamId::PlayMode, 1.0f); // Sequencer
            engine.setSequence(pattern);
            transport.setBpm(120.0);
            transport.setPlaying(true);
        }
        void key(int note, bool down) { pending.push_back({0, 3, {std::uint8_t(down ? 0x90 : 0x80), std::uint8_t(note), 100}}); }
        void run(int frames)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (int f = 0; f < frames; f += int(kBlock)) {
                const krate::TransportState state = transport.advance(kBlock, kRate);
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()),
                                     &state);
                pending.clear();
            }
        }
        std::vector<int> gated() const
        {
            std::vector<int> n;
            for (int v = 0; v < Engine::kVoices; ++v)
                if (engine.voiceActive(v) && engine.voiceGate(v))
                    n.push_back(engine.voiceNote(v));
            std::sort(n.begin(), n.end());
            return n;
        }
    };

private slots:
    // Notes, a chord, a rest and a tie, each sampled a quarter into its step.
    void playsTheSteps()
    {
        SeqPattern p;
        p.length = 4;
        p.steps[0] = notes({48});
        p.steps[1] = notes({55, 51, 58});
        p.steps[2] = SeqStep{}; // rest
        p.steps[3] = notes({60});
        Rig rig(p);
        rig.run(kStep / 4);
        QCOMPARE(rig.gated(), (std::vector<int>{48}));
        QCOMPARE(rig.engine.sequencerStep(), 0);
        rig.run(kStep / 2); // past the gate (half a step)
        QVERIFY(rig.gated().empty());
        rig.run(kStep / 2);
        QCOMPARE(rig.gated(), (std::vector<int>{51, 55, 58}));
        rig.run(kStep);
        QVERIFY(rig.gated().empty()); // the rest
        rig.run(kStep);
        QCOMPARE(rig.gated(), (std::vector<int>{60}));
        rig.run(kStep);
        QCOMPARE(rig.gated(), (std::vector<int>{48})); // length 4: back to step 1
        QCOMPARE(rig.engine.sequencerStep(), 0);
    }

    void tiesHoldTheNotes()
    {
        SeqPattern p;
        p.length = 4;
        p.steps[0] = notes({50});
        p.steps[1] = tie();
        p.steps[2] = tie();
        Rig rig(p);
        rig.run(kStep / 4);
        QCOMPARE(rig.gated(), (std::vector<int>{50}));
        rig.run(2 * kStep); // through steps 2 and 3's starts and gates
        QCOMPARE(rig.gated(), (std::vector<int>{50}));
        rig.run(kStep / 2); // step 3's gate: the next (4) is a rest
        QVERIFY(rig.gated().empty());
    }

    // While running, a key transposes from middle C and doesn't sound itself.
    void keysTranspose()
    {
        SeqPattern p;
        p.length = 2;
        p.steps[0] = notes({48});
        p.steps[1] = notes({52});
        Rig rig(p);
        rig.run(kStep / 4);
        rig.key(65, true); // +5
        rig.run(kStep / 8);
        QCOMPARE(rig.gated(), (std::vector<int>{48})); // the key doesn't sound
        rig.key(65, false);
        rig.run(3 * kStep / 4); // an eighth of a step into step 2
        QCOMPARE(rig.gated(), (std::vector<int>{57}));
        QCOMPARE(rig.engine.transpose(), 5);
        // Stopped: keys play as usual (to hear what you record).
        rig.transport.setPlaying(false);
        rig.run(kBlock);
        rig.key(70, true);
        rig.run(kBlock);
        QCOMPARE(rig.gated(), (std::vector<int>{70}));
        QCOMPARE(rig.engine.sequencerStep(), -1);
    }

    void generatorIsDeterministicAndFollowsDensity()
    {
        const std::array<int, 3> pool{60, 63, 67};
        const SeqPattern a = generatePattern(pool, 2, 0.6, 16, 42);
        QVERIFY(a == generatePattern(pool, 2, 0.6, 16, 42));
        QVERIFY(a != generatePattern(pool, 2, 0.6, 16, 43));
        QCOMPARE(int(a.length), 16);
        for (const SeqStep &s : a.steps)
            for (int i = 0; i < s.count; ++i) {
                const int n = s.notes[std::size_t(i)];
                QVERIFY(n == 60 || n == 63 || n == 67 || n == 72 || n == 75 || n == 79);
            }

        // Density 0: only step 1 plays. Density 1: no rests (ties allowed).
        const SeqPattern sparse = generatePattern(pool, 1, 0.0, 8, 7);
        QCOMPARE(int(sparse.steps[0].count), 1);
        for (int s = 1; s < 8; ++s)
            QVERIFY(sparse.steps[std::size_t(s)].count == 0 && !sparse.steps[std::size_t(s)].tie);
        const SeqPattern full = generatePattern(pool, 1, 1.0, 16, 7);
        for (int s = 0; s < 16; ++s)
            QVERIFY(full.steps[std::size_t(s)].count > 0 || full.steps[std::size_t(s)].tie);

        // No notes given: the default scale.
        const SeqPattern scale = generatePattern({}, 1, 1.0, 16, 3);
        QVERIFY(std::find(kDefaultScale.begin(), kDefaultScale.end(), scale.steps[0].notes[0]) != kDefaultScale.end());
    }

    // The controller: recording from the keys, REST / TIE, length, clear, generate, storage.
    void recordingAndStorage()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            k.engine.prepare(kRate, 64);
            std::array<float, 64> l, r;
            const auto block = [&] { krate::processStereo(k.engine, l.data(), r.data(), 64); };
            k.controller.setValue(QStringLiteral("play_mode"), 1.0);
            QVERIFY(k.sequencer.active());

            k.sequencer.pressStep(0, false);
            QCOMPARE(k.sequencer.cursor(), 0);
            // A chord: keys down, then all up, writes them and moves on.
            k.controller.noteOn(60);
            k.controller.noteOn(64);
            block();
            QTest::qWait(40);
            k.controller.noteOff(60);
            k.controller.noteOff(64);
            block();
            QTRY_COMPARE(k.sequencer.cursor(), 1);
            QCOMPARE(k.sequencer.stepState(0), 1);
            QCOMPARE(k.sequencer.stepText(0), QStringLiteral("C4 E4"));

            k.sequencer.tie();
            k.sequencer.rest();
            QCOMPARE(k.sequencer.stepState(1), 2);
            QCOMPARE(k.sequencer.stepState(2), 0);
            QCOMPARE(k.sequencer.cursor(), 3);
            k.sequencer.pressStep(5, true); // length 6
            QCOMPARE(k.sequencer.length(), 6);

            k.sequencer.setPatternIndex(3);
            k.sequencer.generate();
            QCOMPARE(k.sequencer.length(), 16);
            QCOMPARE(k.sequencer.stepState(0), 1);
        }
        // The next start: both patterns are back.
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.sequencer.patternIndex(), 3);
        QCOMPARE(k.sequencer.stepState(0), 1);
        k.sequencer.setPatternIndex(0);
        QCOMPARE(k.sequencer.length(), 6);
        QCOMPARE(k.sequencer.stepText(0), QStringLiteral("C4 E4"));
        QCOMPARE(k.sequencer.stepState(1), 2);
        k.sequencer.clear();
        QCOMPARE(k.sequencer.length(), 16);
        QCOMPARE(k.sequencer.stepState(0), 0);
    }

    // GENERATE uses the keys held at that moment.
    void generateFromHeldKeys()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(kRate, 64);
        std::array<float, 64> l, r;
        k.controller.setValue(QStringLiteral("arp_range"), 0.0); // 1 octave
        k.controller.noteOn(62);
        k.controller.noteOn(69);
        krate::processStereo(k.engine, l.data(), r.data(), 64);
        k.sequencer.generate();
        for (int s = 0; s < 16; ++s) {
            const SeqStep &step = std::as_const(k.sequencer).current().steps[std::size_t(s)];
            for (int i = 0; i < step.count; ++i)
                QVERIFY(step.notes[std::size_t(i)] == 62 || step.notes[std::size_t(i)] == 69);
        }
    }
};

QTEST_GUILESS_MAIN(TestSequencer)
#include "tst_sequencer.moc"
