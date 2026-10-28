#include <QtTest>

#include <algorithm>
#include <vector>

#include "k909/dsp/Engine.h"
#include "rack/Rack.h"

using namespace k909;

// K909 in the Krate rack: channel 10 reaches it whatever has focus, other
// channels go to the focused device (docs/SHARED_FOUNDATIONS.MD#midi-in-the-rack).
class TestK909InRack : public QObject
{
    Q_OBJECT

    static constexpr std::uint32_t kFrames = 512;

    struct Peaks
    {
        float drums = 0.0f;
        float other = 0.0f;
    };

    static Peaks play(krate::Rack &rack, std::uint8_t status, std::uint8_t note)
    {
        std::vector<float> masterL(kFrames), masterR(kFrames);
        std::vector<std::vector<float>> outs(4, std::vector<float>(kFrames));
        float *const deviceOutputs[4] = {outs[0].data(), outs[1].data(), outs[2].data(), outs[3].data()};
        const krate::MidiEvent midi{0, 3, {status, note, 120}};
        krate::RackBlock block;
        block.frames = kFrames;
        block.midi = &midi;
        block.midiCount = 1;
        block.masterL = masterL.data();
        block.masterR = masterR.data();
        block.deviceOutputs = deviceOutputs;
        rack.process(block);
        const auto peak = [](const std::vector<float> &v) {
            float p = 0.0f;
            for (float s : v)
                p = std::max(p, std::abs(s));
            return p;
        };
        return {peak(outs[0]), peak(outs[2])};
    }

private slots:
    void channelTenPlaysTheDrumsWhateverHasFocus()
    {
        Engine drums;
        Engine other; // a stand-in for the focused instrument
        krate::Rack rack(kFrames);
        const int d = rack.addDevice(drums, 9);
        const int o = rack.addDevice(other);
        rack.prepare(48000.0);
        rack.setFocus(o);
        QCOMPARE(d, 0);

        // Channel 10 (status 0x99): the drums, not the focused device.
        Peaks p = play(rack, 0x99, 36);
        QVERIFY(p.drums > 0.0f);
        QCOMPARE(p.other, 0.0f);
        QVERIFY(drums.sounding(DrumId::Bd));
        QVERIFY(!other.sounding(DrumId::Bd));

        // Channel 1 (0x90): the focused device only.
        p = play(rack, 0x90, 38);
        QVERIFY(other.sounding(DrumId::Sd));
        QVERIFY(!drums.sounding(DrumId::Sd));
    }
};

QTEST_GUILESS_MAIN(TestK909InRack)
#include "tst_rack.moc"
