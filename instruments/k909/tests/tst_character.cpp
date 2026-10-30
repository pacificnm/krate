#include <QtTest>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "k909/dsp/Engine.h"

using namespace k909;

// The voices' shaping knobs, the mixer and the effects (docs/DSP.md#voices,
// docs/DSP.md#mixer-and-effects): each control does what it says, measured.
class TestCharacter : public QObject
{
    Q_OBJECT

    static constexpr double kRate = 48000.0;
    static constexpr std::uint32_t kBlock = 64;

    struct Render
    {
        std::vector<float> left, right;
    };

    // One hit of `note` (frame 0) with `params` set, `seconds` long.
    static Render hit(int note, std::vector<std::pair<ParamId, float>> params, double seconds, int velocity = 100)
    {
        Engine engine;
        engine.prepare(kRate, kBlock);
        for (const auto &[id, v] : params)
            engine.setParam(int(id), v);
        Render out;
        std::vector<float> l(kBlock), r(kBlock);
        for (std::uint64_t f = 0; f < std::uint64_t(seconds * kRate); f += kBlock) {
            const krate::MidiEvent on{0, 3, {0x99, std::uint8_t(note), std::uint8_t(velocity)}};
            krate::processStereo(engine, l.data(), r.data(), kBlock, f == 0 ? &on : nullptr, f == 0 ? 1 : 0);
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

    static double peak(const std::vector<float> &x)
    {
        double p = 0.0;
        for (float v : x)
            p = std::max(p, double(std::fabs(v)));
        return p;
    }

    // Frequency from upward zero crossings in [from, to) seconds.
    static double frequency(const std::vector<float> &x, double from, double to)
    {
        double first = -1.0, last = -1.0;
        int cycles = -1;
        for (auto i = std::size_t(from * kRate) + 1; i < std::size_t(to * kRate); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
                const double t = double(i - 1) + double(x[i - 1]) / double(x[i - 1] - x[i]);
                if (first < 0.0)
                    first = t;
                last = t;
                ++cycles;
            }
        return cycles > 0 ? cycles * kRate / (last - first) : 0.0;
    }

    // Spectral centroid of the first n samples (plain DFT, coarse bins).
    static double centroid(const std::vector<float> &x, std::size_t n)
    {
        double weighted = 0.0, total = 0.0;
        for (std::size_t k = 1; k < n / 2; k += 4) {
            double re = 0.0, im = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double w = 2.0 * std::numbers::pi * double(k * i) / double(n);
                re += x[i] * std::cos(w);
                im += x[i] * std::sin(w);
            }
            const double power = re * re + im * im;
            weighted += power * double(k) * kRate / double(n);
            total += power;
        }
        return weighted / total;
    }

    // The level of harmonic h of f0 against the fundamental, over [from, to) seconds.
    static double harmonic(const std::vector<float> &x, double f0, int h, double from, double to)
    {
        double a1 = 0, b1 = 0, ah = 0, bh = 0;
        for (auto n = std::size_t(from * kRate); n < std::size_t(to * kRate); ++n) {
            const double w = 2.0 * std::numbers::pi * f0 * double(n) / kRate;
            a1 += x[n] * std::sin(w);
            b1 += x[n] * std::cos(w);
            ah += x[n] * std::sin(h * w);
            bh += x[n] * std::cos(h * w);
        }
        return std::hypot(ah, bh) / std::hypot(a1, b1);
    }

    // The amplitude of the component at `hz` over [from, to) seconds.
    static double partial(const std::vector<float> &x, double hz, double from, double to)
    {
        double a = 0, b = 0;
        for (auto n = std::size_t(from * kRate); n < std::size_t(to * kRate); ++n) {
            const double w = 2.0 * std::numbers::pi * hz * double(n) / kRate;
            a += x[n] * std::sin(w);
            b += x[n] * std::cos(w);
        }
        return std::hypot(a, b);
    }

private slots:
    // Drive keeps the peak and brings the tail up (the body sustains, as through a distortion).
    void driveThickensAndKeepsThePeak()
    {
        const Render clean = hit(36, {}, 0.6);
        const Render driven = hit(36, {{ParamId::BdDrive, 1.0f}}, 0.6);
        QVERIFY2(peak(driven.left) < 1.25 * peak(clean.left), qPrintable(QStringLiteral("%1 %2").arg(peak(clean.left)).arg(peak(driven.left))));
        const double cleanTail = rms(clean.left, 0.25, 0.45) / peak(clean.left);
        const double drivenTail = rms(driven.left, 0.25, 0.45) / peak(driven.left);
        QVERIFY2(drivenTail > 2.0 * cleanTail, qPrintable(QStringLiteral("%1 %2").arg(cleanTail).arg(drivenTail)));
    }

    // Sweep 0: the kick starts at its pitch; up: it starts well above it. Bend: how long the drop takes.
    void kickSweepAndBend()
    {
        const auto start = [](float sweep, float bend) {
            return frequency(hit(36, {{ParamId::BdSweep, sweep}, {ParamId::BdBend, bend}, {ParamId::BdAttack, 0.0f}}, 0.2).left,
                             0.012, 0.04);
        };
        const double flat = start(0.0f, 0.5f), swept = start(1.0f, 0.5f), slow = start(1.0f, 1.0f);
        QVERIFY2(flat < 60.0, qPrintable(QString::number(flat)));         // 55 Hz at the default Tune
        QVERIFY2(swept > 1.3 * flat, qPrintable(QString::number(swept)));
        QVERIFY2(slow > 1.3 * swept, qPrintable(QStringLiteral("%1 %2").arg(swept).arg(slow)));
    }

    // Tone: a rounder or squarer body (its third harmonic).
    void kickToneShapesTheBody()
    {
        const auto third = [](float tone) {
            return harmonic(hit(36, {{ParamId::BdTone, tone}, {ParamId::BdSweep, 0.0f}, {ParamId::BdAttack, 0.0f}}, 0.3).left,
                            55.0, 3, 0.05, 0.25);
        };
        QVERIFY2(third(1.0f) > 4.0 * third(0.0f), qPrintable(QStringLiteral("%1 %2").arg(third(0.0f)).arg(third(1.0f))));
    }

    // Crack: a bright snap on the snare's first milliseconds.
    void snareCrackIsBright()
    {
        const double plain = centroid(hit(38, {}, 0.05).left, 1024);
        const double crack = centroid(hit(38, {{ParamId::SdCrack, 1.0f}}, 0.05).left, 1024);
        QVERIFY2(crack > 1.3 * plain, qPrintable(QStringLiteral("%1 %2").arg(plain).arg(crack)));
    }

    // Snare Decay: the body rings longer.
    void snareDecayLengthensTheBody()
    {
        const auto tail = [](float decay) {
            return rms(hit(38, {{ParamId::SdDecay, decay}, {ParamId::SdSnappy, 0.0f}}, 0.5).left, 0.15, 0.3);
        };
        QVERIFY2(tail(1.0f) > 3.0 * tail(0.0f), qPrintable(QStringLiteral("%1 %2").arg(tail(0.0f)).arg(tail(1.0f))));
    }

    // Tune moves the rim, clap and hats up and down (their spectral centroid).
    void tuneMovesRimClapAndHats()
    {
        for (const auto &[note, id] : {std::pair{37, ParamId::RsTune}, std::pair{39, ParamId::CpTune},
                                       std::pair{42, ParamId::HhTune}}) {
            const double low = centroid(hit(note, {{id, 0.0f}}, 0.05).left, 2048);
            const double high = centroid(hit(note, {{id, 1.0f}}, 0.05).left, 2048);
            QVERIFY2(high > 1.2 * low, qPrintable(QStringLiteral("note %1: %2 %3").arg(note).arg(low).arg(high)));
        }
    }

    // Clap Spread: the three bursts further apart, so the clap's front lasts longer.
    void clapSpreadWidensTheBursts()
    {
        const auto front = [](float spread) {
            const Render r = hit(39, {{ParamId::CpSpread, spread}}, 0.15);
            return rms(r.left, 0.045, 0.075) / rms(r.left, 0.0, 0.01);
        };
        QVERIFY2(front(1.0f) > 1.5 * front(0.0f), qPrintable(QStringLiteral("%1 %2").arg(front(0.0f)).arg(front(1.0f))));
    }

    // Pan hard left: nothing on the right.
    void panPlacesTheVoice()
    {
        const Render r = hit(36, {{ParamId::BdPan, 0.0f}}, 0.1);
        QVERIFY(peak(r.left) > 0.1);
        QCOMPARE(peak(r.right), 0.0);
    }

    // A reverb send leaves a tail; gated, the tail is cut after the hold.
    void reverbSendAndGate()
    {
        const auto tail = [](float send, float gate) {
            const Render r = hit(37, {{ParamId::RsRev, send}, {ParamId::RevGate, gate}, {ParamId::RevSize, 0.0f}}, 0.8);
            return std::pair{rms(r.left, 0.05, 0.08), rms(r.left, 0.4, 0.7)};
        };
        const auto dry = tail(0.0f, 0.0f), wet = tail(1.0f, 0.0f), gated = tail(1.0f, 1.0f);
        QVERIFY2(dry.second < 1e-6, qPrintable(QString::number(dry.second)));
        QVERIFY2(wet.first > 1e-3, qPrintable(QString::number(wet.first)));
        QVERIFY2(gated.first > 1e-3, qPrintable(QString::number(gated.first))); // open: the hold is 80 ms
        QVERIFY2(gated.second < 0.05 * tail(1.0f, 1.0f).first, qPrintable(QString::number(gated.second)));
    }

    // A delay send echoes at the Delay Time (1/8 at the default 120 BPM: 0.25 s).
    void delaySendEchoes()
    {
        const Render r = hit(37, {{ParamId::RsDly, 1.0f}}, 0.4);
        const double echo = rms(r.left, 0.25, 0.26), before = rms(r.left, 0.18, 0.24);
        QVERIFY2(echo > 10.0 * before, qPrintable(QStringLiteral("%1 %2").arg(before).arg(echo)));
    }

    // The compressor lowers the peak-to-average ratio of a beat.
    void compressorSquashes()
    {
        const auto crest = [](float comp) {
            Engine engine;
            engine.prepare(kRate, kBlock);
            engine.setParam(int(ParamId::Comp), comp);
            krate::TransportState t;
            t.playing = true;
            t.restarted = true;
            t.bpm = 120.0;
            std::vector<float> out, l(kBlock), r(kBlock);
            for (int b = 0; b < int(2.0 * kRate) / int(kBlock); ++b) {
                krate::processStereo(engine, l.data(), r.data(), kBlock, nullptr, 0, &t);
                t.restarted = false;
                t.beatAtBlockStart += kBlock * 2.0 / kRate;
                out.insert(out.end(), l.begin(), l.end());
            }
            return peak(out) / rms(out, 0.0, 2.0);
        };
        QVERIFY2(crest(1.0f) < 0.75 * crest(0.0f), qPrintable(QStringLiteral("%1 %2").arg(crest(0.0f)).arg(crest(1.0f))));
    }

    // Crush: the sample rate drops (samples held) and the steps get coarse.
    void crushHoldsAndQuantizes()
    {
        const Render r = hit(46, {{ParamId::Crush, 1.0f}}, 0.1);
        int repeats = 0;
        for (std::size_t i = 1; i < 2400; ++i)
            repeats += r.left[i] == r.left[i - 1] ? 1 : 0;
        QVERIFY2(repeats > 2000, qPrintable(QString::number(repeats)));
        const double step = std::pow(2.0, -3.0); // 4 bits
        for (std::size_t i = 0; i < 2400; ++i) {
            const double v = double(r.left[i]) / (0.7071 * 0.64); // back through Volume and the trim
            QVERIFY2(std::fabs(v / step - std::round(v / step)) < 0.01, qPrintable(QString::number(v)));
        }
    }

    // ---- The industrial voices

    // Metal's Material: a pipe's modes far apart (2.76 x the lowest), a plate's close (1.36 x).
    void metalMaterialMovesTheModes()
    {
        const double f0 = 120.0 * std::pow(1500.0 / 120.0, 0.5); // Tune 50 %
        const auto mode = [&](float material, double ratio) {
            const Render r = hit(56, {{ParamId::MeMaterial, material}, {ParamId::MePan, 0.5f}}, 0.4);
            return partial(r.left, f0 * ratio, 0.05, 0.35) / partial(r.left, f0, 0.05, 0.35);
        };
        QVERIFY2(mode(0.0f, 2.756) > 5.0 * mode(0.0f, 1.36), qPrintable(QStringLiteral("%1 %2").arg(mode(0.0f, 2.756)).arg(mode(0.0f, 1.36))));
        QVERIFY2(mode(1.0f, 1.36) > 5.0 * mode(1.0f, 2.756), qPrintable(QStringLiteral("%1 %2").arg(mode(1.0f, 1.36)).arg(mode(1.0f, 2.756))));
    }

    // Metal and Boom ring for their Decay.
    void metalAndBoomDecay()
    {
        for (const auto &[note, id] : {std::pair{56, ParamId::MeDecay}, std::pair{34, ParamId::BmDecay}}) {
            const auto tail = [&](float decay) { return rms(hit(note, {{id, decay}}, 1.2).left, 0.6, 1.0); };
            QVERIFY2(tail(1.0f) > 30.0 * tail(0.0f) + 1e-9, qPrintable(QStringLiteral("note %1: %2 %3").arg(note).arg(tail(0.0f)).arg(tail(1.0f))));
        }
    }

    // Clank's Hardness: more FM, a brighter strike.
    void clankHardnessBrightens()
    {
        const double soft = centroid(hit(54, {{ParamId::CkHard, 0.0f}}, 0.05).left, 2048);
        const double hard = centroid(hit(54, {{ParamId::CkHard, 1.0f}}, 0.05).left, 2048);
        QVERIFY2(hard > 2.0 * soft, qPrintable(QStringLiteral("%1 %2").arg(soft).arg(hard)));
    }

    // Noise Stab's Sweep: the filter starts high and falls (bright, then darker).
    void noiseSweepFalls()
    {
        const Render r = hit(52, {{ParamId::NzSweep, 1.0f}, {ParamId::NzDecay, 0.8f}}, 0.4);
        const std::vector<float> late(r.left.begin() + 9600, r.left.end());
        const double early = centroid(r.left, 2048), after = centroid(late, 2048);
        QVERIFY2(early > 1.5 * after, qPrintable(QStringLiteral("%1 %2").arg(early).arg(after)));
    }

    // Boom: a sub well under the kick, dropping in pitch as it goes.
    void boomIsALowDroppingSub()
    {
        const Render r = hit(34, {}, 1.0);
        const double start = frequency(r.left, 0.01, 0.06), later = frequency(r.left, 0.5, 0.8);
        QVERIFY2(later < 50.0 && later > 30.0, qPrintable(QString::number(later)));
        QVERIFY2(start > 1.5 * later, qPrintable(QStringLiteral("%1 %2").arg(start).arg(later)));
    }

    // Each new voice answers its notes.
    void newVoicesHaveNotes()
    {
        QCOMPARE(Engine::drumForNote(56), DrumId::Me);
        QCOMPARE(Engine::drumForNote(54), DrumId::Ck);
        QCOMPARE(Engine::drumForNote(52), DrumId::Nz);
        QCOMPARE(Engine::drumForNote(34), DrumId::Bm);
    }
};

QTEST_GUILESS_MAIN(TestCharacter)
#include "tst_character.moc"
