#include <QtTest>

#include <set>
#include <vector>

#include "krimson/dsp/Engine.h"

using namespace krimson;

// The per-slot arpeggiator (docs/PLAY.md#arpeggiator).
class TestArp : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;
    // 120 BPM, 1/16 (the default rate): a step every 6000 samples.
    static constexpr std::uint32_t kStep = 6000;

    struct Rig
    {
        Engine engine;
        krate::TransportState transport;
        std::vector<float> left = std::vector<float>(kBlock), right = std::vector<float>(kBlock);

        Rig()
        {
            engine.prepare(kRate, kBlock);
            transport.bpm = 120.0;
        }
        // One block; the transport advances while it plays.
        void block(std::vector<krate::MidiEvent> midi = {})
        {
            float *outputs[2] = {left.data(), right.data()};
            krate::AudioBlock b;
            b.outputs = outputs;
            b.frames = kBlock;
            b.midi = midi.data();
            b.midiCount = std::uint32_t(midi.size());
            b.transport = &transport;
            engine.process(b);
            transport.restarted = false;
            if (transport.playing)
                transport.beatAtBlockStart += kBlock * transport.bpm / 60.0 / kRate;
        }
        void play()
        {
            transport.playing = true;
            transport.restarted = true;
            transport.beatAtBlockStart = 0.0;
        }
        // The notes slot `slot` starts while `frames` samples play.
        std::vector<int> run(int slot, std::uint32_t frames)
        {
            std::vector<int> notes;
            int last = -1;
            for (std::uint32_t done = 0; done < frames; done += kBlock) {
                block();
                const int note = engine.arpNote(slot);
                if (note >= 0 && note != last)
                    notes.push_back(note);
                last = note;
            }
            return notes;
        }
        // The notes slot `slot` has gated now.
        std::set<int> gated(int slot) const
        {
            std::set<int> notes;
            for (int v = 0; v < Engine::kVoices; ++v)
                if (engine.voiceSlot(v) == slot && engine.voiceGate(v))
                    notes.insert(engine.voiceNote(v));
            return notes;
        }
    };

    static krate::MidiEvent on(int channel, int note) { return {0, 3, {std::uint8_t(0x90 | channel), std::uint8_t(note), 100}}; }
    static krate::MidiEvent off(int channel, int note) { return {0, 3, {std::uint8_t(0x80 | channel), std::uint8_t(note), 0}}; }

private slots:
    // Slot A arpeggiates on its channel while slot B plays a chord on its own.
    void slotsArpeggiateIndependently()
    {
        Rig rig;
        const int chA = rig.engine.slotChannel(0), chB = rig.engine.slotChannel(1);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 1.0f);
        rig.play();
        rig.block({on(chA, 60), on(chA, 64), on(chA, 67), on(chB, 48), on(chB, 55)});
        const std::vector<int> notes = rig.run(0, 6 * kStep + kBlock);
        QVERIFY(notes.size() >= 6);
        QCOMPARE(std::vector<int>(notes.begin(), notes.begin() + 6), (std::vector<int>{60, 64, 67, 60, 64, 67}));
        QCOMPARE(rig.gated(1), (std::set<int>{48, 55})); // B holds its chord
        QVERIFY(rig.gated(0).size() <= 1);               // A plays one note at a time
    }

    void rangeAndModes()
    {
        Rig rig;
        const int ch = rig.engine.slotChannel(0);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 1.0f);
        rig.engine.setSlotParam(0, ParamId::ArpRange, position(1, positions::kArpRange)); // 2 octaves
        rig.engine.setSlotParam(0, ParamId::ArpMode, position(2, positions::kArpMode));   // Up & Down
        rig.play();
        rig.block({on(ch, 60), on(ch, 67)});
        const std::vector<int> notes = rig.run(0, 7 * kStep + kBlock);
        QVERIFY(notes.size() >= 7);
        QCOMPARE(std::vector<int>(notes.begin(), notes.begin() + 7), (std::vector<int>{60, 67, 72, 79, 72, 67, 60}));
    }

    void holdLatchesTheKeys()
    {
        Rig rig;
        const int ch = rig.engine.slotChannel(0);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 1.0f);
        rig.engine.setSlotParam(0, ParamId::ArpHold, 1.0f);
        rig.play();
        rig.block({on(ch, 62), on(ch, 65)});
        rig.block({off(ch, 62), off(ch, 65)});
        std::vector<int> notes = rig.run(0, 4 * kStep);
        QCOMPARE(std::set<int>(notes.begin(), notes.end()), (std::set<int>{62, 65}));
        // Hold off: the arpeggio follows the (released) keys and goes quiet.
        rig.engine.setSlotParam(0, ParamId::ArpHold, 0.0f);
        rig.run(0, kStep);
        QVERIFY(rig.run(0, 2 * kStep).empty());
    }

    void stoppingTheTransportReleases()
    {
        Rig rig;
        const int ch = rig.engine.slotChannel(0);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 1.0f);
        rig.block({on(ch, 60)});
        QVERIFY(rig.run(0, 2 * kStep).empty()); // stopped: the arpeggio waits for play
        QVERIFY(rig.gated(0).empty());
        rig.play();
        QVERIFY(!rig.run(0, 2 * kStep).empty());
        rig.transport.playing = false;
        rig.block();
        QCOMPARE(rig.engine.arpNote(0), -1);
        QVERIFY(rig.gated(0).empty());
    }

    // Arpeggio off with keys still down: the keys sound again.
    void arpOffPlaysTheHeldKeys()
    {
        Rig rig;
        const int ch = rig.engine.slotChannel(0);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 1.0f);
        rig.play();
        rig.block({on(ch, 60), on(ch, 64)});
        rig.run(0, kStep);
        rig.engine.setSlotParam(0, ParamId::ArpOn, 0.0f);
        rig.block();
        QCOMPARE(rig.gated(0), (std::set<int>{60, 64}));
    }
};

QTEST_GUILESS_MAIN(TestArp)
#include "tst_arp.moc"
