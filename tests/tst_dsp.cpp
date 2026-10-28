#include <QtTest>

#include <cmath>
#include <vector>

#include "dsp/DiodeLadder.h"
#include "dsp/Engine.h"
#include "dsp/HalfbandDecimator.h"
#include "dsp/Oscillator.h"
#include "dsp/Params.h"
#include "dsp/Voice.h"
#include "TestHost.h"

using namespace k303;

class TestDsp : public QObject
{
    Q_OBJECT

private slots:
    void oscillatorFrequency_data()
    {
        QTest::addColumn<double>("sampleRate");
        QTest::addColumn<double>("hz");
        QTest::newRow("48k 220") << 48000.0 << 220.0;
        QTest::newRow("44.1k 55") << 44100.0 << 55.0;
        QTest::newRow("96k 1000") << 96000.0 << 1000.0;
    }

    // A rising saw crosses zero downward only at its reset: once per period.
    // (PolyBLEP spreads the reset over two samples, so no single step is large.)
    void oscillatorFrequency()
    {
        QFETCH(double, sampleRate);
        QFETCH(double, hz);

        Oscillator osc;
        osc.setSampleRate(sampleRate);
        osc.setFrequency(hz);

        int resets = 0;
        float prev = osc.next();
        for (int i = 1; i < int(sampleRate); ++i) {
            const float s = osc.next();
            if (prev > 0.0f && s <= 0.0f)
                ++resets;
            prev = s;
        }
        QVERIFY(std::abs(resets - hz) <= 1.0);
    }

    void oscillatorBounded_data()
    {
        QTest::addColumn<int>("shape");
        QTest::newRow("saw") << int(Oscillator::Shape::Saw);
        QTest::newRow("square") << int(Oscillator::Shape::Square);
    }

    // PolyBLEP may overshoot slightly, never wildly, and never produces NaN.
    void oscillatorBounded()
    {
        QFETCH(int, shape);
        Oscillator osc;
        osc.setShape(Oscillator::Shape(shape));
        osc.setSampleRate(44100.0);
        for (double hz : {30.0, 440.0, 5000.0, 19000.0, 40000.0}) {
            osc.setFrequency(hz);
            for (int i = 0; i < 10000; ++i) {
                const float s = osc.next();
                QVERIFY(std::isfinite(s));
                QVERIFY(std::abs(s) <= 1.5f);
            }
        }
    }

    void engineSilentWhenStopped()
    {
        Engine engine;
        engine.prepare(48000.0, 0);
        std::vector<float> l(512), r(512);
        krate::processStereo(engine, l.data(), r.data(), 512);
        for (std::size_t i = 0; i < l.size(); ++i) {
            QCOMPARE(l[i], 0.0f);
            QCOMPARE(r[i], 0.0f);
        }
        QCOMPARE(engine.takePeak(), 0.0f);
    }

    void engineProducesSoundWhenRunning()
    {
        Engine engine;
        TestHost host(engine);
        host.play();
        host.run(24000);
        const float peak = engine.takePeak();
        QVERIFY(peak > 0.05f);
        QVERIFY(peak < 1.0f); // soft clip
        QCOMPARE(host.left, host.right);
        QCOMPARE(engine.currentStep(), 3); // 0.5 s at 120 BPM = 4 steps of 125 ms
    }

    // Commands posted from the GUI land at the start of the next block.
    void engineAppliesCommands()
    {
        Engine engine;
        engine.prepare(48000.0, 0);
        std::vector<float> l(64), r(64);

        Command len;
        len.kind = Command::Kind::SetLength;
        len.index = 4;
        QVERIFY(engine.post(len));

        Command step;
        step.kind = Command::Kind::SetStep;
        step.index = 2;
        step.step = Step{5, 1, true, false, Gate::Tie};
        QVERIFY(engine.post(step));

        krate::processStereo(engine, l.data(), r.data(), 64);
        QCOMPARE(int(engine.sequencer().pattern().length), 4);
        const Step &s = engine.sequencer().pattern().steps[2];
        QCOMPARE(int(s.note), 5);
        QCOMPARE(int(s.octave), 1);
        QVERIFY(s.accent);
        QCOMPARE(int(s.gate), int(Gate::Tie));

        Command load;
        load.kind = Command::Kind::LoadPattern;
        load.pattern.length = 7;
        load.pattern.steps[0].note = 9;
        QVERIFY(engine.post(load));
        krate::processStereo(engine, l.data(), r.data(), 64);
        QCOMPARE(int(engine.sequencer().pattern().length), 7);
        QCOMPARE(int(engine.sequencer().pattern().steps[0].note), 9);
    }

    void engineReportsCurrentStep()
    {
        Engine engine;
        TestHost host(engine);
        host.run(64);
        QCOMPARE(engine.currentStep(), -1); // stopped
        host.play();
        host.run(64);
        QCOMPARE(engine.currentStep(), 0);
        QVERIFY(engine.onBeat());
        host.play(false);
        host.run(64);
        QCOMPARE(engine.currentStep(), -1);
        QVERIFY(!engine.onBeat());
    }

    // A volume jump is smoothed over ~10 ms instead of landing on one sample.
    void volumeIsSmoothed()
    {
        Engine loud;
        TestHost loudHost(loud);
        loud.params().set(ParamId::Volume, 1.0f);
        loudHost.play();
        loudHost.run(4800);
        const float steadyPeak = loud.takePeak();

        Engine engine;
        TestHost host(engine);
        engine.params().set(ParamId::Volume, 0.0f);
        host.play();
        host.run(4800);
        engine.takePeak();
        engine.params().set(ParamId::Volume, 1.0f);
        host.run(32);
        // 32 samples into a 480-sample one-pole: ~6% of the way.
        QVERIFY(engine.takePeak() < 0.2f * steadyPeak);
    }

    // --- Diode ladder ---------------------------------------------------------

    static double runSine(DiodeLadder &f, double hz, double rate, int frames)
    {
        double peak = 0.0;
        for (int i = 0; i < frames; ++i) {
            const double y = f.process(std::sin(2.0 * M_PI * hz * i / rate));
            if (i > frames / 2)
                peak = std::max(peak, std::abs(y));
        }
        return peak;
    }

    void ladderIsLowpass()
    {
        constexpr double rate = 192000.0;
        DiodeLadder f;
        f.setCoefficients(1000.0, rate, 0.0);
        const double pass = runSine(f, 100.0, rate, 96000);
        f.reset();
        const double stop = runSine(f, 16000.0, rate, 96000);
        QVERIFY2(pass > 0.3 && pass < 3.0, qPrintable(QString::number(pass)));
        QVERIFY2(stop < pass * 0.01, qPrintable(QString::number(stop / pass))); // > 40 dB down
    }

    // The resonant peak sits near 0.67 x the coefficient frequency; there,
    // high feedback must lift the response well above the no-feedback curve.
    void ladderResonancePeaks()
    {
        constexpr double rate = 192000.0;
        constexpr double peakHz = 670.0;
        DiodeLadder f;
        f.setCoefficients(1000.0, rate, 0.0);
        const double flat = runSine(f, peakHz, rate, 96000);
        f.reset();
        f.setCoefficients(1000.0, rate, 15.0);
        const double resonant = runSine(f, peakHz, rate, 96000);
        QVERIFY2(resonant > 4.0 * flat, qPrintable(QString("%1 vs %2").arg(resonant).arg(flat)));
    }

    // Passband (DC) gain is 1 / (1 + k): Voice's makeup gain relies on this.
    void ladderPassbandGain()
    {
        for (double k : {0.0, 5.0, 15.0}) {
            DiodeLadder f;
            f.setCoefficients(1000.0, 192000.0, k);
            double y = 0.0;
            for (int i = 0; i < 192000; ++i)
                y = f.process(1.0);
            QVERIFY2(std::abs(y * (1.0 + k) - 1.0) < 0.02, qPrintable(QString("k=%1 dc=%2").arg(k).arg(y)));
        }
    }

    void ladderStable_data()
    {
        QTest::addColumn<double>("rate");
        QTest::newRow("44.1k x4") << 44100.0 * 4;
        QTest::newRow("48k x4") << 48000.0 * 4;
        QTest::newRow("96k x4") << 96000.0 * 4;
    }

    // Full feedback, cutoff swept across the whole range, loud saw input.
    void ladderStable()
    {
        QFETCH(double, rate);
        DiodeLadder f;
        Oscillator osc;
        osc.setSampleRate(rate);
        osc.setFrequency(55.0);
        const int frames = int(rate * 2);
        for (int i = 0; i < frames; ++i) {
            const double t = double(i) / frames;
            const double cutoff = 20.0 * std::pow(0.45 * rate / 4 / 20.0, std::abs(std::sin(7.0 * M_PI * t)));
            f.setCoefficients(cutoff, rate, 15.0);
            const double y = f.process(std::tanh(3.0 * osc.next()));
            QVERIFY(std::isfinite(y));
            QVERIFY(std::abs(y) < 10.0);
        }
    }

    // --- Decimator ------------------------------------------------------------

    void decimatorPassesDcAndRejectsHighBand()
    {
        HalfbandDecimator<31> d;
        float y = 0.0f;
        for (int i = 0; i < 100; ++i)
            y = d.process(1.0f, 1.0f);
        QVERIFY(std::abs(y - 1.0f) < 1e-4f);

        // 0.42 of the input rate is above the output Nyquist: must be removed, not folded back.
        d.reset();
        float peak = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            const float a = float(std::sin(2.0 * M_PI * 0.42 * (2 * i)));
            const float b = float(std::sin(2.0 * M_PI * 0.42 * (2 * i + 1)));
            const float out = d.process(a, b);
            if (i > 100)
                peak = std::max(peak, std::abs(out));
        }
        QVERIFY2(peak < 0.003f, qPrintable(QString::number(peak))); // > 50 dB down
    }

    // --- Voice ----------------------------------------------------------------

    static VoiceParams openFilter()
    {
        VoiceParams p;
        p.cutoffHz = 5000.0;
        p.resonance = 0.0;
        p.envModOctaves = 0.0;
        return p;
    }

    void voicePitch()
    {
        Voice v;
        v.setSampleRate(48000.0);
        v.setParams(openFilter());
        v.noteOn(57, false); // A3, 220 Hz
        for (int i = 0; i < 4800; ++i)
            v.next();
        int periods = 0;
        float prev = v.next();
        for (int i = 0; i < 48000; ++i) {
            const float s = v.next();
            if (prev > 0.0f && s <= 0.0f)
                ++periods;
            prev = s;
        }
        QVERIFY2(std::abs(periods - 220) <= 2, qPrintable(QString::number(periods)));
    }

    void voiceTuningShiftsPitch()
    {
        Voice v;
        v.setSampleRate(48000.0);
        auto p = openFilter();
        p.tuningSemitones = 12.0;
        v.setParams(p);
        v.noteOn(57, false);
        for (int i = 0; i < 4800; ++i)
            v.next();
        int periods = 0;
        float prev = v.next();
        for (int i = 0; i < 48000; ++i) {
            const float s = v.next();
            if (prev > 0.0f && s <= 0.0f)
                ++periods;
            prev = s;
        }
        QVERIFY2(std::abs(periods - 440) <= 3, qPrintable(QString::number(periods)));
    }

    static float notePeak(bool accent)
    {
        Voice v;
        v.setSampleRate(48000.0);
        VoiceParams p;
        p.accent = 1.0;
        v.setParams(p);
        v.noteOn(36, accent);
        float peak = 0.0f;
        for (int i = 0; i < 9600; ++i)
            peak = std::max(peak, std::abs(v.next()));
        return peak;
    }

    void voiceAccentIsLouder()
    {
        QVERIFY(notePeak(true) > 1.3f * notePeak(false));
    }

    void voiceReleasesAfterGateOff()
    {
        Voice v;
        v.setSampleRate(48000.0);
        v.noteOn(48, false);
        for (int i = 0; i < 4800; ++i)
            v.next();
        v.gateOff();
        for (int i = 0; i < 4800; ++i) // 100 ms: release is ~9 ms
            v.next();
        float peak = 0.0f;
        for (int i = 0; i < 480; ++i)
            peak = std::max(peak, std::abs(v.next()));
        QVERIFY(peak < 1e-4f);
    }

    // Extreme settings at every rate: finite, bounded.
    void voiceStableAtExtremes()
    {
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            Voice v;
            v.setSampleRate(rate);
            VoiceParams p;
            p.cutoffHz = units::cutoffHz(1.0f);
            p.resonance = 1.0;
            p.envModOctaves = units::envModOctaves(1.0f);
            p.decaySeconds = units::decaySeconds(0.0f);
            p.accent = 1.0;
            p.square = true;
            p.tuningSemitones = 12.0;
            v.setParams(p);
            for (int n = 0; n < 40; ++n) {
                v.noteOn(n % 2 ? 60 : 24, true);
                for (int i = 0; i < int(rate / 20); ++i) {
                    const float s = v.next();
                    QVERIFY(std::isfinite(s));
                    QVERIFY(std::abs(s) < 8.0f);
                }
            }
        }
    }
};

QTEST_APPLESS_MAIN(TestDsp)
#include "tst_dsp.moc"
