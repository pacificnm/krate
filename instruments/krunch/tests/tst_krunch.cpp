#include <QtTest>

#include <cmath>
#include <complex>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include "SyntheticGuitar.h"
#include "dsp/Gate.h"
#include "dsp/Krunch.h"
#include "dsp/PowerAmp.h"
#include "dsp/Preamp.h"
#include "dsp/ToneStack.h"

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

// A pedal for the chains: multiplies by a factor.
class GainPedal final : public IPedal
{
public:
    explicit GainPedal(float g) : gain(g) {}
    void prepare(double) noexcept override {}
    void process(float *buffer, std::uint32_t frames) noexcept override
    {
        for (std::uint32_t n = 0; n < frames; ++n)
            buffer[n] *= gain;
    }
    float gain;
};

} // namespace

class TestKrunch : public QObject
{
    Q_OBJECT

private slots:
    // --- Parameters ---------------------------------------------------------------

    void parameterTable()
    {
        QCOMPARE(int(kParams.size()), 6 + 4 * 10);
        QVERIFY(kParams.size() <= 64); // the "changed by audio" mask
        std::set<std::string_view> keys;
        for (const auto &p : kParams) {
            QVERIFY(keys.insert(p.key).second);
            QVERIFY(p.defaultValue >= 0.0f && p.defaultValue <= 1.0f);
        }
        QCOMPARE(kParams[std::size_t(channelParam(2, ChannelParam::Gain))].key, std::string_view("ch3_gain"));
        QCOMPARE(kParams[std::size_t(channelParam(0, ChannelParam::Loop))].key, std::string_view("ch1_loop"));
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

    void voicesFromCleanToLead()
    {
        double previous = 0.0;
        for (Voice v : {Voice::Clean, Voice::Crunch, Voice::Heavy, Voice::Lead}) {
            Preamp p;
            p.prepare(kRate);
            Preamp::Settings s;
            s.voice = v;
            s.gain = 0.5f;
            p.set(s);
            const double d = thd(p, 220.0, 0.05f);
            QVERIFY2(d > previous, qPrintable(QStringLiteral("voice %1: THD %2").arg(int(v)).arg(d)));
            previous = d;
        }
    }

    // The lead voice is saturated: 12 dB more guitar is barely louder.
    void leadSaturates()
    {
        const auto level = [](float amplitude) {
            Preamp p;
            p.prepare(kRate);
            Preamp::Settings s;
            s.voice = Voice::Lead;
            s.gain = 0.5f;
            p.set(s);
            auto x = sine(9600, 220.0, amplitude);
            p.process(x.data(), std::uint32_t(x.size()));
            return db(rms(x, 4800));
        };
        QVERIFY(level(0.2f) - level(0.05f) < 2.0);
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

    void everyChannelPlaysAtASimilarLevel()
    {
        const auto di = tools::SyntheticGuitar(kRate).riff();
        std::vector<double> levels;
        for (int c = 0; c < kChannels; ++c) {
            Krunch amp;
            amp.prepare(kRate, 128);
            setChannel(amp, c);
            const auto out = play(amp, di);
            QVERIFY(peak(out) < 1.0f);
            levels.push_back(db(rms(out)));
        }
        const auto [lo, hi] = std::minmax_element(levels.begin(), levels.end());
        QVERIFY2(*hi - *lo < 4.0, qPrintable(QStringLiteral("%1 .. %2 dB").arg(*lo).arg(*hi)));
        QVERIFY(*lo > -28.0);
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
        QVERIFY2(biggest < 2.5f * steady, qPrintable(QStringLiteral("%1 vs %2").arg(biggest).arg(steady)));
    }

    void pedalsInFrontAndInTheLoop()
    {
        const auto note = sine(9600, 110.0, 0.05f);
        const auto levelWith = [&](float front, float loop, bool loopOn) {
            Krunch amp;
            GainPedal a(front), b(loop);
            amp.frontPedals().add(a);
            amp.loopPedals().add(b);
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
