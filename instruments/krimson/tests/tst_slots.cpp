#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

#include <QFile>
#include <QTemporaryDir>

#include "krimson/app/KrimsonInstrument.h"
#include "krimson/dsp/Engine.h"

using namespace krimson;

// The four slots (docs/PLAY.md#slots): channels, layering, split, on/off,
// per-slot sounds and key state, slots.json.
class TestSlots : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Rig
    {
        Engine engine;
        std::vector<float> left, right;
        std::vector<krate::MidiEvent> pending;

        Rig()
        {
            engine.prepare(kRate, kBlock);
            for (int s = 0; s < SlotCount; ++s)
                engine.setSlotParam(s, ParamId::AenvR, 0.3f); // short releases
        }
        void on(int channel, int note) { pending.push_back({0, 3, {std::uint8_t(0x90 | channel), std::uint8_t(note), 100}}); }
        void off(int channel, int note) { pending.push_back({0, 3, {std::uint8_t(0x80 | channel), std::uint8_t(note), 0}}); }
        void cc(int channel, int number, int value)
        {
            pending.push_back({0, 3, {std::uint8_t(0xB0 | channel), std::uint8_t(number), std::uint8_t(value)}});
        }
        void run(double seconds = 0.02)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()));
                pending.clear();
                left.insert(left.end(), l.begin(), l.end());
                right.insert(right.end(), r.begin(), r.end());
            }
        }
        // (slot, note) of every gated voice, sorted.
        std::vector<std::pair<int, int>> playing() const
        {
            std::vector<std::pair<int, int>> v;
            for (int i = 0; i < Engine::kVoices; ++i)
                if (engine.voiceActive(i) && engine.voiceGate(i))
                    v.push_back({engine.voiceSlot(i), engine.voiceNote(i)});
            std::sort(v.begin(), v.end());
            return v;
        }
    };
    using Playing = std::vector<std::pair<int, int>>;

private slots:
    // Slots A-D own channels 3-6 by default; other channels are the keyboard (slot A lit).
    void channelsAndTheKeyboard()
    {
        Rig rig;
        rig.on(2, 60); // channel 3: A
        rig.on(3, 62); // channel 4: B
        rig.on(5, 64); // channel 6: D
        rig.on(0, 67); // channel 1: the keyboard, A lit
        rig.run();
        QCOMPARE(rig.playing(), (Playing{{0, 60}, {0, 67}, {1, 62}, {3, 64}}));
    }

    void litSlotsLayer()
    {
        Rig rig;
        rig.engine.setLitSlots(0b0110); // B and C
        rig.on(0, 60);
        rig.run();
        QCOMPARE(rig.playing(), (Playing{{1, 60}, {2, 60}}));
        rig.off(0, 60);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void splitKeyboard()
    {
        Rig rig;
        rig.engine.setLitSlots(0b0011);
        rig.engine.setSlotZone(0, Engine::Zone::Lower);
        rig.engine.setSlotZone(1, Engine::Zone::Upper);
        rig.engine.setSplitNote(60);
        rig.on(0, 48);
        rig.on(0, 72);
        rig.run();
        QCOMPARE(rig.playing(), (Playing{{0, 48}, {1, 72}}));
    }

    void slotOffTakesNothing()
    {
        Rig rig;
        rig.engine.setSlotOn(1, false);
        rig.on(3, 60); // B's channel, B off: the keyboard (A)
        rig.run();
        QCOMPARE(rig.playing(), (Playing{{0, 60}}));
    }

    // Each slot has its own voice mode and sound.
    void perSlotModesAndSounds()
    {
        Rig rig;
        rig.engine.setSlotParam(0, ParamId::VoiceMode, position(1, positions::kVoiceMode)); // A mono
        rig.engine.setSlotParam(1, ParamId::Cutoff, 0.1f);                                 // B dark
        rig.run(kBlock / kRate);
        for (int n : {60, 64, 67}) {
            rig.on(2, n); // A: mono, last note
            rig.on(3, n); // B: poly chord
        }
        rig.run();
        QCOMPARE(rig.playing(), (Playing{{0, 67}, {1, 60}, {1, 64}, {1, 67}}));
        QVERIFY(rig.engine.settings(1).cutoffOctaves < rig.engine.settings(0).cutoffOctaves);
    }

    // The sustain pedal on a slot's channel holds only that slot.
    void sustainPerSlot()
    {
        Rig rig;
        rig.on(2, 60);
        rig.on(3, 62);
        rig.cc(2, 64, 127); // pedal on A's channel
        rig.run();
        rig.off(2, 60);
        rig.off(3, 62);
        rig.run(0.3);
        QCOMPARE(rig.playing(), (Playing{{0, 60}}));
    }

    void slotPan()
    {
        Rig rig;
        rig.engine.setSlotParam(0, ParamId::SlotPan, 0.0f); // hard left
        rig.on(2, 60);
        rig.run(0.2);
        double l = 0.0, r = 0.0;
        for (std::size_t i = 0; i < rig.left.size(); ++i) {
            l += std::fabs(rig.left[i]);
            r += std::fabs(rig.right[i]);
        }
        QVERIFY(l > 0.0);
        QVERIFY(r < 0.01 * l);
    }

    // A note-off reaches the slot that took the note, even after the lit slots change.
    void noStuckNotesWhenLayersChange()
    {
        Rig rig;
        rig.on(0, 60); // A
        rig.run();
        rig.engine.setLitSlots(0b0010); // now B
        rig.off(0, 60);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    // The slot buttons and slots.json.
    void buttonsAndStorage()
    {
        QTemporaryDir dir;
        krate::InstrumentContext context{nullptr, dir.filePath("config"), dir.filePath("data")};
        {
            const auto instance = registryEntry().create(context);
            auto &k = static_cast<Instance &>(*instance);
            QVERIFY(k.slotSetup.isLit(0));
            k.slotSetup.pressSlot(2, true); // layer C: lit and edited
            QVERIFY(k.slotSetup.isLit(0) && k.slotSetup.isLit(2));
            QCOMPARE(k.controller.editedSlot(), 2);
            k.controller.setValue(QStringLiteral("cutoff"), 0.25); // C's cutoff
            k.slotSetup.setChannel(2, 9);
            k.slotSetup.setZone(2, 2);
            k.slotSetup.setSplitNote(64);
            k.slotSetup.setOn(3, false);
            k.slotSetup.pressSlot(0, true); // unlight A: C stays edited
            QVERIFY(!k.slotSetup.isLit(0));
            QCOMPARE(k.controller.editedSlot(), 2);
            k.slotSetup.pressSlot(2, true); // the last lit slot can't be unlit
            QVERIFY(k.slotSetup.isLit(2));
            instance->triggerAction(QStringLiteral("slotB")); // plain press: only B
            QVERIFY(k.slotSetup.isLit(1) && !k.slotSetup.isLit(2));
            QCOMPARE(k.controller.editedSlot(), 1);
        }
        QVERIFY(QFile::exists(dir.filePath("config/slots.json")));
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QCOMPARE(k.controller.editedSlot(), 1);
        QVERIFY(k.slotSetup.isLit(1));
        QCOMPARE(double(k.engine.slotParam(2, ParamId::Cutoff)), double(0.25f));
        QCOMPARE(k.slotSetup.channel(2), 9);
        QCOMPARE(k.slotSetup.zone(2), 2);
        QCOMPARE(k.slotSetup.splitNote(), 64);
        QVERIFY(!k.slotSetup.isOn(3));
    }
};

QTEST_GUILESS_MAIN(TestSlots)
#include "tst_slots.moc"
