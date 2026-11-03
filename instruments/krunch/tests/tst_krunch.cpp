#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>

#include <cmath>
#include <complex>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "SyntheticGuitar.h"
#include "app/HeadFile.h"
#include "dsp/Gate.h"
#include "dsp/Krunch.h"
#include "dsp/Capture.h"
#include "dsp/DualHead.h"
#include "dsp/PowerAmp.h"
#include "dsp/Preamp.h"
#include "dsp/ToneStack.h"
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

// Total harmonic distortion of a sine through a preamp (harmonics 2..10 against the fundamental).
double thd(Preamp &preamp, double hz = 220.0, float amplitude = 0.2f)
{
    auto s = sine(9600, hz, amplitude);
    preamp.process(s.data(), std::uint32_t(s.size()));
    const std::size_t from = 4800, n = 4800;
    const auto power = [&](double f) {
        std::complex<double> sum = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            sum += double(s[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * f * double(i) / kRate);
        return std::norm(sum);
    };
    double harmonics = 0.0;
    for (int h = 2; h <= 10; ++h)
        harmonics += power(hz * h);
    return std::sqrt(harmonics / power(hz));
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
        QCOMPARE(int(kParams.size()), 7 + 4 * 13);
        QVERIFY(kParams.size() <= 64); // the "changed by audio" mask
        std::set<std::string_view> keys;
        for (const auto &p : kParams) {
            QVERIFY(keys.insert(p.key).second);
            QVERIFY(p.defaultValue >= 0.0f && p.defaultValue <= 1.0f);
        }
        QCOMPARE(kParams[std::size_t(channelParam(2, ChannelParam::Gain))].key, std::string_view("ch3_gain"));
        QCOMPARE(kParams[std::size_t(channelParam(0, ChannelParam::Loop))].key, std::string_view("ch1_loop"));
        QCOMPARE(kParams[std::size_t(channelParam(3, ChannelParam::Tube))].key, std::string_view("ch4_tube"));
        QCOMPARE(kParams[std::size_t(ParamId::PowerTube)].key, std::string_view("power_tube"));
        // The four channels start clean, crunch, heavy, lead.
        for (int c = 0; c < kChannels; ++c)
            QCOMPARE(int(units::voice(kParams[std::size_t(channelParam(c, ChannelParam::Voice))].defaultValue)), c);
    }

    void displayText()
    {
        QCOMPARE(format::input(1.0f), std::string("In 2"));
        QCOMPARE(format::channel(1.0f), std::string("Channel 4"));
        QCOMPARE(format::voice(2.0f / 3.0f), std::string("Heavy"));
        QCOMPARE(format::dial(0.65f), std::string("6.5"));
        QCOMPARE(format::level(60.0f / 72.0f), std::string("+0.0 dB"));
        QCOMPARE(format::level(0.0f), std::string("-inf dB"));
        QCOMPARE(format::gate(0.0f), std::string("Off"));
        QCOMPARE(format::gate(0.5f), std::string("-50 dB"));
        QCOMPARE(format::onOff(1.0f), std::string("On"));
    }

    // --- Tone stack ---------------------------------------------------------------

    void toneStackScoopsTheMids()
    {
        ToneStack t;
        t.set(kRate, ToneStack::kAmerican, 0.5, 0.5, 0.5);
        QVERIFY(db(t.magnitude(kRate, 700.0)) < db(t.magnitude(kRate, 100.0)) - 6.0);
        QVERIFY(db(t.magnitude(kRate, 700.0)) < db(t.magnitude(kRate, 5000.0)) - 4.0);
        for (double f : {30.0, 100.0, 1000.0, 10000.0, 20000.0}) // passive: never gains
            QVERIFY(t.magnitude(kRate, f) <= 1.0);
    }

    void toneStackControls()
    {
        ToneStack t;
        const auto at = [&](double bass, double mid, double treble, double f) {
            t.set(kRate, ToneStack::kBritish, bass, mid, treble);
            return db(t.magnitude(kRate, f));
        };
        QVERIFY(at(1, .5, .5, 60) > at(0, .5, .5, 60) + 6.0);
        QVERIFY(at(.5, 1, .5, 700) > at(.5, 0, .5, 700) + 6.0);
        QVERIFY(at(.5, .5, 1, 5000) > at(.5, .5, 0, 5000) + 6.0);
        // Treble barely moves the lows, bass barely the highs.
        QVERIFY(std::fabs(at(.5, .5, 1, 60) - at(.5, .5, 0, 60)) < 1.5);
        QVERIFY(std::fabs(at(1, .5, .5, 8000) - at(0, .5, .5, 8000)) < 1.5);
    }

    void toneStackIsStable()
    {
        ToneStack t;
        for (double b : {0.0, 1.0})
            for (double m : {0.0, 1.0})
                for (double tr : {0.0, 1.0}) {
                    t.set(kRate, ToneStack::kAmerican, b, m, tr);
                    t.reset();
                    float y = t.process(1.0f); // impulse
                    for (int n = 0; n < 48000; ++n)
                        y = t.process(0.0f);
                    QVERIFY(std::isfinite(y) && std::fabs(y) < 1e-6f);
                }
    }

    // --- Preamp -------------------------------------------------------------------

    void moreGainMoreDistortion()
    {
        Preamp low, high;
        low.prepare(kRate);
        high.prepare(kRate);
        Preamp::Settings s;
        s.voice = Voice::Crunch;
        s.gain = 0.1f;
        low.set(s);
        s.gain = 0.9f;
        high.set(s);
        QVERIFY(thd(high) > 2.0 * thd(low));
    }

    // Clean, Crunch, Heavy: more distorted each; Lead (smoother, for sustain) well past Crunch.
    void voicesFromCleanToLead()
    {
        std::vector<double> d;
        for (Voice v : {Voice::Clean, Voice::Crunch, Voice::Heavy, Voice::Lead}) {
            Preamp p;
            p.prepare(kRate);
            Preamp::Settings s;
            s.voice = v;
            s.gain = 0.5f;
            p.set(s);
            d.push_back(thd(p, 220.0, 0.05f));
        }
        QVERIFY2(d[0] < d[1] && d[1] < d[2], qPrintable(QStringLiteral("THD %1 %2 %3").arg(d[0]).arg(d[1]).arg(d[2])));
        QVERIFY2(d[3] > 1.5 * d[1], qPrintable(QStringLiteral("lead %1, crunch %2").arg(d[3]).arg(d[1])));
    }

    // The tubes answer the pick: played 12 dB softer, a driven channel is
    // quieter (Crunch by several dB; the old stages came out within half a
    // dB) and cleaner.
    void touchResponse()
    {
        for (Voice v : {Voice::Crunch, Voice::Heavy, Voice::Lead}) {
            Preamp::Settings s;
            s.voice = v;
            s.gain = 0.5f;
            const auto level = [&s](float amplitude) {
                Preamp p;
                p.prepare(kRate);
                p.set(s);
                auto x = sine(19200, 220.0, amplitude);
                p.process(x.data(), std::uint32_t(x.size()));
                return db(rms(x, 9600));
            };
            const auto distortion = [&s](float amplitude) {
                Preamp p;
                p.prepare(kRate);
                p.set(s);
                return thd(p, 220.0, amplitude);
            };
            // Crunch quieter by several dB; the high-gain voices hold their level
            // (as such amps do: they're saturated) without choking, and clean up.
            const double drop = level(0.2f) - level(0.05f);
            const double least = v == Voice::Crunch ? 2.5 : -0.5; // (and no choking: never much louder played softer)
            QVERIFY2(drop > least, qPrintable(QStringLiteral("voice %1: %2 dB quieter for 12 dB softer").arg(int(v)).arg(drop)));
            QVERIFY2(distortion(0.05f) < distortion(0.2f), qPrintable(QStringLiteral("voice %1: no cleaner played softer").arg(int(v))));
        }
    }

    // Each preamp tube changes the sound; a lower-gain tube distorts less.
    void tubesChangeTheSound()
    {
        std::vector<double> d;
        for (PreTube t : {PreTube::HighGain, PreTube::Smooth, PreTube::Medium, PreTube::LowGain}) {
            Preamp p;
            p.prepare(kRate);
            Preamp::Settings s;
            s.voice = Voice::Crunch;
            s.gain = 0.5f;
            s.tube = t;
            p.set(s);
            d.push_back(thd(p, 220.0, 0.05f));
        }
        QVERIFY2(d[3] < d[0], qPrintable(QStringLiteral("low gain %1, high gain %2").arg(d[3]).arg(d[0])));
        for (std::size_t i = 1; i < d.size(); ++i)
            QVERIFY(std::fabs(d[i] - d[0]) > 0.002);
    }

    // The resting bias is right whatever the tube and stage: silence in, silence out
    // (no thump settling at the start).
    void silenceIsSilentFromTheStart()
    {
        for (int v = 0; v < int(Voice::Count); ++v)
            for (int t = 0; t < int(PreTube::Count); ++t) {
                Preamp p;
                p.prepare(kRate);
                Preamp::Settings s;
                s.voice = Voice(v);
                s.tube = PreTube(t);
                s.gain = 1.0f;
                p.set(s);
                std::vector<float> x(4800, 0.0f);
                p.process(x.data(), std::uint32_t(x.size()));
                float peak = 0.0f;
                for (float y : x)
                    peak = std::max(peak, std::fabs(y));
                QVERIFY2(peak < 1e-4f, qPrintable(QStringLiteral("voice %1 tube %2: %3").arg(v).arg(t).arg(peak)));
            }
    }

    // The power tubes: each clips at its own level.
    void powerTubesDiffer()
    {
        std::vector<double> levels;
        for (int t = 0; t < int(PowerTube::Count); ++t) {
            PowerAmp p;
            p.prepare(kRate);
            p.set(0.9f, 0.0f, 0.0f, PowerTube(t));
            auto x = sine(9600, 220.0, 0.5f);
            p.process(x.data(), std::uint32_t(x.size()));
            levels.push_back(db(rms(x, 4800)));
        }
        // The big beam's headroom is the most, the small pentode's the least.
        QVERIFY2(levels[int(PowerTube::BigBeam)] > levels[int(PowerTube::SmallPentode)] + 3.0,
                 qPrintable(QStringLiteral("%1 vs %2 dB").arg(levels[int(PowerTube::BigBeam)]).arg(levels[int(PowerTube::SmallPentode)])));
    }

    void cleanAtLowGainIsClean()
    {
        Preamp p;
        p.prepare(kRate);
        Preamp::Settings s;
        s.voice = Voice::Clean;
        s.gain = 0.2f;
        p.set(s);
        QVERIFY(thd(p, 220.0, 0.05f) < 0.03);
    }

    // 4x oversampling keeps aliasing low with a high note into the most gain.
    void littleAliasing()
    {
        Preamp p;
        p.prepare(kRate);
        Preamp::Settings s;
        s.voice = Voice::Lead;
        s.gain = 1.0f;
        p.set(s);
        auto x = sine(12000, 1318.5, 0.3f); // the top E
        p.process(x.data(), std::uint32_t(x.size()));
        QVERIFY(inharmonicShare(x, 1318.5, 6000, 4096) < 0.01); // under -20 dB
    }

    // --- Power amp, gate ----------------------------------------------------------

    void sagCompresses()
    {
        const auto gainAt = [](float sag, float amplitude) {
            PowerAmp p;
            p.prepare(kRate);
            p.set(0.8f, sag, 0.0f);
            auto x = sine(9600, 220.0, amplitude);
            p.process(x.data(), std::uint32_t(x.size()));
            return rms(x, 4800) / amplitude;
        };
        // Dig in harder: the output grows less than the input, more so with sag.
        const double without = gainAt(0.0f, 0.5f) / gainAt(0.0f, 0.05f);
        const double with = gainAt(1.0f, 0.5f) / gainAt(1.0f, 0.05f);
        QVERIFY(without < 1.0);
        QVERIFY(with < without * 0.9);
    }

    void masterIsLouder()
    {
        double previous = 0.0;
        for (float master : {0.1f, 0.4f, 0.7f, 1.0f}) {
            PowerAmp p;
            p.prepare(kRate);
            p.set(master, 0.3f, 0.4f);
            auto x = sine(4800, 220.0, 0.1f);
            p.process(x.data(), std::uint32_t(x.size()));
            QVERIFY(rms(x, 2400) > previous);
            previous = rms(x, 2400);
        }
    }

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

    // The channels peak at a similar level (a clean sound can't match a
    // compressed one's average without slamming the power amp).
    void everyChannelPlaysAtASimilarLevel()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        std::vector<double> peaks;
        for (int c = 0; c < kChannels; ++c) {
            Krunch amp;
            amp.prepare(kRate, 128);
            setChannel(amp, c);
            const auto out = play(amp, di);
            QVERIFY(peak(out) < 1.0f);
            QVERIFY(db(rms(out)) > -34.0);
            peaks.push_back(db(peak(out)));
        }
        const auto [lo, hi] = std::minmax_element(peaks.begin(), peaks.end());
        QVERIFY2(*hi - *lo < 5.0, qPrintable(QStringLiteral("%1 .. %2 dB").arg(*lo).arg(*hi)));
    }

    // The clean channel stays clean when you dig in: hot guitar, sag up.
    void cleanChannelHasHeadroom()
    {
        Krunch amp;
        amp.prepare(kRate, 128);
        amp.setParam(int(ParamId::Sag), 0.7f);
        const auto out = play(amp, sine(48000, 196.0, 0.45f)); // a hot pickup through a healthy interface gain
        const std::size_t from = 24000, n = 12000;
        const auto power = [&](double f) {
            std::complex<double> sum = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                sum += double(out[from + i]) * std::polar(1.0, -2.0 * std::numbers::pi * f * double(i) / kRate);
            return std::norm(sum);
        };
        double harmonics = 0.0;
        for (int h = 2; h <= 11; ++h)
            harmonics += power(196.0 * h);
        const double thd = std::sqrt(harmonics / power(196.0));
        QVERIFY2(thd < 0.04, qPrintable(QStringLiteral("THD %1").arg(thd)));
    }

    void inputKnobChoosesTheInput()
    {
        Krunch amp;
        amp.prepare(kRate, 128);
        const auto note = sine(4800, 110.0, 0.2f);
        const std::vector<float> silence(4800, 0.0f);
        std::vector<float> left(4800), right(4800);
        float *const outputs[2] = {left.data(), right.data()};
        const float *const inputs[2] = {silence.data(), note.data()};
        krate::AudioBlock block{outputs, 4800};
        block.inputs = inputs;
        amp.process(block);
        QCOMPARE(peak(left), 0.0f);
        amp.setParam(int(ParamId::Input), 1.0f);
        amp.process(block);
        QVERIFY(peak(left) > 0.01f);
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
        amp.prepare(kRate, 128);
        amp.setParam(channelParam(0, ChannelParam::Volume), 0.0f);
        QVERIFY(peak(play(amp, note), 4800) < 1e-6f);
        Krunch amp2;
        amp2.prepare(kRate, 128);
        amp2.setParam(int(ParamId::Level), 0.0f);
        QVERIFY(peak(play(amp2, note), 4800) < 1e-6f);
    }

    // Switching channels crossfades: no jump bigger than the playing itself makes.
    void channelSwitchDoesNotClick()
    {
        Krunch amp;
        amp.prepare(kRate, 128);
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
            GainPedal a(front), b(loop);
            PedalList frontList, loopList;
            frontList.count = loopList.count = 1;
            frontList.pedals[0] = &a;
            loopList.pedals[0] = &b;
            amp.frontPedals().set(&frontList);
            amp.loopPedals().set(&loopList);
            amp.prepare(kRate, 128);
            amp.setParam(channelParam(0, ChannelParam::Loop), loopOn ? 1.0f : 0.0f);
            amp.setParam(channelParam(0, ChannelParam::Gain), 0.1f); // clean: levels pass through
            amp.setParam(int(ParamId::Master), 0.2f);
            return rms(play(amp, note), 4800);
        };
        const double base = levelWith(1.0f, 1.0f, true);
        QVERIFY(levelWith(0.5f, 1.0f, true) < base * 0.6);  // front pedals: always in
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
            DualHead dual;
            dual.setKnob(DualHead::Delay, delay);
            PedalList list;
            list.count = 1;
            list.pedals[0] = &dual;
            if (pedal)
                amp.frontPedals().set(&list);
            amp.prepare(kRate, 128);
            dual.prepare(kRate);
            amp.setParam(int(ParamId::Master), 0.2f);
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

    // An amp capture (a .nam model) plays in place of the preamp and power
    // amp: on its channel only, bounded, faster than real time; a file that
    // isn't a model is refused with a reason.
    void capturePlaysInPlaceOfTheAmp()
    {
        std::string error;
        const auto model = CaptureModel::load(NAM_EXAMPLES "/wavenet_a1_standard.nam", kRate, int(Krunch::kBlock), &error);
        QVERIFY2(model, error.c_str());
        QCOMPARE(model->sampleRate(), kRate);
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](bool capture, int channel) {
            Krunch amp;
            amp.prepare(kRate, 128);
            if (capture)
                amp.setCapture(2, model.get());
            setChannel(amp, channel);
            return play(amp, di);
        };
        const auto own = run(false, 2);
        QElapsedTimer timer;
        timer.start();
        const auto captured = run(true, 2);
        const double seconds = double(timer.nsecsElapsed()) * 1e-9;
        QVERIFY2(seconds < double(di.size()) / kRate, qPrintable(QStringLiteral("%1 s for %2 s").arg(seconds).arg(double(di.size()) / kRate)));
        double diff = 0.0;
        for (std::size_t i = 0; i < own.size(); ++i)
            diff += std::fabs(double(own[i]) - double(captured[i]));
        QVERIFY(diff > 1.0);                // it plays the capture, not Krunch's preamp
        QVERIFY(peak(captured) < 2.0f);     // bounded
        QVERIFY(db(rms(captured)) > -60.0); // and sounds
        for (float x : captured)
            QVERIFY(std::isfinite(x));
        // Another channel plays Krunch's own amp as before.
        const auto other = run(true, 1), otherOwn = run(false, 1);
        QCOMPARE(other, otherOwn);

        QTemporaryDir dir;
        const QString junk = dir.filePath("junk.nam");
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not a model");
        f.close();
        error.clear();
        QVERIFY(!CaptureModel::load(junk.toStdString(), kRate, 128, &error));
        QVERIFY(!error.empty());
    }

    // Amp heads (docs/HEADS.md): a channel built as a head. A built-in voice
    // made explicit (its tubes and power amp spelled out) plays exactly as the
    // voice does; another head plays differently, on its channel only; none
    // again: the voice. A head file keeps the design, and brings wild values in.
    void headsBuildTheChannel()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](const HeadDesign *head, int channel) {
            Krunch amp;
            amp.prepare(kRate, 128);
            amp.setParam(channelParam(2, ChannelParam::Tube), 1.0f / 3.0f);     // smooth
            amp.setParam(int(ParamId::PowerTube), 2.0f / 3.0f);                // small pentodes
            amp.setHead(2, head);
            setChannel(amp, channel);
            return play(amp, di);
        };
        const HeadDesign same = explicitHead(Voice::Heavy, PreTube::Smooth, PowerTube::SmallPentode);
        QCOMPARE(run(&same, 2), run(nullptr, 2)); // (channel 3's voice is Heavy)
        HeadDesign other = same;
        other.stage[0].cathodeOhms = 4000.0;
        other.power.headroom = 0.5;
        other.peakDb = 6.0;
        other.lowCutHz = 90.0;
        other.highCutHz = 5000.0;
        const auto changed = run(&other, 2), voice = run(nullptr, 2);
        double diff = 0.0;
        for (std::size_t i = 0; i < changed.size(); ++i)
            diff += std::fabs(double(changed[i]) - double(voice[i]));
        QVERIFY(diff > 1.0);
        QCOMPARE(run(&other, 1), run(nullptr, 1)); // another channel: its voice
        for (float x : changed)
            QVERIFY(std::isfinite(x));

        HeadFile file;
        file.name = QStringLiteral("Test head");
        file.design = other;
        file.design.channelTube = false;
        file.design.channelPower = false;
        const auto back = HeadFile::fromJson(file.toJson());
        QVERIFY(back);
        QCOMPARE(back->name, file.name);
        QCOMPARE(run(&back->design, 2), run(&file.design, 2)); // the same amp
        QJsonObject wild = file.toJson();
        QJsonObject power = wild.value(u"power").toObject();
        power[QStringLiteral("headroom")] = -5.0;
        power[QStringLiteral("high_cut_hz")] = 1e9;
        wild[QStringLiteral("power")] = power;
        const auto tamed = HeadFile::fromJson(wild);
        QVERIFY(tamed);
        QVERIFY(tamed->design.power.headroom > 0.0);
        QVERIFY(tamed->design.highCutHz <= 20000.0);
        QVERIFY(!HeadFile::fromJson(QJsonObject{{QStringLiteral("version"), 1}}));
    }

    // CABINET off: no IR and no speaker, the amp straight out (both sides);
    // at about the level the cabinet played at.
    void cabinetBypass()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        const auto run = [&](bool cabinet) {
            Krunch amp;
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
