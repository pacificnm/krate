#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

#include "kalliope/dsp/Engine.h"

using namespace kalliope;

// Polyphony: allocation, stealing, key modes, sustain (docs/PLAY.md#key-modes, docs/DSP.md#voices-and-allocation).
class TestVoices : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Rig
    {
        Engine engine;
        std::vector<float> out;
        std::vector<krate::MidiEvent> pending;

        explicit Rig(int keyMode = 0)
        {
            engine.prepare(kRate, kBlock);
            engine.setParam(ParamId::Chorus, 0.0f); // the voices alone (effects: tst_fx)
            engine.setParam(ParamId::KeyMode, position(keyMode, positions::kKeyMode));
            engine.setParam(ParamId::Env2R, 0.3f); // ~16 ms release
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
    };

private slots:
    void chordUsesOneVoicePerNote()
    {
        Rig rig;
        for (int n : {60, 64, 67, 71})
            rig.on(n);
        rig.run();
        QCOMPARE(rig.notes(), (std::vector<int>{60, 64, 67, 71}));
        QCOMPARE(rig.engine.activeVoices(), 4);
        rig.off(64);
        rig.run(0.3);
        QCOMPARE(rig.notes(), (std::vector<int>{60, 67, 71}));
        QCOMPARE(rig.engine.activeVoices(), 3); // its release has finished
    }

    void sameNoteReusesItsVoice()
    {
        Rig rig;
        rig.on(60);
        rig.run();
        rig.off(60);
        rig.run(0.005); // still releasing
        rig.on(60);
        rig.run();
        QCOMPARE(rig.engine.activeVoices(), 1);
        QCOMPARE(rig.notes(), (std::vector<int>{60}));
    }

    // Nine notes on eight voices: the oldest held note gives way.
    void ninthNoteStealsTheOldestHeld()
    {
        Rig rig;
        for (int n = 60; n < 68; ++n) {
            rig.on(n);
            rig.run(0.01);
        }
        QCOMPARE(rig.engine.activeVoices(), 8);
        rig.on(72);
        rig.run(0.02); // the 2 ms fade, then the new note
        QCOMPARE(rig.notes(), (std::vector<int>{61, 62, 63, 64, 65, 66, 67, 72}));
    }

    // With some notes released, a released voice is taken before a held one.
    void releasedVoicesGoFirst()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Env2R, 0.9f); // long releases: still sounding
        for (int n = 60; n < 68; ++n)
            rig.on(n);
        rig.run();
        rig.off(65);
        rig.run();
        rig.on(72);
        rig.run();
        QCOMPARE(rig.notes(), (std::vector<int>{60, 61, 62, 63, 64, 66, 67, 72}));
    }

    // Stealing fades the old note out instead of cutting it.
    void stealingDoesNotClick()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Env2R, 1.0f);
        for (int n = 48; n < 56; ++n)
            rig.on(n);
        rig.run(0.3);
        const std::size_t at = rig.out.size();
        float biggest = 0.0f;
        for (std::size_t i = at - 4800; i < at; ++i)
            biggest = std::max(biggest, std::fabs(rig.out[i] - rig.out[i - 1]));
        rig.on(84); // steals a low note while all eight ring
        rig.run(0.05);
        for (std::size_t i = at; i < rig.out.size(); ++i)
            QVERIFY2(std::fabs(rig.out[i] - rig.out[i - 1]) < 2.0f * biggest + 1e-3f, qPrintable(QString::number(i)));
    }

    void sustainPedalHoldsReleasedNotes()
    {
        Rig rig;
        rig.on(60);
        rig.on(64);
        rig.run();
        rig.pedal(true);
        rig.off(60);
        rig.off(64);
        rig.run(0.3);
        QCOMPARE(rig.notes(), (std::vector<int>{60, 64})); // held by the pedal
        rig.on(67);
        rig.off(67);
        rig.run();
        QCOMPARE(rig.notes(), (std::vector<int>{60, 64, 67}));
        rig.pedal(false);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    void unisonStacksEveryVoice()
    {
        Rig rig(1);
        rig.on(60);
        rig.run();
        QCOMPARE(rig.engine.activeVoices(), 8);
        QCOMPARE(rig.notes(), std::vector<int>(8, 60));
        // Legato: a second key moves the stack without restarting the envelopes.
        rig.run(0.2);
        const double before = rig.engine.voiceLevel(0);
        rig.on(67);
        rig.run(0.005);
        QCOMPARE(rig.notes(), std::vector<int>(8, 67));
        QVERIFY(rig.engine.voiceLevel(0) <= before + 1e-9);
        rig.off(67);
        rig.run();
        QCOMPARE(rig.notes(), std::vector<int>(8, 60)); // back to the held key
        rig.off(60);
        rig.run(0.3);
        QCOMPARE(rig.engine.activeVoices(), 0);
    }

    // The unison stack is detuned: its level wobbles rather than staying a steady saw.
    void unisonIsBoundedAndDetuned()
    {
        Rig rig(1);
        rig.engine.setParam(ParamId::Cutoff, 1.0f);
        rig.engine.setParam(ParamId::VcfEnv, 0.0f);
        rig.on(48, 127);
        rig.run(1.0);
        float peak = 0.0f;
        for (float s : rig.out)
            peak = std::max(peak, std::fabs(s));
        QVERIFY(peak < 1.0f);
        QVERIFY(peak > 0.05f);
    }

    void soloUsesOneVoice()
    {
        Rig rig(2);
        rig.on(60);
        rig.on(64);
        rig.on(67);
        rig.run();
        QCOMPARE(rig.engine.activeVoices(), 1);
        QCOMPARE(rig.notes(), (std::vector<int>{67}));
        rig.off(67);
        rig.run();
        QCOMPARE(rig.notes(), (std::vector<int>{64})); // last-note priority
    }

    void changingKeyModeReleasesEverything()
    {
        Rig rig;
        for (int n : {60, 64, 67})
            rig.on(n);
        rig.run();
        rig.engine.setParam(ParamId::KeyMode, 1.0f); // Solo
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
            rig.engine.setParam(ParamId::Velocity, 1.0f);
            rig.on(60, i == 0 ? 32 : 127);
            rig.run(0.3);
            peak[i] = 0.0f;
            for (float s : rig.out)
                peak[i] = std::max(peak[i], std::fabs(s));
        }
        QVERIFY(peak[0] < 0.5f * peak[1]);
    }

    // Eight notes at full resonance and level stay finite and bounded.
    void eightVoicesStayBounded()
    {
        Rig rig;
        rig.engine.setParam(ParamId::Res, 1.0f);
        rig.engine.setParam(ParamId::VcaLevel, 1.0f);
        rig.engine.setParam(ParamId::Volume, 1.0f);
        for (int n : {36, 43, 48, 52, 55, 60, 64, 67})
            rig.on(n, 127);
        rig.run(1.0);
        float peak = 0.0f;
        for (float s : rig.out) {
            QVERIFY(std::isfinite(s));
            peak = std::max(peak, std::fabs(s));
        }
        QVERIFY2(peak < 4.0f, qPrintable(QString::number(peak)));
    }
};

QTEST_GUILESS_MAIN(TestVoices)
#include "tst_voices.moc"
