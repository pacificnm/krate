#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

#include "krimson/dsp/Engine.h"

using namespace krimson;

// Polyphony and voice modes (docs/DSP.md#voices-and-slots, docs/PLAY.md#voice-modes-per-slot).
class TestVoices : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Rig
    {
        Engine engine;
        std::vector<float> out, outR;
        std::vector<krate::MidiEvent> pending;

        explicit Rig(int mode = 0)
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::VoiceMode, position(mode, positions::kVoiceMode));
            engine.setParam(ParamId::AenvR, 0.3f); // ~20 ms release
        }
        void send(std::uint8_t status, std::uint8_t d1, std::uint8_t d2) { pending.push_back({0, 3, {status, d1, d2}}); }
        void on(int note, int velocity = 100) { send(0x90, std::uint8_t(note), std::uint8_t(velocity)); }
        void off(int note) { send(0x80, std::uint8_t(note), 0); }
        void pedal(bool down) { send(0xB0, 64, down ? 127 : 0); }
        void run(double seconds = 0.02)
        {
            std::vector<float> l(kBlock), r(kBlock);
            for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
                krate::processStereo(engine, l.data(), r.data(), kBlock, pending.data(), std::uint32_t(pending.size()));
                pending.clear();
                out.insert(out.end(), l.begin(), l.end());
                outR.insert(outR.end(), r.begin(), r.end());
            }
        }
        std::vector<int> notes() const
        {
            std::vector<int> n;
            for (int v = 0; v < Engine::kVoices; ++v)
                if (engine.voiceActive(v) && engine.voiceGate(v))
                    n.push_back(engine.voiceNote(v));
            std::sort(n.begin(), n.end());
            return n;
        }
        float peak() const
        {
            float p = 0.0f;
            for (float s : out)
                p = std::max(p, std::fabs(s));
            return p;
        }
    };

    // Frequency from upward zero crossings of the last `seconds` of the output.
    static double frequency(const std::vector<float> &x, double seconds)
    {
        const std::size_t from = x.size() - std::size_t(seconds * kRate);
        double first = -1.0, last = -1.0;
        int count = 0;
        for (std::size_t i = from; i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
                const double t = double(i - 1) + double(-x[i - 1]) / double(x[i] - x[i - 1]);
                if (first < 0.0)
                    first = t;
                else
                    ++count;
                last = t;
            }
        return count > 0 ? count * kRate / (last - first) : 0.0;
    }

private slots:
    void chordUsesOneVoicePerNote()
    {
        Rig rig;
        for (int n : {60, 64, 67, 71})
            rig.on(n);
        rig.run();
        QCOMPARE(rig.notes(), (std::vector<int>{60, 64, 67, 71}));
        rig.off(64);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 3);
    }

    void seventeenthNoteStealsTheOldestHeld()
    {
        Rig rig;
        for (int n = 40; n < 56; ++n) {
            rig.on(n);
            rig.run(0.005);
        }
        QCOMPARE(rig.engine.activeVoices(), 16);
        rig.on(80);
        rig.run(0.02);
        std::vector<int> expected;
        for (int n = 41; n < 56; ++n)
            expected.push_back(n);
        expected.push_back(80);
        QCOMPARE(rig.notes(), expected);
    }

    void releasedVoicesGoFirst()
    {
        Rig rig;
        rig.engine.setParam(ParamId::AenvR, 0.9f); // long releases: still sounding
        for (int n = 40; n < 56; ++n)
            rig.on(n);
        rig.run();
        rig.off(50);
        rig.run();
        rig.on(80);
        rig.run();
        const auto notes = rig.notes();
        QVERIFY(std::find(notes.begin(), notes.end(), 40) != notes.end()); // the oldest held survives
        QVERIFY(std::find(notes.begin(), notes.end(), 80) != notes.end());
    }

    void stealingDoesNotClick()
    {
        Rig rig;
        rig.engine.setParam(ParamId::AenvR, 1.0f);
        rig.engine.setParam(ParamId::AenvA, 0.2f); // ~4 ms: the new note's own attack isn't the click we look for
        for (int n = 36; n < 52; ++n)
            rig.on(n);
        rig.run(0.3);
        const std::size_t at = rig.out.size();
        float biggest = 0.0f;
        for (std::size_t i = at - 4800; i < at; ++i)
            biggest = std::max(biggest, std::fabs(rig.out[i] - rig.out[i - 1]));
        rig.on(84);
        rig.run(0.05);
        // The steal itself: the 2 ms fade and the new note's first milliseconds
        // (later, the bright high note's own waveform has bigger steps by nature).
        float worst = 0.0f;
        for (std::size_t i = at; i < at + std::size_t(0.004 * kRate); ++i)
            worst = std::max(worst, std::fabs(rig.out[i] - rig.out[i - 1]));
        QVERIFY2(worst < 2.0f * biggest + 1e-3f, qPrintable(QStringLiteral("%1 vs %2").arg(worst).arg(biggest)));
    }

    void sustainPedal()
    {
        Rig rig;
        rig.on(60);
        rig.on(64);
        rig.run();
        rig.pedal(true);
        rig.off(60);
        rig.off(64);
        rig.run(0.3);
        QCOMPARE(rig.notes(), (std::vector<int>{60, 64}));
        rig.pedal(false);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void monoLastNoteRetriggers()
    {
        Rig rig(1);
        rig.engine.setParam(ParamId::AenvD, 0.7f); // ~0.5 s
        rig.engine.setParam(ParamId::AenvS, 0.2f); // decays: a retrigger shows as a jump back up
        rig.on(60);
        rig.run(1.5);
        const double settled = rig.engine.voiceLevel(0);
        rig.on(64);
        rig.run(0.004);
        QCOMPARE(rig.engine.activeVoices(), 1);
        QCOMPARE(rig.notes(), (std::vector<int>{64}));
        QVERIFY(rig.engine.voiceLevel(0) > settled + 0.3); // retriggered
        rig.off(64);
        rig.run(0.01);
        QCOMPARE(rig.notes(), (std::vector<int>{60})); // back to the held key
    }

    void legatoDoesNotRetrigger()
    {
        Rig rig(2);
        rig.engine.setParam(ParamId::AenvD, 0.7f);
        rig.engine.setParam(ParamId::AenvS, 0.2f);
        rig.on(60);
        rig.run(1.5);
        const double settled = rig.engine.voiceLevel(0);
        rig.on(64);
        rig.run(0.004);
        QCOMPARE(rig.notes(), (std::vector<int>{64}));
        QVERIFY(rig.engine.voiceLevel(0) < settled + 0.05); // no retrigger
    }

    void unisonStacksThreeVoicesInStereo()
    {
        Rig rig(3);
        rig.engine.setParam(ParamId::UnisonDetune, 1.0f);
        rig.on(48);
        rig.run(0.5);
        QCOMPARE(rig.engine.activeVoices(), 3); // the default: three voices
        QCOMPARE(rig.notes(), std::vector<int>(3, 48));
        // Detuned and spread: the two sides differ.
        double difference = 0.0;
        for (std::size_t i = rig.out.size() / 2; i < rig.out.size(); ++i)
            difference += std::fabs(rig.out[i] - rig.outR[i]);
        QVERIFY(difference > 1.0);
        QVERIFY(rig.peak() < 1.0f);
    }

    void glideSlidesThePitch()
    {
        Rig rig(1);
        rig.engine.setParam(ParamId::Cutoff, 1.0f);
        rig.engine.setParam(ParamId::FenvAmt, 0.0f);
        rig.engine.setParam(ParamId::OscMix, 0.0f);
        rig.engine.setParam(ParamId::Glide, 0.5f); // ~0.5 s to get there
        rig.on(57); // 220 Hz
        rig.run(0.6);
        rig.on(69); // 440 Hz: glides up
        rig.run(0.05);
        const double early = frequency(rig.out, 0.03);
        rig.run(1.5);
        const double late = frequency(rig.out, 0.2);
        QVERIFY2(early > 230.0 && early < 400.0, qPrintable(QString::number(early)));
        QVERIFY(std::abs(late - 440.0) < 2.0);
    }

    void changingModeReleasesEverything()
    {
        Rig rig;
        for (int n : {60, 64, 67})
            rig.on(n);
        rig.run();
        rig.engine.setParam(ParamId::VoiceMode, 1.0f);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void allNotesOff()
    {
        Rig rig;
        for (int n : {60, 64, 67})
            rig.on(n);
        rig.run();
        rig.send(0xB0, 123, 0);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void velocityScalesLevel()
    {
        float peak[2];
        for (int i = 0; i < 2; ++i) {
            Rig rig;
            rig.engine.setParam(ParamId::AmpVel, 1.0f);
            rig.on(60, i == 0 ? 32 : 127);
            rig.run(0.3);
            peak[i] = rig.peak();
        }
        QVERIFY(peak[0] < 0.5f * peak[1]);
    }

    void sixteenVoicesStayBounded()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Res, 1.0f);
        rig.engine.setParam(ParamId::Drive, 1.0f);
        rig.engine.setParam(ParamId::Gain, 1.0f);
        rig.engine.setParam(ParamId::Volume, 1.0f);
        for (int n = 36; n < 68; n += 2)
            rig.on(n, 127);
        rig.run(1.0);
        for (float s : rig.out)
            QVERIFY(std::isfinite(s));
        QVERIFY2(rig.peak() < 8.0f, qPrintable(QString::number(rig.peak())));
    }
};

QTEST_GUILESS_MAIN(TestVoices)
#include "tst_voices.moc"
