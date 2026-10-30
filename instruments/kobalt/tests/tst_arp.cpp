#include <QtTest>

#include <vector>

#include "core/Transport.h"
#include "kobalt/dsp/Engine.h"

using namespace kobalt;
namespace p = kobalt::positions;

// The arpeggiator (docs/BRIEF.md#scope-v1): modes, range, gate, hold, timing on the transport.
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
        std::vector<int> notes;  // each note the voice started, in order
        std::vector<int> starts; // the block (frame) it started in
        std::vector<int> ends;   // the block its gate closed in
        int frame = 0;
        bool gate = false;

        Rig()
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::PlayMode, position(1, p::kPlayMode)); // Arpeggio
            transport.setBpm(120.0);
            transport.setPlaying(true);
        }
        void key(int note, bool down)
        {
            pending.push_back({0, 3, {std::uint8_t(down ? 0x90 : 0x80), std::uint8_t(note), 100}});
        }
        void run(int frames)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (int f = 0; f < frames; f += int(kBlock)) {
                const krate::TransportState state = transport.advance(kBlock, kRate);
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(),
                                     std::uint32_t(pending.size()), &state);
                pending.clear();
                const bool now = engine.voice().gate();
                // A new note: the gate opened (each arpeggio note's closes at half a step).
                if (now && !gate) {
                    notes.push_back(engine.voice().note());
                    starts.push_back(frame);
                } else if (!now && gate) {
                    ends.push_back(frame);
                }
                gate = now;
                frame += int(kBlock);
            }
        }
    };

private slots:
    void modes_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<int>("range");
        QTest::addColumn<std::vector<int>>("expected");
        QTest::newRow("up") << 0 << 0 << std::vector<int>{60, 64, 67, 60, 64, 67, 60};
        QTest::newRow("up & down") << 1 << 0 << std::vector<int>{60, 64, 67, 64, 60, 64, 67};
        QTest::newRow("down") << 2 << 0 << std::vector<int>{67, 64, 60, 67, 64, 60, 67};
        QTest::newRow("up, 2 octaves") << 0 << 1 << std::vector<int>{60, 64, 67, 72, 76, 79, 60};
        QTest::newRow("down, 3 octaves") << 2 << 2 << std::vector<int>{91, 88, 84, 79, 76, 72, 67};
    }
    void modes()
    {
        QFETCH(int, mode);
        QFETCH(int, range);
        QFETCH(std::vector<int>, expected);
        Rig rig;
        rig.engine.setParam(ParamId::ArpMode, position(mode, p::kArpMode));
        rig.engine.setParam(ParamId::ArpRange, position(range, p::kArpRange));
        rig.key(67, true); // out of order: the arpeggio sorts them
        rig.key(60, true);
        rig.key(64, true);
        rig.run(kStep * int(expected.size()) - kStep / 2);
        QCOMPARE(rig.notes, expected);
    }

    // Steps start on the 1/16 grid and each note's gate closes at half a step.
    void timingAndGate()
    {
        Rig rig;
        rig.key(60, true);
        rig.key(62, true);
        rig.run(kStep * 4);
        QCOMPARE(int(rig.starts.size()), 4);
        for (std::size_t i = 0; i < rig.starts.size(); ++i) {
            QVERIFY(std::abs(rig.starts[i] - int(i) * kStep) < int(kBlock));
            QVERIFY(std::abs(rig.ends[i] - (int(i) * kStep + kStep / 2)) < int(kBlock));
        }
    }

    // Hold keeps the arpeggio going with the keys up; the next chord replaces it.
    void hold()
    {
        Rig rig;
        rig.engine.setParam(ParamId::ArpHold, 1.0f);
        rig.key(60, true);
        rig.key(64, true);
        rig.run(kStep);
        rig.key(60, false);
        rig.key(64, false);
        rig.run(kStep * 3);
        QCOMPARE(rig.notes, (std::vector<int>{60, 64, 60, 64}));
        rig.key(70, true); // a new chord: just 70 now
        rig.run(kStep * 2);
        QCOMPARE(rig.notes.back(), 70);
        QCOMPARE(rig.notes[rig.notes.size() - 2], 70);
    }

    // Stopped, the arpeggio waits; the keys alone don't sound in Arpeggio.
    void waitsForTheTransport()
    {
        Rig rig;
        rig.transport.setPlaying(false);
        rig.key(60, true);
        rig.run(kStep * 2);
        QVERIFY(rig.notes.empty());
        rig.transport.setPlaying(true);
        rig.run(kStep);
        QCOMPARE(rig.notes, std::vector<int>{60});
    }

    // Back to Note: the key still down sounds again.
    void backToNote()
    {
        Rig rig;
        rig.key(60, true);
        rig.run(kStep);
        rig.engine.setParam(ParamId::PlayMode, 0.0f);
        rig.run(int(kBlock));
        QVERIFY(rig.engine.voice().gate());
        QCOMPARE(rig.engine.voice().note(), 60);
        rig.run(kStep * 2); // the arpeggio's 60, the key's 60, and no more arpeggio steps
        QCOMPARE(rig.notes, (std::vector<int>{60, 60}));
    }
};

QTEST_GUILESS_MAIN(TestArp)
#include "tst_arp.moc"
