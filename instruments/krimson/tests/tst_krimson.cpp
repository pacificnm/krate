#include <QtTest>

#include <array>
#include <cmath>
#include <set>
#include <string_view>

#include <QFile>
#include <QTemporaryDir>

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
