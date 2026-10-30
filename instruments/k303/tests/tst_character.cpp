#include <QtTest>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "core/Instrument.h"
#include "dsp/Engine.h"
#include "dsp/Oscillator.h"
#include "dsp/Params.h"
#include "dsp/Voice.h"

using namespace k303;

// The fuller sound (docs/DSP.md): the bass kept under resonance, the bent saw
// and the shaped square, the accent sweep, and the output stage (drive, tone,
// delay, reverb, width). Each control measured.
class TestCharacter : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    static double partial(const std::vector<float> &x, double hz)
    {
        double a = 0.0, b = 0.0;
        for (std::size_t n = 0; n < x.size(); ++n) {
            const double w = 2.0 * std::numbers::pi * hz * double(n) / kRate;
            a += x[n] * std::sin(w);
            b += x[n] * std::cos(w);
        }
        return 2.0 * std::hypot(a, b) / double(x.size());
    }

    // Brightness: the RMS of the sample-to-sample difference over the RMS (a
    // derivative weights each partial by its frequency).
    static double brightness(const std::vector<float> &x)
    {
        double d = 0.0, e = 0.0;
        for (std::size_t i = 1; i < x.size(); ++i) {
            d += double(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
            e += double(x[i]) * x[i];
        }
        return std::sqrt(d / e);
    }

    // A held A1 through the voice alone (no envelope sweep), 0.25 .. 0.75 s.
    static std::vector<float> held(double resonance, double cutoffHz = 1000.0)
    {
        Voice v;
        v.setSampleRate(kRate);
        VoiceParams p;
        p.cutoffHz = cutoffHz;
        p.resonance = resonance;
        p.envModOctaves = 0.0;
        p.accent = 0.0;
        v.setParams(p);
        v.noteOn(33, false);
        std::vector<float> x;
        for (int i = 0; i < 36000; ++i) {
            const float s = v.next();
            if (i >= 12000)
                x.push_back(s);
        }
        return x;
    }

    struct Stereo
    {
        std::vector<float> left, right;
    };

    // The engine playing MIDI notes (one every 0.25 s, the first accented) with `params`.
    static Stereo play(std::vector<std::pair<ParamId, float>> params, double seconds, int notes = 4)
    {
        Engine engine;
        engine.prepare(kRate, kBlock);
        for (const auto &[id, v] : params)
            engine.params().set(id, v);
        Stereo out;
        std::vector<float> l(kBlock), r(kBlock);
        const auto noteFrames = std::uint64_t(0.25 * kRate);
        for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
            // Each note held 0.18 s of its 0.25 s.
            std::vector<krate::MidiEvent> midi;
            const std::uint64_t n = f / noteFrames, at = f % noteFrames;
            const auto key = std::uint8_t(36 + 7 * (n % 2));
            if (n < std::uint64_t(notes) && at < kBlock)
                midi.push_back({0, 3, {0x90, key, std::uint8_t(n == 0 ? 120 : 80)}});
            const auto off = std::uint64_t(0.18 * kRate);
            if (n < std::uint64_t(notes) && at <= off && off < at + kBlock)
                midi.push_back({std::uint32_t(off - at), 3, {0x80, key, 0}});
            krate::processStereo(engine, l.data(), r.data(), kBlock, midi.data(), std::uint32_t(midi.size()));
            out.left.insert(out.left.end(), l.begin(), l.end());
            out.right.insert(out.right.end(), r.begin(), r.end());
        }
        return out;
    }

    static double rms(const std::vector<float> &x, double from, double to)
    {
        const auto a = std::size_t(from * kRate), b = std::min(x.size(), std::size_t(to * kRate));
        double sum = 0.0;
        for (std::size_t i = a; i < b; ++i)
            sum += double(x[i]) * x[i];
        return std::sqrt(sum / double(std::max<std::size_t>(b - a, 1)));
    }

private slots:
    // At the default resonance the bass keeps its weight: the fundamental
    // within 3 dB of no resonance (it was 8 dB down); full resonance still thins it.
    void bassKeptUnderResonance()
    {
        const double open = partial(held(0.0), 55.0);
        const double half = partial(held(0.5), 55.0);
        const double full = partial(held(1.0), 55.0);
        const double halfDb = 20.0 * std::log10(half / open), fullDb = 20.0 * std::log10(full / open);
        QVERIFY2(halfDb > -3.0, qPrintable(QString::number(halfDb)));
        QVERIFY2(fullDb < halfDb, qPrintable(QStringLiteral("%1 %2").arg(halfDb).arg(fullDb)));
    }

    // The saw is bent (below the straight ramp mid-cycle) and has no DC.
    void sawIsBentWithoutDc()
    {
        Oscillator osc;
        osc.setSampleRate(kRate);
        osc.setFrequency(kRate / 1000.0); // 1000 samples per cycle
        std::vector<float> x(1000);
        for (float &s : x)
            s = osc.next();
        double mean = 0.0;
        for (float s : x)
            mean += s;
        mean /= double(x.size());
        QVERIFY2(std::fabs(mean) < 0.005, qPrintable(QString::number(mean)));
        QVERIFY2(std::fabs(x[500] - (-0.04f)) < 0.01f, qPrintable(QString::number(x[500]))); // a ramp: 0
    }

    // The square, made from the saw: a little under half high, its rising
    // edge (the comparator) slower than its falling one (the saw's reset).
    void squareIsShapedFromTheSaw()
    {
        Oscillator osc;
        osc.setSampleRate(kRate);
        osc.setShape(Oscillator::Shape::Square);
        osc.setFrequency(kRate / 1000.0);
        std::vector<float> x(2000);
        for (float &s : x)
            s = osc.next();
        int high = 0, rising = 0, falling = 0;
        for (std::size_t i = 1000; i < 2000; ++i) {
            high += x[i] > 0.0f ? 1 : 0;
            if (std::fabs(x[i]) < 0.9f)
                (x[i] > x[i - 1] ? rising : falling) += 1;
        }
        QVERIFY2(std::abs(high - 450) <= 5, qPrintable(QString::number(high))); // 45 %, docs/DSP.md#oscillator
        QVERIFY2(rising > 2 * std::max(falling, 1), qPrintable(QStringLiteral("%1 %2").arg(rising).arg(falling)));
    }

    // Accent Sweep: how far an accented note opens the filter.
    void sweepOpensAccents()
    {
        const auto bright = [](float sweep) {
            const Stereo s = play({{ParamId::Sweep, sweep}, {ParamId::Accent, 1.0f}, {ParamId::Cutoff, 0.2f}}, 0.1, 1);
            return brightness(std::vector<float>(s.left.begin() + 480, s.left.begin() + 480 + 2048));
        };
        QVERIFY2(bright(1.0f) > 1.3 * bright(0.0f), qPrintable(QStringLiteral("%1 %2").arg(bright(0.0f)).arg(bright(1.0f))));
    }

    // Drive: denser (peak to average down), louder by no more than ~4 dB.
    void driveDensifies()
    {
        const Stereo clean = play({}, 1.0), driven = play({{ParamId::Drive, 1.0f}}, 1.0);
        const auto crest = [](const std::vector<float> &x) {
            float p = 0.0f;
            for (float v : x)
                p = std::max(p, std::fabs(v));
            return double(p) / rms(x, 0.0, 1.0);
        };
        QVERIFY2(crest(driven.left) < 0.7 * crest(clean.left),
                 qPrintable(QStringLiteral("%1 %2").arg(crest(clean.left)).arg(crest(driven.left))));
        const double gainDb = 20.0 * std::log10(rms(driven.left, 0.0, 1.0) / rms(clean.left, 0.0, 1.0));
        QVERIFY2(gainDb > 0.0 && gainDb < 4.5, qPrintable(QString::number(gainDb)));
    }

    // Tone darkens the driven sound.
    void toneDarkens()
    {
        const auto bright = [](float tone) {
            const Stereo s = play({{ParamId::Drive, 0.6f}, {ParamId::Tone, tone}}, 0.2, 1);
            return brightness(std::vector<float>(s.left.begin() + 480, s.left.begin() + 480 + 2048));
        };
        QVERIFY2(bright(1.0f) > 1.5 * bright(0.0f), qPrintable(QStringLiteral("%1 %2").arg(bright(0.0f)).arg(bright(1.0f))));
    }

    // Effects off: the bass is mono (both sides identical). The delay echoes
    // after the note; the reverb leaves a tail; Width 0 folds the effects to mono.
    void effectsAndWidth()
    {
        const Stereo dry = play({}, 1.0, 1);
        QVERIFY(dry.left == dry.right);
        QVERIFY2(rms(dry.left, 0.6, 0.9) < 1e-6, qPrintable(QString::number(rms(dry.left, 0.6, 0.9))));

        const Stereo delayed = play({{ParamId::DelayLevel, 0.6f}}, 1.0, 1);
        QVERIFY(delayed.left != delayed.right); // ping-pong
        QVERIFY2(rms(delayed.left, 0.3, 0.9) + rms(delayed.right, 0.3, 0.9) > 1e-3, "no echoes");

        const Stereo reverb = play({{ParamId::ReverbLevel, 0.6f}}, 1.0, 1);
        QVERIFY2(rms(reverb.left, 0.6, 0.9) > 1e-4, qPrintable(QString::number(rms(reverb.left, 0.6, 0.9))));

        const Stereo narrow = play({{ParamId::DelayLevel, 0.6f}, {ParamId::ReverbLevel, 0.6f}, {ParamId::Width, 0.0f}}, 1.0, 1);
        for (std::size_t i = 0; i < narrow.left.size(); ++i)
            QVERIFY(std::fabs(narrow.left[i] - narrow.right[i]) < 1e-6f);
    }
};

QTEST_GUILESS_MAIN(TestCharacter)
#include "tst_character.moc"
