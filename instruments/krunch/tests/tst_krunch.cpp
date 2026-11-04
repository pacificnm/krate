#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>

#include <array>
#include <cmath>
#include <memory>
#include <complex>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "SyntheticGuitar.h"
#include "dsp/Gate.h"
#include "dsp/Krunch.h"
#include "dsp/Capture.h"
#include "dsp/DualHead.h"
#include "dsp/Wav.h"

using namespace krunch;

namespace {

constexpr double kRate = 48000.0;

double db(double gain) { return 20.0 * std::log10(std::max(gain, 1e-12)); }

std::vector<float> sine(std::size_t frames, double hz, float amplitude)
{
    std::vector<float> s(frames);
    for (std::size_t i = 0; i < frames; ++i)
        s[i] = amplitude * float(std::sin(2.0 * std::numbers::pi * hz * double(i) / kRate));
    return s;
}

double rms(const std::vector<float> &s, std::size_t from = 0)
{
    double sum = 0.0;
    for (std::size_t i = from; i < s.size(); ++i)
        sum += double(s[i]) * s[i];
    return std::sqrt(sum / double(std::max<std::size_t>(1, s.size() - from)));
}

float peak(const std::vector<float> &s, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    float p = 0.0f;
    for (std::size_t i = from; i < std::min(to, s.size()); ++i)
        p = std::max(p, std::fabs(s[i]));
    return p;
}

// The amp, fed `in` (on input 1) in 128-frame blocks; returns the left output.
std::vector<float> play(Krunch &amp, const std::vector<float> &in, bool withInputs = true)
{
    std::vector<float> out(in.size()), right(in.size());
    for (std::size_t at = 0; at < in.size(); at += 128) {
        const auto frames = std::uint32_t(std::min<std::size_t>(128, in.size() - at));
        float *const outputs[2] = {out.data() + at, right.data() + at};
        const float *const inputs[2] = {in.data() + at, in.data() + at};
        krate::AudioBlock block{outputs, frames};
        block.inputs = withInputs ? inputs : nullptr;
        amp.process(block);
    }
    return out;
}

// Both sides of the output.
std::pair<std::vector<float>, std::vector<float>> playStereo(Krunch &amp, const std::vector<float> &in)
{
    std::vector<float> left(in.size()), right(in.size());
    for (std::size_t at = 0; at < in.size(); at += 128) {
        const auto frames = std::uint32_t(std::min<std::size_t>(128, in.size() - at));
        float *const outputs[2] = {left.data() + at, right.data() + at};
        const float *const inputs[2] = {in.data() + at, in.data() + at};
        krate::AudioBlock block{outputs, frames};
        block.inputs = inputs;
        amp.process(block);
    }
    return {left, right};
}

double energy(const std::vector<float> &s, std::size_t from, std::size_t to)
{
    double e = 0.0;
    for (std::size_t i = from; i < std::min(to, s.size()); ++i)
        e += double(s[i]) * double(s[i]);
    return e;
}

// A short burst at the start of `n` samples.
std::vector<float> burst(std::size_t n)
{
    std::vector<float> x(n, 0.0f);
    for (std::size_t i = 0; i < 240; ++i)
        x[i] = 0.1f * float(std::sin(2.0 * std::numbers::pi * 440.0 * double(i) / kRate));
    return x;
}

void setChannel(Krunch &amp, int channel) { amp.setParam(int(ParamId::Channel), float(channel) / float(kChannels - 1)); }

// An amp capture for the tests (the capture engine's own example model): a
// fresh one each time (a capture keeps its recent input: one per amp and channel).
std::unique_ptr<CaptureModel> testCapture()
{
    std::string error;
    auto model = CaptureModel::load(NAM_EXAMPLES "/wavenet_a1_standard.nam", kRate, int(Krunch::kBlock), &error);
    if (!model)
        qFatal("the test capture: %s", error.c_str());
    return model;
}

// An amp with a capture on every channel (kept with it).
struct Amped
{
    std::array<std::unique_ptr<CaptureModel>, kChannels> captures;
    explicit Amped(Krunch &amp)
    {
        for (int c = 0; c < kChannels; ++c) {
            captures[std::size_t(c)] = testCapture();
            amp.setCapture(c, captures[std::size_t(c)].get());
        }
    }
};

// Share of the signal's power away from the harmonics of `hz` (aliasing, noise),
// by a Hann-windowed DFT of `n` samples from `from`.
double inharmonicShare(const std::vector<float> &s, double hz, std::size_t from, std::size_t n)
{
    double harmonic = 0.0, other = 0.0;
    const double binHz = kRate / double(n);
    for (std::size_t k = 1; k < n / 2; ++k) {
        std::complex<double> sum = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(n - 1));
            sum += w * double(s[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * double(k * i) / double(n));
        }
        const double f = double(k) * binHz;
        const double nearest = std::round(f / hz) * hz;
        (std::fabs(f - nearest) <= 3.0 * binHz && nearest > 0.0 ? harmonic : other) += std::norm(sum);
    }
    return other / (harmonic + other);
}

// A pedal for the chains: multiplies by its one knob (0..1 -> 0..2).
std::string gainText(float v) { return std::to_string(v); }
const KnobInfo kGainKnobs[] = {{"gain", "GAIN", 0.5f, &gainText}};
const PedalInfo kGainInfo{"test_gain", "Gain", "#000000", kGainKnobs, false};

class GainPedal final : public Pedal
{
public:
    explicit GainPedal(float g) : Pedal(kGainInfo) { setKnob(0, g / 2.0f); }

protected:
    void prepareEffect(double) noexcept override {}
    void resetEffect() noexcept override {}
    void processEffect(float *buffer, std::uint32_t frames) noexcept override
    {
        for (std::uint32_t n = 0; n < frames; ++n)
            buffer[n] *= 2.0f * knob(0);
    }
};

} // namespace

class TestKrunch : public QObject
{
    Q_OBJECT

private slots:
    // --- Parameters ---------------------------------------------------------------

    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), 3 + 4 * 11);
        QVERIFY(kParams.size() <= 64); // the "changed by audio" mask
        std::set<std::string_view> keys;
        for (const auto &p : kParams) {
            QVERIFY(keys.insert(p.key).second);
            QVERIFY(p.defaultValue >= 0.0f && p.defaultValue <= 1.0f);
        }
        QCOMPARE(kParams[std::size_t(channelParam(2, ChannelParam::Gain))].key, std::string_view("ch3_gain"));
        QCOMPARE(kParams[std::size_t(channelParam(0, ChannelParam::Loop))].key, std::string_view("ch1_loop"));
        QCOMPARE(kParams[std::size_t(channelParam(3, ChannelParam::Cabinet))].key, std::string_view("ch4_cabinet"));
        QCOMPARE(kParams[std::size_t(ParamId::Level)].key, std::string_view("level"));
        // Every channel starts with its capture as captured: GAIN and the EQ at noon.
        for (int c = 0; c < kChannels; ++c)
            for (ChannelParam p : {ChannelParam::Gain, ChannelParam::Bass, ChannelParam::Mid, ChannelParam::Treble, ChannelParam::Presence})
                QCOMPARE(kParams[std::size_t(channelParam(c, p))].defaultValue, 0.5f);
    }

    void displayText()
    {
        QCOMPARE(format::input(1.0f), std::string("In 2"));
        QCOMPARE(format::channel(1.0f), std::string("Channel 4"));
        QCOMPARE(format::gain(0.5f), std::string("+0.0 dB"));
        QCOMPARE(format::gain(1.0f), std::string("+12.0 dB"));
        QCOMPARE(format::mid(0.0f), std::string("-10.0 dB"));
        QCOMPARE(format::presence(1.0f), std::string("+8.0 dB"));
        QCOMPARE(format::dial(0.65f), std::string("6.5"));
        QCOMPARE(format::level(60.0f / 72.0f), std::string("+0.0 dB"));
        QCOMPARE(format::level(0.0f), std::string("-inf dB"));
        QCOMPARE(format::gate(0.0f), std::string("Off"));
        QCOMPARE(format::gate(0.5f), std::string("-50 dB"));
        QCOMPARE(format::onOff(1.0f), std::string("On"));
    }

    // --- The gate -------------------------------------------------------------------

    void gateShutsTheNoiseOut()
    {
        Gate gate;
        gate.prepare(kRate);
        std::vector<float> hiss(9600);
        for (std::size_t i = 0; i < hiss.size(); ++i)
            hiss[i] = (i % 2 ? 1.0f : -1.0f) * 0.0003f; // about -70 dBFS
        auto off = hiss;
        gate.process(off.data(), std::uint32_t(off.size()), 0.0f);
        QCOMPARE(off, hiss); // off: untouched
        auto gated = hiss;
        gate.process(gated.data(), std::uint32_t(gated.size()), 0.5f); // -50 dB
        QVERIFY(peak(gated, 8000) == 0.0f);
        QVERIFY(gate.closed());
        auto note = sine(4800, 110.0, 0.2f);
        gate.process(note.data(), std::uint32_t(note.size()), 0.5f);
        QVERIFY(peak(note, 100) > 0.19f); // opens within a fraction of a millisecond
        QVERIFY(!gate.closed());
    }

    // --- The amp ------------------------------------------------------------------

    void playsTheInterfaceInputs()
    {
        Krunch amp;
        QCOMPARE(amp.inputChannels(), 2);
        QCOMPARE(amp.outputChannels(), 2);
    }

    void inputKnobChoosesTheInput()
    {
        Krunch amp;
        Amped amped(amp);
        amp.prepare(kRate, 128);
        const auto note = sine(4800, 110.0, 0.2f);
        const std::vector<float> silence(4800, 0.0f);
        std::vector<float> left(4800), right(4800);
        float *const outputs[2] = {left.data(), right.data()};
        const float *const inputs[2] = {silence.data(), note.data()};
        krate::AudioBlock block{outputs, 4800};
        block.inputs = inputs;
        amp.process(block);
        const float quiet = peak(left); // (a capture hums a little with nothing in)
        QVERIFY(quiet < 0.002f);
        amp.setParam(int(ParamId::Input), 1.0f);
        amp.process(block);
        QVERIFY(peak(left) > 10.0f * quiet);
        QCOMPARE(left, right); // mono, both sides
    }

    void noInputsIsSilence()
    {
        Krunch amp;
        amp.prepare(kRate, 128);
        const auto out = play(amp, sine(4800, 110.0, 0.2f), false);
        QCOMPARE(peak(out), 0.0f);
    }

    void volumeAndLevelBottomOut()
    {
        const auto note = sine(9600, 110.0, 0.2f);
        Krunch amp;
        Amped amped(amp);
        amp.prepare(kRate, 128);
        QVERIFY(peak(play(amp, note), 4800) > 1e-3f); // it sounds
        amp.setParam(channelParam(0, ChannelParam::Volume), 0.0f);
        QVERIFY(peak(play(amp, note), 4800) < 1e-6f);
        Krunch amp2;
        Amped amped2(amp2);
        amp2.prepare(kRate, 128);
        amp2.setParam(int(ParamId::Level), 0.0f);
        QVERIFY(peak(play(amp2, note), 4800) < 1e-6f);
    }

    // Switching channels crossfades: no jump bigger than the playing itself makes.
    void channelSwitchDoesNotClick()
    {
        Krunch amp;
        Amped amped(amp);
        amp.prepare(kRate, 128);
        amp.setParam(channelParam(2, ChannelParam::Gain), 0.9f); // another sound there
        const auto note = sine(48000, 82.41, 0.2f);
        auto first = play(amp, std::vector<float>(note.begin(), note.begin() + 24000));
        float steady = 0.0f;
        for (std::size_t i = 12001; i < first.size(); ++i)
            steady = std::max(steady, std::fabs(first[i] - first[i - 1]));
        setChannel(amp, 2);
        const auto second = play(amp, std::vector<float>(note.begin() + 24000, note.end()));
        float biggest = std::fabs(second[0] - first.back());
        for (std::size_t i = 1; i < 2400; ++i)
            biggest = std::max(biggest, std::fabs(second[i] - second[i - 1]));
        for (std::size_t i = 12001; i < second.size(); ++i) // the new channel's own playing, settled
            steady = std::max(steady, std::fabs(second[i] - second[i - 1]));
        QVERIFY2(biggest < 2.5f * steady, qPrintable(QStringLiteral("%1 vs %2").arg(biggest).arg(steady)));
    }

    void pedalsInFrontAndInTheLoop()
    {
        const auto note = sine(9600, 110.0, 0.05f);
        const auto levelWith = [&](float front, float loop, bool loopOn) {
            Krunch amp;
            Amped amped(amp);
            GainPedal a(front), b(loop);
            PedalList frontList, loopList;
            frontList.count = loopList.count = 1;
            frontList.pedals[0] = &a;
            loopList.pedals[0] = &b;
            amp.frontPedals().set(&frontList);
            amp.loopPedals().set(&loopList);
            amp.prepare(kRate, 128);
            amp.setParam(channelParam(0, ChannelParam::Loop), loopOn ? 1.0f : 0.0f);
            amp.setParam(channelParam(0, ChannelParam::Gain), 0.1f); // driven gently: levels pass through
            return rms(play(amp, note), 4800);
        };
        const double base = levelWith(1.0f, 1.0f, true);
        QVERIFY(levelWith(0.0f, 1.0f, true) < base * 0.1);  // front pedals: always in (into the capture)
        QVERIFY(levelWith(1.0f, 0.5f, true) < base * 0.6);  // loop pedals: with the loop on
        QVERIFY(std::fabs(levelWith(1.0f, 0.5f, false) - base) < base * 0.01); // loop off: bypassed
    }

    // SPACE: with both IRs, A to the left and B to the right as it turns up.
    // (B here is A 1000 samples later, so where each one lands shows.)
    void spaceSpreadsTheCabinets()
    {
        std::vector<float> a(2048, 0.0f), b(2048, 0.0f);
        a[0] = 1.0f;
        b[1000] = 1.0f;
        krate::WavData wa, wb;
        wa.rate = wb.rate = int(kRate);
        wa.channels = wb.channels = 1;
        wa.left = a;
        wb.left = b;
        const auto kernelA = IrKernel::build(wa, kRate), kernelB = IrKernel::build(wb, kRate);
        QVERIFY(kernelA && kernelB);
        // The amp with A, B or both loaded, SPACE as given.
        const auto sides = [&](bool withA, bool withB, float space) {
            Krunch amp;
            Amped amped(amp);
            amp.prepare(kRate, 128);
            if (withA)
                amp.cabinet().setKernel(Krunch::cabinetSlot(0, false), kernelA.get());
            if (withB)
                amp.cabinet().setKernel(Krunch::cabinetSlot(0, true), kernelB.get());
            amp.setParam(channelParam(0, ChannelParam::Space), space);
            return playStereo(amp, burst(9600));
        };
        const auto apart = [](const std::vector<float> &x, const std::vector<float> &y, float scale) {
            double d = 0.0, e = 0.0;
            for (std::size_t i = 0; i < x.size(); ++i) {
                d += std::pow(double(x[i]) - scale * double(y[i]), 2.0);
                e += std::pow(scale * double(y[i]), 2.0);
            }
            return d / e;
        };
        const auto [onlyA, onlyAr] = sides(true, false, 1.0f); // one IR: SPACE does nothing
        QCOMPARE(apart(onlyA, onlyAr, 1.0f), 0.0);
        const auto [onlyB, onlyBr] = sides(false, true, 0.0f);
        const auto [l0, r0] = sides(true, true, 0.0f);
        QCOMPARE(apart(l0, r0, 1.0f), 0.0); // both on both sides
        // At 10: the left is A alone, the right B alone (each at the mix's half).
        const auto [l1, r1] = sides(true, true, 1.0f);
        QVERIFY2(apart(l1, onlyA, 0.5f) < 1e-8, qPrintable(QStringLiteral("left %1").arg(apart(l1, onlyA, 0.5f))));
        QVERIFY2(apart(r1, onlyB, 0.5f) < 1e-8, qPrintable(QStringLiteral("right %1").arg(apart(r1, onlyB, 0.5f))));
        // Half way: the left is A and half of B.
        const auto [lh, rh] = sides(true, true, 0.5f);
        std::vector<float> expected(lh.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            expected[i] = 0.5f * onlyA[i] + 0.25f * onlyB[i];
        QVERIFY2(apart(lh, expected, 1.0f) < 1e-8, qPrintable(QStringLiteral("half %1").arg(apart(lh, expected, 1.0f))));
    }

    // Dual Head: the right side is the left, DELAY later (the second amp).
    void dualHeadSetsASecondAmpApart()
    {
        const auto sides = [](bool pedal, float delay) {
            Krunch amp;
            Amped amped(amp);
            DualHead dual;
            dual.setKnob(DualHead::Delay, delay);
            PedalList list;
            list.count = 1;
            list.pedals[0] = &dual;
            if (pedal)
                amp.frontPedals().set(&list);
            amp.prepare(kRate, 128);
            dual.prepare(kRate);
            return playStereo(amp, burst(19200));
        };
        const auto lagOf = [](const std::vector<float> &l, const std::vector<float> &r) {
            int best = 0;
            double top = -1.0;
            for (int lag = 0; lag < 2400; ++lag) {
                double c = 0.0;
                for (std::size_t i = 0; i + std::size_t(lag) < 9600; ++i)
                    c += double(l[i]) * double(r[i + std::size_t(lag)]);
                if (c > top) {
                    top = c;
                    best = lag;
                }
            }
            return best;
        };
        const auto [l0, r0] = sides(false, 0.25f);
        QCOMPARE(lagOf(l0, r0), 0); // no pedal: the same on both sides
        QCOMPARE(energy(l0, 0, 9600), energy(r0, 0, 9600));
        const auto [l1, r1] = sides(true, 0.25f); // 10 ms
        QVERIFY2(std::abs(lagOf(l1, r1) - 480) <= 2, qPrintable(QStringLiteral("lag %1").arg(lagOf(l1, r1))));
        QCOMPARE(DualHead::delayMs(0.25f), 10.0);
    }

    // A channel plays its amp capture (a .nam model): bounded, faster than
    // real time; a channel without one is silent; a file that isn't a model
    // is refused with a reason.
    void capturesPlayOnTheirChannels()
    {
        const auto model = testCapture();
        QCOMPARE(model->sampleRate(), kRate);
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](int channel) {
            Krunch amp;
            amp.prepare(kRate, 128);
            amp.setCapture(2, model.get());
            setChannel(amp, channel);
            return play(amp, di);
        };
        QElapsedTimer timer;
        timer.start();
        const auto captured = run(2);
        const double seconds = double(timer.nsecsElapsed()) * 1e-9;
        QVERIFY2(seconds < double(di.size()) / kRate, qPrintable(QStringLiteral("%1 s for %2 s").arg(seconds).arg(double(di.size()) / kRate)));
        QVERIFY(peak(captured) < 2.0f);     // bounded
        QVERIFY(db(rms(captured)) > -60.0); // and sounds
        for (float x : captured)
            QVERIFY(std::isfinite(x));
        QCOMPARE(peak(run(1)), 0.0f); // no amp on that channel: silent

        QTemporaryDir dir;
        const QString junk = dir.filePath("junk.nam");
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not a model");
        f.close();
        std::string error;
        QVERIFY(!CaptureModel::load(junk.toStdString(), kRate, 128, &error));
        QVERIFY(!error.empty());
    }

    // GAIN drives the capture (noon as captured), the EQ shapes what comes out.
    void gainAndEqShapeTheCapture()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](float gain, float bass, float treble) {
            Krunch amp;
            Amped amped(amp);
            amp.prepare(kRate, 128);
            amp.setParam(channelParam(0, ChannelParam::Gain), gain);
            amp.setParam(channelParam(0, ChannelParam::Bass), bass);
            amp.setParam(channelParam(0, ChannelParam::Treble), treble);
            amp.setParam(channelParam(0, ChannelParam::Cabinet), 0.0f);
            return play(amp, di);
        };
        const auto noon = run(0.5f, 0.5f, 0.5f);
        // Driven harder or gentler: not as noon (the test capture is a toy model:
        // how its level follows isn't an amp's, only that it changes).
        const auto differs = [&noon](const std::vector<float> &x) {
            double apart = 0.0;
            for (std::size_t i = 0; i < x.size(); ++i)
                apart += std::fabs(double(x[i]) - double(noon[i]));
            return apart > 1.0;
        };
        QVERIFY(differs(run(1.0f, 0.5f, 0.5f)));
        QVERIFY(differs(run(0.0f, 0.5f, 0.5f)));
        // The top end: a first difference against the whole.
        const auto highs = [](const std::vector<float> &x) {
            double e = 0.0, z = 0.0;
            for (std::size_t i = 1; i < x.size(); ++i) {
                e += std::pow(double(x[i]) - double(x[i - 1]), 2.0);
                z += double(x[i]) * double(x[i]);
            }
            return e / z;
        };
        QVERIFY(highs(run(0.5f, 0.5f, 1.0f)) > 1.5 * highs(noon)); // TREBLE up
        QVERIFY(highs(run(0.5f, 1.0f, 0.5f)) < highs(noon));       // BASS up: more lows
    }

    // CABINET off: no IR and no speaker, the amp straight out (both sides);
    // at about the level the cabinet played at.
    void cabinetBypass()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](bool cabinet) {
            Krunch amp;
            Amped amped(amp);
            amp.prepare(kRate, 128);
            setChannel(amp, 1);
            amp.setParam(channelParam(1, ChannelParam::Cabinet), cabinet ? 1.0f : 0.0f);
            return playStereo(amp, di);
        };
        const auto [on, onR] = run(true);
        const auto [off, offR] = run(false);
        QCOMPARE(off, offR); // mono, on both sides
        // The speaker's top cut is gone: far more above 6 kHz without it.
        const auto highs = [](const std::vector<float> &s) {
            double e = 0.0, z = 0.0;
            for (std::size_t i = 1; i < s.size(); ++i) { // a first difference: the top end, roughly
                const double d = double(s[i]) - double(s[i - 1]);
                e += d * d;
                z += double(s[i]) * double(s[i]);
            }
            return e / z;
        };
        QVERIFY2(highs(off) > 1.3 * highs(on), qPrintable(QStringLiteral("%1 vs %2").arg(highs(off)).arg(highs(on))));
        QVERIFY2(std::fabs(db(rms(off)) - db(rms(on))) < 6.0, qPrintable(QStringLiteral("%1 vs %2 dB").arg(db(rms(off))).arg(db(rms(on)))));
        QVERIFY(peak(off) < 1.0f);
    }

    void peaksForTheMeters()
    {
        Krunch amp;
        Amped amped(amp);
        amp.prepare(kRate, 128);
        auto in = std::vector<float>(1024, 0.0f);
        in[100] = -0.8f;
        play(amp, in);
        QCOMPARE(amp.takeInputPeak(), 0.8f);
        QVERIFY(amp.takeOutputPeak() > 0.0f);
        QCOMPARE(amp.takeInputPeak(), 0.0f); // taken
    }

    void noDenormalsOrNaNsFromSilence()
    {
        Krunch amp;
        Amped amped(amp);
        amp.prepare(kRate, 128);
        setChannel(amp, 3);
        auto in = sine(4800, 110.0, 0.5f);
        in.resize(48000 * 2, 0.0f);
        const auto out = play(amp, in);
        for (float s : out)
            QVERIFY(std::isfinite(s));
        QVERIFY(peak(out, 48000 * 2 - 4800) < 1e-4f); // the gate and the tails have it quiet again
    }
};

QTEST_APPLESS_MAIN(TestKrunch)
#include "tst_krunch.moc"
