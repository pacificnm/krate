#include <QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <vector>

#include "app/MidiLearnController.h"
#include "app/Persistence.h"
#include "dsp/Engine.h"

using namespace k303;
namespace kp = k303::persistence;

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
        engine.setCcMapping(21, int(ParamId::Cutoff));
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
        engine.setCcMapping(21, int(ParamId::Decay));
        MidiEvent ch10 = cc(21, 127);
        ch10.data[0] = 0xB9;
        block(engine, {ch10});
        QCOMPARE(engine.params().get(ParamId::Decay), 1.0f);
    }

    void switchFlipsAtHalfway()
    {
        Engine engine;
        engine.setCcMapping(30, int(ParamId::Waveform));
        block(engine, {cc(30, 63)});
        QCOMPARE(engine.params().get(ParamId::Waveform), 0.0f);
        block(engine, {cc(30, 64)});
        QCOMPARE(engine.params().get(ParamId::Waveform), 1.0f);
    }

    // Every multi-position control snaps to a position, not just Waveform.
    void selectorsSnapToPositions()
    {
        Engine engine;
        engine.setCcMapping(31, int(ParamId::PlayMode));
        block(engine, {cc(31, 70)});  // 70/127 = 0.55 -> position 2 of 0..4
        QCOMPARE(engine.params().get(ParamId::PlayMode), 0.5f);
        engine.setCcMapping(32, int(ParamId::Scale));
        block(engine, {cc(32, 127)});
        QCOMPARE(engine.params().get(ParamId::Scale), 1.0f);
    }

    void learnBindsFirstCcAndAppliesIt()
    {
        Engine engine;
        engine.setLearnTarget(int(ParamId::Resonance));
        block(engine, {cc(74, 100), cc(75, 10)});
        QCOMPARE(engine.ccMapping(74), int(ParamId::Resonance));
        QCOMPARE(engine.ccMapping(75), -1); // only the first CC binds
        QCOMPARE(engine.learnTarget(), -1);
        QCOMPARE(engine.params().get(ParamId::Resonance), 100.0f / 127.0f);

        const auto ev = engine.popEvent();
        QVERIFY(ev.has_value());
        QCOMPARE(int(ev->kind), int(EngineEvent::Kind::CcLearned));
        QCOMPARE(int(ev->a), 74);
        QCOMPARE(int(ev->b), int(ParamId::Resonance));
    }

    void rebindingMovesTheCc()
    {
        Engine engine;
        engine.setCcMapping(21, int(ParamId::Cutoff));
        engine.setLearnTarget(int(ParamId::Accent));
        block(engine, {cc(21, 64)});
        QCOMPARE(engine.ccMapping(21), int(ParamId::Accent));
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
        engine.setRunning(true);
        block(engine);
        QVERIFY(block(engine, {noteOn(48)}, 2400) < 1e-3f); // silent pattern, key ignored
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

    // --- Mapping file ---------------------------------------------------------

    void mapRoundTrip()
    {
        kp::MidiMap map;
        map.inputPortMatch = QStringLiteral("My Synth");
        map.ccToParam[21] = int(ParamId::Cutoff);
        map.ccToParam[22] = int(ParamId::Cutoff);
        map.ccToParam[127] = int(ParamId::Tempo);
        QVERIFY(kp::saveMidiMap(m_path, map));

        kp::MidiMap back;
        QCOMPARE(kp::loadMidiMap(m_path, &back), kp::LoadResult::Loaded);
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
        QCOMPARE(kp::loadMidiMap(m_path, &map), kp::LoadResult::Loaded);
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
            QCOMPARE(engine.learnTarget(), -1); // arming needs learn mode

            learn.setActive(true);
            learn.arm(QStringLiteral("cutoff"));
            QCOMPARE(learn.armedParam(), QStringLiteral("cutoff"));
            QCOMPARE(engine.learnTarget(), int(ParamId::Cutoff));

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
        QCOMPARE(fresh.ccMapping(21), int(ParamId::Cutoff));
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
        QCOMPARE(engine.learnTarget(), -1);
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
        QCOMPARE(engine.ccMapping(40), -1);

        learn.arm(QStringLiteral("volume"));
        block(engine, {cc(7, 1)});
        learn.drainEvents();
        learn.clearAll();
        QCOMPARE(engine.ccMapping(7), -1);
        kp::MidiMap saved;
        QCOMPARE(kp::loadMidiMap(m_path, &saved), kp::LoadResult::Loaded);
        QCOMPARE(saved.ccToParam[7], -1);
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    int m_counter = 0;
};

QTEST_GUILESS_MAIN(TestMidi)
#include "tst_midi.moc"
