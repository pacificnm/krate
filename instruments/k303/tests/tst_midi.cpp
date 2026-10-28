#include <QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <vector>

#include <array>
#include <string>

#include "app/MidiLearnController.h"
#include "core/Midi.h"
#include "app/MidiMapFile.h"
#include "dsp/Engine.h"
#include "TestHost.h"

using namespace k303;
namespace kp = krate::files;

namespace {

MidiEvent cc(int number, int value, std::uint32_t frame = 0)
{
    return MidiEvent{frame, 3, {0xB0, std::uint8_t(number), std::uint8_t(value)}};
}

MidiEvent noteOn(int note, int velocity = 80, std::uint32_t frame = 0)
{
    return MidiEvent{frame, 3, {0x90, std::uint8_t(note), std::uint8_t(velocity)}};
}

MidiEvent noteOff(int note, std::uint32_t frame = 0)
{
    return MidiEvent{frame, 3, {0x80, std::uint8_t(note), 0}};
}

// Process one block with the given events; return its peak level.
float block(Engine &engine, std::vector<MidiEvent> events = {}, std::uint32_t frames = 64)
{
    std::vector<float> l(frames), r(frames);
    engine.takePeak();
    krate::processStereo(engine, l.data(), r.data(), frames, events.data(), std::uint32_t(events.size()));
    return engine.takePeak();
}

float settle(Engine &engine, std::uint32_t frames)
{
    return block(engine, {}, frames);
}

// A minimal non-k303 instrument: three parameters, no sound. Proves the
// shared MIDI pieces don't depend on k303.
std::string fmtPlain(float v) { return std::to_string(v); }

constexpr krate::ParamDescriptor kFakeParams[] = {
    {"alpha", "Alpha", 0.25f, 0, &fmtPlain},
    {"mode", "Mode", 0.0f, 3, &fmtPlain},
    {"gate", "Gate", 0.0f, 2, &fmtPlain},
};

class FakeInstrument final : public krate::IInstrument
{
public:
    void prepare(double, std::uint32_t) noexcept override {}
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kFakeParams; }
    float param(int i) const noexcept override { return values[std::size_t(i)]; }
    void setParam(int i, float v) noexcept override { values[std::size_t(i)] = v; }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return 0; }
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &learn; }
    void process(const krate::AudioBlock &block) noexcept override
    {
        for (std::uint32_t i = 0; i < block.midiCount; ++i) {
            const auto m = krate::decode(block.midi[i]);
            if (m.type == krate::MidiMessage::Type::ControlChange)
                if (const auto r = learn.controlChange(m.data1, m.data2); r.param >= 0)
                    values[std::size_t(r.param)] = r.value;
        }
    }

    std::array<float, 3> values{0.25f, 0.0f, 0.0f};
    krate::MidiLearnMap learn{kFakeParams};
};

} // namespace

class TestMidi : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("midimap-%1.json").arg(++m_counter));
    }

    void mappedCcMovesParameter()
    {
        Engine engine;
        engine.midiLearnMap()->setMapping(21, int(ParamId::Cutoff));
        block(engine, {cc(21, 127)});
        QCOMPARE(engine.params().get(ParamId::Cutoff), 1.0f);
        block(engine, {cc(21, 0)});
        QCOMPARE(engine.params().get(ParamId::Cutoff), 0.0f);
        // The GUI is told which parameters MIDI changed.
        QVERIFY(engine.params().takeChangedByAudio() & (1u << int(ParamId::Cutoff)));

        block(engine, {cc(22, 127)}); // unmapped: no effect
        QCOMPARE(engine.params().get(ParamId::Resonance), paramDescriptor(ParamId::Resonance).defaultValue);
    }

    void anyChannelIsAccepted()
    {
        Engine engine;
        engine.midiLearnMap()->setMapping(21, int(ParamId::Decay));
        MidiEvent ch10 = cc(21, 127);
        ch10.data[0] = 0xB9;
        block(engine, {ch10});
        QCOMPARE(engine.params().get(ParamId::Decay), 1.0f);
    }

    void switchFlipsAtHalfway()
    {
        Engine engine;
        engine.midiLearnMap()->setMapping(30, int(ParamId::Waveform));
        block(engine, {cc(30, 63)});
        QCOMPARE(engine.params().get(ParamId::Waveform), 0.0f);
        block(engine, {cc(30, 64)});
        QCOMPARE(engine.params().get(ParamId::Waveform), 1.0f);
    }

    // Every multi-position control snaps to a position, not just Waveform.
    void selectorsSnapToPositions()
    {
        Engine engine;
        engine.midiLearnMap()->setMapping(31, int(ParamId::PlayMode));
        block(engine, {cc(31, 70)});  // 70/127 = 0.55 -> position 2 of 0..4
        QCOMPARE(engine.params().get(ParamId::PlayMode), 0.5f);
        engine.midiLearnMap()->setMapping(32, int(ParamId::Scale));
        block(engine, {cc(32, 127)});
        QCOMPARE(engine.params().get(ParamId::Scale), 1.0f);
    }

    void learnBindsFirstCcAndAppliesIt()
    {
        Engine engine;
        engine.midiLearnMap()->setLearnTarget(int(ParamId::Resonance));
        block(engine, {cc(74, 100), cc(75, 10)});
        QCOMPARE(engine.midiLearnMap()->mapping(74), int(ParamId::Resonance));
        QCOMPARE(engine.midiLearnMap()->mapping(75), -1); // only the first CC binds
        QCOMPARE(engine.midiLearnMap()->learnTarget(), -1);
        QCOMPARE(engine.params().get(ParamId::Resonance), 100.0f / 127.0f);

        const auto learned = engine.midiLearnMap()->popLearned();
        QVERIFY(learned.has_value());
        QCOMPARE(int(learned->cc), 74);
        QCOMPARE(int(learned->param), int(ParamId::Resonance));
        QVERIFY(!engine.midiLearnMap()->popLearned().has_value());
    }

    void rebindingMovesTheCc()
    {
        Engine engine;
        engine.midiLearnMap()->setMapping(21, int(ParamId::Cutoff));
        engine.midiLearnMap()->setLearnTarget(int(ParamId::Accent));
        block(engine, {cc(21, 64)});
        QCOMPARE(engine.midiLearnMap()->mapping(21), int(ParamId::Accent));
    }

    void keysPlayWhileStopped()
    {
        Engine engine;
        QVERIFY(block(engine, {noteOn(48)}, 2400) > 0.01f);
        QVERIFY(block(engine, {noteOff(48)}, 4800) > 0.0f); // release tail
        QVERIFY(settle(engine, 480) < 1e-3f);
    }

    // Last-note priority: releasing the newest key returns to the one still held.
    void legatoReturnsToHeldKey()
    {
        Engine engine;
        block(engine, {noteOn(48)}, 2400);
        block(engine, {noteOn(55)}, 2400);
        block(engine, {noteOff(55)}, 4800);
        QVERIFY(settle(engine, 480) > 0.01f); // 48 still sounding
        block(engine, {noteOff(48)}, 4800);
        QVERIFY(settle(engine, 480) < 1e-3f);
    }

    void releasingAnOlderKeyKeepsTheNewest()
    {
        Engine engine;
        block(engine, {noteOn(48)}, 2400);
        block(engine, {noteOn(55)}, 2400);
        block(engine, {noteOff(48)}, 4800);
        QVERIFY(settle(engine, 480) > 0.01f); // 55 still sounding
    }

    void noteOnVelocityZeroIsNoteOff()
    {
        Engine engine;
        block(engine, {noteOn(48)}, 2400);
        block(engine, {noteOn(48, 0)}, 4800);
        QVERIFY(settle(engine, 480) < 1e-3f);
    }

    void keysIgnoredWhileSequencerRuns()
    {
        Engine engine;
        Command clear;
        clear.kind = Command::Kind::LoadPattern;
        for (auto &s : clear.pattern.steps)
            s.gate = Gate::Rest;
        engine.post(clear);
        TestHost host(engine);
        host.play();
        host.run(64);
        engine.takePeak();
        host.run(2400, {noteOn(48)});
        QVERIFY(engine.takePeak() < 1e-3f); // silent pattern, key ignored
        // ...but the GUI still hears about the key (for step record later).
        bool sawNote = false;
        while (const auto ev = engine.popEvent())
            sawNote = sawNote || ev->kind == EngineEvent::Kind::NoteOn;
        QVERIFY(sawNote);
    }

    void eventsApplyAtTheirFrame()
    {
        Engine engine;
        std::vector<float> l(256), r(256);
        const MidiEvent ev = noteOn(48, 80, 200);
        krate::processStereo(engine, l.data(), r.data(), 256, &ev, 1);
        for (int i = 0; i < 200; ++i)
            QCOMPARE(l[i], 0.0f);       // silent before the event
        float after = 0.0f;
        for (int i = 200; i < 256; ++i)
            after = std::max(after, std::abs(l[i]));
        QVERIFY(after > 0.0f);
    }

    // --- Shared pieces ---------------------------------------------------------

    void decodeMessages()
    {
        using T = krate::MidiMessage::Type;
        auto m = krate::decode(noteOn(60, 100));
        QCOMPARE(int(m.type), int(T::NoteOn));
        QCOMPARE(int(m.data1), 60);
        QCOMPARE(int(m.data2), 100);
        QCOMPARE(int(krate::decode(noteOn(60, 0)).type), int(T::NoteOff)); // velocity 0
        QCOMPARE(int(krate::decode(noteOff(60)).type), int(T::NoteOff));
        MidiEvent ch5 = cc(7, 99);
        ch5.data[0] = 0xB4;
        m = krate::decode(ch5);
        QCOMPARE(int(m.type), int(T::ControlChange));
        QCOMPARE(int(m.channel), 4);
        QCOMPARE(int(krate::decode(MidiEvent{0, 1, {0xF8, 0, 0}}).type), int(T::Other)); // clock
        QCOMPARE(int(krate::decode(MidiEvent{}).type), int(T::Other));                    // empty
    }

    void learnMapOnItsOwn()
    {
        krate::MidiLearnMap map(kFakeParams);
        QCOMPARE(map.controlChange(10, 64).param, krate::MidiLearnMap::kUnmapped);

        map.setLearnTarget(1); // "mode": 3 positions
        auto r = map.controlChange(10, 80);
        QCOMPARE(r.param, 1);
        QCOMPARE(r.value, 0.5f);  // 80/127 snaps to the middle position
        QCOMPARE(map.mapping(10), 1);
        QCOMPARE(map.learnTarget(), krate::MidiLearnMap::kUnmapped);
        QVERIFY(map.popLearned().has_value());

        map.setMapping(11, 0); // "alpha": continuous
        QCOMPARE(map.controlChange(11, 127).value, 1.0f);
        map.setMapping(12, 99); // out of range: ignored
        QCOMPARE(map.mapping(12), krate::MidiLearnMap::kUnmapped);
        map.setLearnTarget(99);
        QCOMPARE(map.learnTarget(), krate::MidiLearnMap::kUnmapped);
        map.clearAll();
        QCOMPARE(map.mapping(10), krate::MidiLearnMap::kUnmapped);
    }

    // The learn controller drives an instrument it knows nothing about.
    void learnControllerWithAnotherInstrument()
    {
        {
            FakeInstrument fake;
            MidiLearnController learn(fake, m_path, nullptr);
            learn.setActive(true);
            learn.arm(QStringLiteral("gate"));
            QCOMPARE(fake.learn.learnTarget(), 2);

            std::vector<float> l(64), r(64);
            const MidiEvent ev = cc(20, 100);
            krate::processStereo(fake, l.data(), r.data(), 64, &ev, 1);
            learn.drainEvents();
            QCOMPARE(learn.mappingFor(QStringLiteral("gate")), QStringLiteral("20"));
            QCOMPARE(fake.values[2], 1.0f); // switch: 100 >= half -> on
        }
        // Saved by key, reloaded into a fresh instrument.
        FakeInstrument fresh;
        MidiLearnController learn(fresh, m_path, nullptr);
        QCOMPARE(fresh.learn.mapping(20), 2);
        kp::MidiMap saved;
        QCOMPARE(kp::loadMidiMap(m_path, kFakeParams, &saved), kp::LoadResult::Loaded);
        QCOMPARE(saved.ccToParam[20], 2);
    }

    // --- Mapping file ---------------------------------------------------------

    void mapRoundTrip()
    {
        kp::MidiMap map;
        map.inputPortMatch = QStringLiteral("My Synth");
        map.ccToParam[21] = int(ParamId::Cutoff);
        map.ccToParam[22] = int(ParamId::Cutoff);
        map.ccToParam[127] = int(ParamId::Tempo);
        QVERIFY(kp::saveMidiMap(m_path, kParams, map));

        kp::MidiMap back;
        QCOMPARE(kp::loadMidiMap(m_path, kParams, &back), kp::LoadResult::Loaded);
        QCOMPARE(back.inputPortMatch, QStringLiteral("My Synth"));
        QVERIFY(back.ccToParam == map.ccToParam);
    }

    void mapSkipsBadEntries()
    {
        QFile f(m_path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({ "version": 1, "mappings": [
            { "cc": 21, "param": "cutoff" }, { "cc": 200, "param": "cutoff" },
            { "cc": 5, "param": "warp-drive" }, { "cc": 6 } ] })");
        f.close();
        kp::MidiMap map;
        QCOMPARE(kp::loadMidiMap(m_path, kParams, &map), kp::LoadResult::Loaded);
        QCOMPARE(map.ccToParam[21], int(ParamId::Cutoff));
        QCOMPARE(map.ccToParam[5], -1);
        QCOMPARE(map.ccToParam[6], -1);
        QCOMPARE(map.inputPortMatch, QStringLiteral("Impact LX49")); // default kept
    }

    // --- MidiLearn controller -------------------------------------------------

    void learnEndToEnd()
    {
        Engine engine;
        {
            MidiLearnController learn(engine, m_path, nullptr);
            learn.arm(QStringLiteral("cutoff"));
            QCOMPARE(engine.midiLearnMap()->learnTarget(), -1); // arming needs learn mode

            learn.setActive(true);
            learn.arm(QStringLiteral("cutoff"));
            QCOMPARE(learn.armedParam(), QStringLiteral("cutoff"));
            QCOMPARE(engine.midiLearnMap()->learnTarget(), int(ParamId::Cutoff));

            QSignalSpy mappings(&learn, &MidiLearnController::mappingsChanged);
            block(engine, {cc(21, 90)});
            learn.drainEvents();
            QCOMPARE(mappings.count(), 1);
            QCOMPARE(learn.armedParam(), QString());
            QCOMPARE(learn.mappingFor(QStringLiteral("cutoff")), QStringLiteral("21"));
            QVERIFY(QFile::exists(m_path)); // saved

            learn.setActive(false);
        }
        // A new controller loads the saved map into a fresh engine.
        Engine fresh;
        MidiLearnController learn(fresh, m_path, nullptr);
        QCOMPARE(fresh.midiLearnMap()->mapping(21), int(ParamId::Cutoff));
        QCOMPARE(learn.mappingFor(QStringLiteral("cutoff")), QStringLiteral("21"));
    }

    void armTogglesAndLeavingLearnDisarms()
    {
        Engine engine;
        MidiLearnController learn(engine, m_path, nullptr);
        learn.setActive(true);
        learn.arm(QStringLiteral("decay"));
        learn.arm(QStringLiteral("decay")); // second click disarms
        QCOMPARE(learn.armedParam(), QString());
        learn.arm(QStringLiteral("decay"));
        learn.setActive(false);
        QCOMPARE(learn.armedParam(), QString());
        QCOMPARE(engine.midiLearnMap()->learnTarget(), -1);
    }

    void clearAndClearAll()
    {
        Engine engine;
        MidiLearnController learn(engine, m_path, nullptr);
        learn.setActive(true);
        learn.arm(QStringLiteral("accent"));
        block(engine, {cc(40, 1)});
        learn.drainEvents();
        learn.arm(QStringLiteral("accent"));
        block(engine, {cc(41, 1)});
        learn.drainEvents();
        QCOMPARE(learn.mappingFor(QStringLiteral("accent")), QStringLiteral("40 41"));

        learn.clear(QStringLiteral("accent"));
        QCOMPARE(learn.mappingFor(QStringLiteral("accent")), QString());
        QCOMPARE(engine.midiLearnMap()->mapping(40), -1);

        learn.arm(QStringLiteral("volume"));
        block(engine, {cc(7, 1)});
        learn.drainEvents();
        learn.clearAll();
        QCOMPARE(engine.midiLearnMap()->mapping(7), -1);
        kp::MidiMap saved;
        QCOMPARE(kp::loadMidiMap(m_path, kParams, &saved), kp::LoadResult::Loaded);
        QCOMPARE(saved.ccToParam[7], -1);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_counter = 0;
};

QTEST_GUILESS_MAIN(TestMidi)
#include "tst_midi.moc"
