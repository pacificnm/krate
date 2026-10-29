#include <QtTest>

#include <QTemporaryDir>

#include <vector>

#include "core/Transport.h"
#include "kalliope/app/KalliopeInstrument.h"
#include "kalliope/dsp/Engine.h"

using namespace kalliope;

// The arpeggiator (docs/PLAY.md#arpeggiator): modes, range, gate, hold, timing on the transport.
class TestArp : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 16;
    static constexpr int kStep = 6000; // a 1/16 step at 120 BPM

    struct Rig
    {
        Engine engine;
        krate::Transport transport;
        std::vector<krate::MidiEvent> pending;
        std::vector<int> notes;  // each new arpeggio note, in order
        std::vector<int> starts; // the frame (to the block) each one started
        int frame = 0;

        Rig()
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::Chorus, 0.0f);
            engine.setParam(ParamId::PlayMode, 0.5f); // Arpeggio
            transport.setBpm(120.0);
            transport.setPlaying(true);
        }
        void key(int note, bool down) { pending.push_back({0, 3, {std::uint8_t(down ? 0x90 : 0x80), std::uint8_t(note), 100}}); }
        void run(int frames)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (int f = 0; f < frames; f += int(kBlock)) {
                const krate::TransportState state = transport.advance(kBlock, kRate);
                const int before = engine.arpNote();
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()),
                                     &state);
                pending.clear();
                const int now = engine.arpNote();
                if (now >= 0 && (now != before || started(frame)))
                    if (notes.empty() || starts.back() < frame - kStep / 2) {
                        notes.push_back(now);
                        starts.push_back(frame);
                    }
                frame += int(kBlock);
            }
        }
        // A step boundary falls inside this block.
        static bool started(int frame) { return frame % kStep < int(kBlock); }
    };

    static std::vector<int> play(int mode, int octaves, std::vector<int> chord, int steps)
    {
        Rig rig;
        rig.engine.setParam(ParamId::ArpMode, position(mode, positions::kArpMode));
        rig.engine.setParam(ParamId::ArpRange, position(octaves - 1, positions::kArpRange));
        for (int n : chord)
            rig.key(n, true);
        rig.run(steps * kStep);
        return rig.notes;
    }

private slots:
    void modes()
    {
        QCOMPARE(play(0, 1, {64, 60, 67}, 6), (std::vector<int>{60, 64, 67, 60, 64, 67}));
        QCOMPARE(play(1, 1, {60, 64, 67}, 6), (std::vector<int>{67, 64, 60, 67, 64, 60}));
        QCOMPARE(play(2, 1, {60, 64, 67}, 7), (std::vector<int>{60, 64, 67, 64, 60, 64, 67}));
        QCOMPARE(play(0, 2, {60, 64, 67}, 7), (std::vector<int>{60, 64, 67, 72, 76, 79, 60}));
        const auto random = play(3, 1, {60, 64, 67}, 12);
        QCOMPARE(int(random.size()), 12);
        for (int n : random)
            QVERIFY(n == 60 || n == 64 || n == 67);
    }

    // Steps start on the beat grid (to the block here; StepClock is exact to the sample).
    void lockedToTheTransport()
    {
        Rig rig;
        rig.key(60, true);
        rig.key(67, true);
        rig.run(4 * kStep);
        QCOMPARE(int(rig.starts.size()), 4);
        for (std::size_t i = 0; i < rig.starts.size(); ++i)
            QVERIFY(std::abs(rig.starts[i] - int(i) * kStep) < int(kBlock));
    }

    // Each note lasts half a step; then the voice is released until the next step.
    void gateIsHalfAStep()
    {
        Rig rig;
        rig.key(60, true);
        rig.run(kStep / 4);
        QCOMPARE(rig.engine.arpNote(), 60);
        rig.run(kStep / 2);
        QCOMPARE(rig.engine.arpNote(), -1);
        bool gated = false;
        for (int v = 0; v < Engine::kVoices; ++v)
            gated = gated || rig.engine.voiceGate(v);
        QVERIFY(!gated);
    }

    void holdKeepsTheChord()
    {
        Rig rig;
        rig.engine.setParam(ParamId::ArpHold, 1.0f);
        rig.key(60, true);
        rig.key(64, true);
        rig.run(kBlock);
        rig.key(60, false);
        rig.key(64, false);
        rig.run(4 * kStep - 2 * int(kBlock)); // up to just before the fifth step
        QCOMPARE(rig.notes, (std::vector<int>{60, 64, 60, 64}));
        // A new chord (no keys down) replaces it.
        rig.key(72, true);
        rig.run(2 * kStep);
        QCOMPARE(rig.notes.back(), 72);
        QCOMPARE(rig.notes[rig.notes.size() - 2], 72);
    }

    void noKeysNoNotes()
    {
        Rig rig;
        rig.run(4 * kStep);
        QVERIFY(rig.notes.empty());
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void stoppingTheTransportStopsTheArpeggio()
    {
        Rig rig;
        rig.key(60, true);
        rig.run(kStep / 4);
        QCOMPARE(rig.engine.arpNote(), 60);
        rig.transport.setPlaying(false);
        rig.run(kStep);
        QCOMPARE(rig.engine.arpNote(), -1);
        rig.run(2 * kStep);
        QCOMPARE(int(rig.notes.size()), 1);
    }

    // Arpeggio off: the keys still down play as a chord again.
    void turningItOffPlaysTheKeys()
    {
        Rig rig;
        rig.key(60, true);
        rig.key(64, true);
        rig.run(kStep);
        rig.engine.setParam(ParamId::PlayMode, 0.0f); // Note
        rig.run(kStep);
        QCOMPARE(rig.engine.arpNote(), -1);
        int gated = 0;
        for (int v = 0; v < Engine::kVoices; ++v)
            gated += rig.engine.voiceGate(v) ? 1 : 0;
        QCOMPARE(gated, 2);
    }

    // Standalone: Play Mode Arpeggio starts the transport, leaving it stops what it started.
    void standaloneArpRunsTheTransport()
    {
        QTemporaryDir dir;
        krate::Transport transport;
        krate::InstrumentContext context{&transport, dir.filePath("config"), dir.filePath("data")};
        const auto instance = registryEntry().create(context);
        auto &k = static_cast<Instance &>(*instance);
        QVERIFY(!transport.playing());
        k.controller.setValue(QStringLiteral("play_mode"), 0.5); // Arpeggio
        QVERIFY(transport.playing());
        k.controller.setValue(QStringLiteral("play_mode"), 0.0); // Note
        QVERIFY(!transport.playing());
        // Started by hand: leaving Arpeggio leaves it running.
        k.controller.setPlaying(true);
        k.controller.setValue(QStringLiteral("play_mode"), 0.5);
        k.controller.setValue(QStringLiteral("play_mode"), 0.0);
        QVERIFY(transport.playing());
    }
};

QTEST_GUILESS_MAIN(TestArp)
#include "tst_arp.moc"
