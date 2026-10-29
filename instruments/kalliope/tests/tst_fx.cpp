#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

#include "kalliope/dsp/Effects.h"
#include "kalliope/dsp/Engine.h"

using namespace kalliope;

// Chorus and delay (docs/DSP.md#effects).
class TestFx : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;

    static double rms(const std::vector<float> &x, std::size_t from, std::size_t to)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < to; ++i)
            sum += double(x[i]) * x[i];
        return std::sqrt(sum / double(to - from));
    }

private slots:
    // An impulse: the first echo on the left after the delay time, the next on the right.
    void delayPingPongs()
    {
        Delay delay;
        delay.prepare(kRate);
        const std::size_t n = 40000;
        std::vector<float> l(n, 0.0f), r(n, 0.0f);
        l[0] = r[0] = 1.0f;
        delay.process(l.data(), r.data(), std::uint32_t(n), 0.25, 1.0, 0.5);
        QCOMPARE(l[12000], 1.0f); // 0.25 s at 48 kHz, full level
        for (std::size_t i = 1; i < 12000; ++i)
            QVERIFY(l[i] == 0.0f && r[i] == 0.0f);
        for (std::size_t i = 12001; i < 24000; ++i)
            QVERIFY(std::fabs(l[i]) < 1e-6f && r[i] == 0.0f);
        float second = 0.0f;
        for (std::size_t i = 24000; i < 24400; ++i)
            second = std::max(second, std::fabs(r[i]));
        QVERIFY(second > 0.0f && second < 0.5f); // half the level, darkened by the feedback low-pass
    }

    void delayFeedbackStaysBounded()
    {
        Delay delay;
        delay.prepare(kRate);
        std::uint32_t seed = 1;
        float peak = 0.0f;
        std::vector<float> l(4800), r(4800);
        for (int block = 0; block < 100; ++block) { // 10 s of noise, feedback at maximum
            for (std::size_t i = 0; i < l.size(); ++i) {
                seed = seed * 1664525u + 1013904223u;
                l[i] = r[i] = float(std::int32_t(seed)) / 4294967296.0f; // -0.5 .. 0.5
            }
            delay.process(l.data(), r.data(), std::uint32_t(l.size()), 0.1, 1.0, 0.9);
            for (std::size_t i = 0; i < l.size(); ++i) {
                QVERIFY(std::isfinite(l[i]) && std::isfinite(r[i]));
                peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
            }
        }
        QVERIFY2(peak < 6.0f, qPrintable(QString::number(peak)));
    }

    void chorusMakesStereo()
    {
        const std::size_t n = 48000;
        std::vector<float> in(n);
        for (std::size_t i = 0; i < n; ++i)
            in[i] = float(0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * double(i) / kRate));
        for (int mode = 0; mode < 3; ++mode) {
            Chorus chorus;
            chorus.prepare(kRate);
            std::vector<float> l(n), r(n);
            chorus.process(in.data(), l.data(), r.data(), std::uint32_t(n), mode);
            if (mode == 0) {
                QVERIFY(l == in && r == in);
                continue;
            }
            QVERIFY(l != r);
            const double ratio = rms(l, 4800, n) / rms(in, 4800, n);
            QVERIFY2(ratio > 0.5 && ratio < 1.4, qPrintable(QString::number(ratio)));
        }
    }

    // Through the engine: the delay time follows the transport's tempo (1/8 note).
    void delayFollowsTheTempo()
    {
        for (double bpm : {120.0, 60.0}) {
            Engine engine;
            engine.prepare(kRate, 64);
            engine.setParam(ParamId::Chorus, 0.0f);
            engine.setParam(ParamId::DelayLevel, 1.0f);
            engine.setParam(ParamId::DelayFb, 0.0f);
            engine.setParam(ParamId::Env2D, 0.2f); // a short blip
            engine.setParam(ParamId::Env2S, 0.0f);
            engine.setParam(ParamId::Env2R, 0.2f);
            krate::TransportState t;
            t.bpm = bpm;
            std::vector<float> out;
            std::vector<float> l(64), r(64);
            for (int block = 0; block < 1200; ++block) {
                const krate::MidiEvent on{0, 3, {0x90, 72, 127}};
                const krate::MidiEvent off{0, 3, {0x80, 72, 0}};
                const krate::MidiEvent *midi = block == 0 ? &on : block == 10 ? &off : nullptr;
                krate::processStereo(engine, l.data(), r.data(), 64, midi, midi ? 1 : 0, &t);
                out.insert(out.end(), l.begin(), l.end());
            }
            const auto echo = std::size_t(0.5 * 60.0 / bpm * kRate); // 1/8 = half a beat
            const double atEcho = rms(out, echo, echo + 480);
            const double before = rms(out, echo - 2400, echo - 480);
            QVERIFY2(atEcho > 10.0 * before, qPrintable(QStringLiteral("%1 BPM: %2 vs %3").arg(bpm).arg(atEcho).arg(before)));
        }
    }
};

QTEST_GUILESS_MAIN(TestFx)
#include "tst_fx.moc"
