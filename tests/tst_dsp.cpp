#include <QtTest>

#include <cmath>
#include <vector>

#include "dsp/Engine.h"
#include "dsp/Oscillator.h"
#include "dsp/Params.h"

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
        engine.setSampleRate(48000.0);
        std::vector<float> l(512), r(512);
        engine.process(l.data(), r.data(), 512);
        for (std::size_t i = 0; i < l.size(); ++i) {
            QCOMPARE(l[i], 0.0f);
            QCOMPARE(r[i], 0.0f);
        }
        QCOMPARE(engine.takePeak(), 0.0f);
    }

    void engineRampsUpWhenRunning()
    {
        Engine engine;
        engine.setSampleRate(48000.0);
        engine.setRunning(true);
        std::vector<float> l(4800), r(4800);
        engine.process(l.data(), r.data(), 4800);

        // No click: the first samples are tiny, the tail is at the test-tone level.
        QVERIFY(std::abs(l[0]) < 0.01f);
        const float expected = 0.125f * float(units::volumeGain(paramInfo(ParamId::Volume).defaultValue));
        const float peak = engine.takePeak();
        QVERIFY(peak > 0.8f * expected && peak <= 1.05f * expected);
        QCOMPARE(l, r);
    }

    // Count downward zero crossings (one per saw period) over `frames`.
    static int periods(Engine &engine, int frames)
    {
        std::vector<float> l(frames), r(frames);
        engine.process(l.data(), r.data(), frames);
        int n = 0;
        for (int i = 1; i < frames; ++i)
            if (l[i - 1] > 0.0f && l[i] <= 0.0f)
                ++n;
        return n;
    }

    void tuningMovesPitch()
    {
        Engine engine;
        engine.setSampleRate(48000.0);
        engine.setRunning(true);
        QVERIFY(std::abs(periods(engine, 48000) - 220) <= 1);

        engine.params().set(ParamId::Tuning, 1.0f); // +12 semitones
        periods(engine, 4800);                      // let the 10 ms glide settle
        QVERIFY(std::abs(periods(engine, 48000) - 440) <= 1);
    }

    // A volume jump is smoothed: no sample-to-sample step bigger than the
    // waveform itself produces (the saw's own reset is the largest).
    void volumeIsSmoothed()
    {
        Engine engine;
        engine.setSampleRate(48000.0);
        engine.setRunning(true);
        engine.params().set(ParamId::Volume, 0.0f);
        std::vector<float> l(9600), r(9600);
        engine.process(l.data(), r.data(), 9600); // settle at silence (after run ramp)

        engine.params().set(ParamId::Volume, 1.0f);
        engine.process(l.data(), r.data(), 64);
        // 64 samples into a 10 ms (480-sample) one-pole: ~12% of the way, not 100%.
        float maxAbs = 0.0f;
        for (int i = 0; i < 64; ++i)
            maxAbs = std::max(maxAbs, std::abs(l[i]));
        QVERIFY(maxAbs < 0.125f * 0.2f);
    }

    void waveformSwitchesShape()
    {
        Engine engine;
        engine.setSampleRate(48000.0);
        engine.setRunning(true);
        engine.params().set(ParamId::Waveform, 1.0f); // square
        std::vector<float> l(9600), r(9600);
        engine.process(l.data(), r.data(), 9600);

        // A settled square sits near +/- its peak almost all the time; a saw doesn't.
        const float peak = engine.takePeak();
        int nearPeak = 0;
        for (int i = 4800; i < 9600; ++i)
            if (std::abs(l[i]) > 0.9f * peak)
                ++nearPeak;
        QVERIFY(nearPeak > 4800 * 0.9);
    }
};

QTEST_APPLESS_MAIN(TestDsp)
#include "tst_dsp.moc"
