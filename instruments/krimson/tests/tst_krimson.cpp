#include <QtTest>

#include <array>
#include <cmath>
#include <set>
#include <string_view>

#include <QFile>
#include <QTemporaryDir>

#include "app/InstrumentMidiLearn.h"
#include "core/Transport.h"
#include "krimson/app/KrimsonInstrument.h"
#include "krimson/dsp/Params.h"

using namespace krimson;

class TestKrimson : public QObject
{
    Q_OBJECT

private slots:
    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), ParamCount);
        QVERIFY(ParamCount <= 64);
        QCOMPARE(SlotParamCount, ParamCount - 3);
        QCOMPARE(paramDescriptor(ParamId::Osc1Wave).key, std::string_view("osc1_wave"));
        QCOMPARE(paramDescriptor(ParamId::SlotPan).key, std::string_view("slot_pan"));
        QCOMPARE(paramDescriptor(ParamId::Octave).key, std::string_view("octave"));
        std::set<std::string_view> keys;
        for (const auto &info : kParams) {
            QVERIFY(keys.insert(info.key).second);
            for (float v : {0.0f, 0.5f, 1.0f})
                QVERIFY(!info.format(v).empty());
            QCOMPARE(krate::snapToDetent(info, info.defaultValue), info.defaultValue);
        }
        const auto text = [](ParamId id, float v) { return paramDescriptor(id).format(v); };
        QCOMPARE(text(ParamId::FilterType, 1.0f), std::string("Notch + LP"));
        QCOMPARE(text(ParamId::Osc2Semi, 1.0f), std::string("+24"));
        QCOMPARE(text(ParamId::Osc2Semi, 0.5f), std::string("+0"));
        QCOMPARE(text(ParamId::VoiceMode, 1.0f), std::string("Unison"));
        QCOMPARE(text(ParamId::Lfo1Wave, 0.75f), std::string("S&H"));
        QCOMPARE(text(ParamId::SlotPan, 0.5f), std::string("C"));
        QCOMPARE(text(ParamId::AenvA, 0.0f), std::string("0.5 ms"));
    }

    void entryDescribesKrimson()
    {
        const krate::InstrumentEntry entry = registryEntry();
        QCOMPARE(entry.id, QStringLiteral("krimson"));
        QCOMPARE(entry.qmlModule, QStringLiteral("Krimson"));
        QCOMPARE(entry.defaultMidiChannel, 2);
        QVERIFY(entry.actions.contains(QStringLiteral("slotD")));
        QVERIFY(entry.actions.contains(QStringLiteral("morph")));
        QCOMPARE(entry.actions.size(), 4 + 8 + 8 + 6);
    }

    // The table shows the edited slot; the instrument's own parameters are shared.
    void slotsAreSeparate()
    {
        Engine engine;
        engine.setParam(ParamId::Cutoff, 0.2f); // slot A
        engine.setParam(ParamId::Volume, 0.5f);
        engine.takeParamsChangedByAudio();
        engine.setEditedSlot(2);
        QVERIFY((engine.takeParamsChangedByAudio() >> int(ParamId::Cutoff)) & 1u); // the panel refreshes
        QCOMPARE(engine.param(ParamId::Cutoff), paramDescriptor(ParamId::Cutoff).defaultValue);
        engine.setParam(ParamId::Cutoff, 0.9f); // slot C
        QCOMPARE(engine.param(ParamId::Volume), 0.5f);
        QCOMPARE(engine.slotParam(0, ParamId::Cutoff), 0.2f);
        QCOMPARE(engine.slotParam(2, ParamId::Cutoff), 0.9f);
        engine.setEditedSlot(0);
        QCOMPARE(engine.param(ParamId::Cutoff), 0.2f);
    }

    // A learned CC edits the edited slot.
    void learnedCcEditsTheEditedSlot()
    {
        Engine engine;
        engine.prepare(48000.0, 64);
        engine.setEditedSlot(1);
        engine.midiLearnMap()->setMapping(30, int(ParamId::Res));
        std::array<float, 64> l, r;
        const krate::MidiEvent cc{0, 3, {0xB0, 30, 127}};
        krate::processStereo(engine, l.data(), r.data(), 64, &cc, 1);
        QCOMPARE(engine.slotParam(1, ParamId::Res), 1.0f);
        QCOMPARE(engine.slotParam(0, ParamId::Res), paramDescriptor(ParamId::Res).defaultValue);
    }

    // The on-screen keyboard and wheels play the lit slots; Octave shifts MIDI keys
    // only, and a key's note-off matches its note-on even if Octave changed.
    void keyboardWheelsAndOctave()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        const auto block = [&](const krate::MidiEvent *e = nullptr) {
            krate::processStereo(k.engine, l.data(), r.data(), 64, e, e ? 1 : 0);
        };
        k.slotSetup.pressSlot(1, false); // B lit
        k.controller.noteOn(64);
        block();
        QVERIFY(k.controller.noteHeld(64));
        QCOMPARE(k.engine.voiceSlot(0), 1);
        k.controller.noteOff(64);
        k.controller.pitchBend(1.0);
        k.controller.modWheel(0.5);
        block();
        QVERIFY(!k.controller.noteHeld(64));
        QVERIFY(k.engine.settings(1).bendSemis > 1.9);

        k.controller.setValue(QStringLiteral("octave"), position(4, positions::kOctave)); // +1
        const krate::MidiEvent on{0, 3, {0x90, 60, 100}}, off{0, 3, {0x80, 60, 0}};
        block(&on);
        QVERIFY(k.engine.noteHeld(72));
        k.controller.setValue(QStringLiteral("octave"), position(3, positions::kOctave)); // 0, while held
        block(&off);
        QVERIFY(!k.engine.noteHeld(72));
        k.controller.noteOn(60); // on-screen: as shown, not shifted
        block();
        QVERIFY(k.engine.noteHeld(60));
        k.controller.noteOff(60);
        block();

        // A song's notes play as written: Octave is for the player's keys.
        k.controller.setValue(QStringLiteral("octave"), position(0, positions::kOctave)); // -3
        const krate::MidiEvent songOn{0, 3, {0x90, 45, 100}, true}, songOff{0, 3, {0x80, 45, 0}, true};
        block(&songOn);
        QVERIFY(k.engine.noteHeld(45));
        block(&songOff);
        QVERIFY(!k.engine.noteHeld(45));
    }

    void controllerSwitchesSlotsAndDrivesTheTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(k.controller.ownsTransport());
        k.controller.setValue(QStringLiteral("tempo"), units::tempoValue(150.0));
        QVERIFY(std::abs(transport.bpm() - 150.0) < 1e-3);
        k.controller.setValue(QStringLiteral("cutoff"), 0.3);
        instance->triggerAction(QStringLiteral("slotB"));
        QCOMPARE(k.controller.editedSlot(), 1);
        // Slot B's own value (a first run starts B on program 12).
        QCOMPARE(k.controller.value(QStringLiteral("cutoff")), double(k.engine.slotParam(1, ParamId::Cutoff)));
        QVERIFY(std::abs(k.controller.value(QStringLiteral("cutoff")) - 0.3) > 0.01);
        instance->triggerAction(QStringLiteral("slotA"));
        QCOMPARE(k.controller.value(QStringLiteral("cutoff")), double(0.3f));
    }

    // The panel's wheels follow a MIDI keyboard's: the engine publishes where
    // pitch bend and the mod wheel were last put, and the controller passes it on.
    void wheelsFollowMidi()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.prepare(48000.0, 64);
        std::array<float, 64> l, r;
        const auto send = [&](krate::MidiEvent e) { krate::processStereo(k.engine, l.data(), r.data(), 64, &e, 1); };
        QCOMPARE(k.engine.bendPosition(), 0.0f);
        QCOMPARE(k.engine.wheelPosition(), 0.0f);
        QSignalSpy wheels(&k.controller, &KrimsonController::wheelsChanged);
        send({0, 3, {0xE0, 0x7F, 0x7F}}); // bend all the way up (channel 1: the keyboard's)
        send({0, 3, {0xB0, 1, 127}});     // mod wheel all the way up
        QCOMPARE(k.engine.bendPosition(), 1.0f);
        QCOMPARE(k.engine.wheelPosition(), 1.0f);
        QTRY_COMPARE(k.controller.bendPosition(), 1.0);
        QCOMPARE(k.controller.wheelPosition(), 1.0);
        QVERIFY(wheels.count() >= 1);
        send({0, 3, {0xE0, 0x00, 0x40}}); // back to the middle
        QTRY_COMPARE(k.controller.bendPosition(), 0.0);
        // The on-screen wheels report the same way.
        k.controller.modWheel(0.25);
        krate::processStereo(k.engine, l.data(), r.data(), 64, nullptr, 0);
        QVERIFY(std::abs(k.engine.wheelPosition() - 0.25f) < 0.01f);
    }

    // The octave buttons are learnable (standalone and in the rack: both learn
    // the entry's actions) and step the keys' octave, stopping at +-3.
    void octaveButtonsLearnAndStep()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto entry = registryEntry();
        QVERIFY(entry.actions.contains("octave_down"));
        QVERIFY(entry.actions.contains("octave_up"));
        const auto instance = entry.create(context);
        auto &k = static_cast<Instance &>(*instance);
        krate::InstrumentMidiLearn learn(k.engine, dir.filePath("midimap.json"), nullptr, entry.actions);
        learn.setActive(true);
        learn.arm(QStringLiteral("octave_up"));
        QCOMPARE(learn.armedParam(), QStringLiteral("octave_up"));
        learn.arm(QStringLiteral("octave_down"));
        QCOMPARE(learn.armedParam(), QStringLiteral("octave_down"));
        learn.setActive(false);

        const auto shift = [&] { return units::octaveShift(float(k.controller.value(QStringLiteral("octave")))); };
        QCOMPARE(shift(), 0);
        instance->triggerAction(QStringLiteral("octave_up"));
        QCOMPARE(shift(), 1);
        for (int i = 0; i < 5; ++i)
            instance->triggerAction(QStringLiteral("octave_up"));
        QCOMPARE(shift(), 3);
        for (int i = 0; i < 9; ++i)
            instance->triggerAction(QStringLiteral("octave_down"));
        QCOMPARE(shift(), -3);
    }

    // A song in the library keeps Krimson's whole sound: all four slots, not
    // only the edited one, plus the instrument's own parameters.
    void wholeSoundSavesAndLoads()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        k.engine.setSlotParam(1, ParamId::Cutoff, 0.11f); // slot B, not the edited one
        k.engine.setSlotParam(3, ParamId::Res, 0.77f);    // slot D
        k.engine.setMorph(2, ParamId::Cutoff, 0.9f);      // a morph target on C
        k.controller.setValue(QStringLiteral("volume"), 0.33);
        const QJsonObject sound = instance->saveSound();
        QVERIFY(sound.contains("slots"));
        QVERIFY(sound.value("params").toObject().contains("volume"));

        k.engine.setSlotParam(1, ParamId::Cutoff, 0.5f);
        k.engine.setSlotParam(3, ParamId::Res, 0.1f);
        k.engine.clearMorph(2);
        k.controller.setValue(QStringLiteral("volume"), 0.8);
        instance->loadSound(sound);
        QCOMPARE(k.engine.slotParam(1, ParamId::Cutoff), 0.11f);
        QCOMPARE(k.engine.slotParam(3, ParamId::Res), 0.77f);
        QCOMPARE(k.engine.morph(2, ParamId::Cutoff), 0.9f);
        QCOMPARE(k.engine.param(ParamId::Volume), 0.33f);
        // slots.json has it too (the next start begins from this sound).
        QVERIFY(QFile::exists(dir.filePath("config/slots.json")));
    }

    // Standalone, the edited slot's ARP switch starts and stops the transport.
    void arpDrivesTheStandaloneTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        instance->triggerAction(QStringLiteral("arp"));
        QCOMPARE(k.engine.slotParam(0, ParamId::ArpOn), 1.0f);
        QVERIFY(transport.playing());
        instance->triggerAction(QStringLiteral("slotB")); // another slot: no change
        QVERIFY(transport.playing());
        instance->triggerAction(QStringLiteral("slotA"));
        instance->triggerAction(QStringLiteral("arp"));
        QVERIFY(!transport.playing());
        // Hold toggles the edited slot only.
        instance->triggerAction(QStringLiteral("hold"));
        QCOMPARE(k.engine.slotParam(0, ParamId::ArpHold), 1.0f);
        QCOMPARE(k.engine.slotParam(1, ParamId::ArpHold), 0.0f);
    }
};

QTEST_GUILESS_MAIN(TestKrimson)
#include "tst_krimson.moc"
