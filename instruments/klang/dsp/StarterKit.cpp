#include "klang/dsp/StarterKit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "klang/dsp/Engine.h"

namespace klang {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

class Noise
{
public:
    explicit Noise(std::uint32_t seed) : m_state(seed) {}
    double next()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return double(std::int32_t(m_state)) / 2147483648.0;
    }

private:
    std::uint32_t m_state;
};

// A two-pole resonator (a formant, a ringing metal mode).
class Resonator
{
public:
    Resonator(double hz, double bandwidth, double rate)
    {
        const double r = std::exp(-std::numbers::pi * bandwidth / rate);
        m_a1 = 2.0 * r * std::cos(kTwoPi * hz / rate);
        m_a2 = -r * r;
        m_gain = 1.0 - r;
    }
    double process(double x)
    {
        const double y = m_gain * x + m_a1 * m_y1 + m_a2 * m_y2;
        m_y2 = m_y1;
        m_y1 = y;
        return y;
    }

private:
    double m_a1 = 0.0, m_a2 = 0.0, m_gain = 1.0, m_y1 = 0.0, m_y2 = 0.0;
};

double saw(double phase) { return 2.0 * (phase - std::floor(phase + 0.5)); }

std::vector<float> render(double seconds, double rate, auto &&f)
{
    std::vector<float> x(std::size_t(seconds * rate));
    for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = float(std::clamp(f(double(i) / rate), -1.0, 1.0));
    // A 2 ms fade at the end, so a one shot doesn't click.
    const std::size_t fade = std::min(x.size(), std::size_t(0.002 * rate));
    for (std::size_t k = 0; k < fade; ++k)
        x[x.size() - 1 - k] *= float(k) / float(fade);
    return x;
}

// Each sound as a function of time; all deterministic.
std::vector<float> kick(double rate)
{
    return render(0.7, rate, [](double t) {
        const double phase = kTwoPi * (45.0 * t + 140.0 * (1.0 - std::exp(-t * 35.0)) / 35.0);
        return std::tanh(1.6 * std::sin(phase) * std::exp(-t * 6.0)) + 0.4 * std::exp(-t * 400.0);
    });
}

std::vector<float> snare(double rate)
{
    Noise n(11);
    double lp = 0.0;
    return render(0.4, rate, [&](double t) {
        lp += (n.next() - lp) * 0.6;
        return 0.6 * lp * std::exp(-t * 14.0) + 0.5 * std::sin(kTwoPi * 185.0 * t) * std::exp(-t * 30.0);
    });
}

std::vector<float> hat(double rate, double decay, std::uint32_t seed)
{
    Noise n(seed);
    double last = 0.0;
    return render(decay * 2.5, rate, [&](double t) {
        const double x = n.next();
        const double high = x - last; // a cheap high-pass
        last = x;
        return 0.6 * high * std::exp(-t / decay * 2.3);
    });
}

std::vector<float> metal(double rate, std::initializer_list<double> modes, double decay, double level)
{
    std::vector<Resonator> bank;
    for (double hz : modes)
        bank.emplace_back(hz, hz * 0.004, rate);
    Noise n(23);
    return render(decay * 3.0, rate, [&](double t) {
        const double strike = t < 0.004 ? n.next() : 0.0;
        double y = 0.0;
        for (Resonator &r : bank)
            y += r.process(strike * 40.0);
        return level * std::tanh(y * std::exp(-t / decay));
    });
}

std::vector<float> zap(double rate)
{
    Noise n(31);
    double phase = 0.0;
    return render(0.5, rate, [&](double t) {
        phase += (90.0 + 2400.0 * std::exp(-t * 18.0)) / rate;
        return (0.7 * std::sin(kTwoPi * phase) + 0.15 * n.next()) * std::exp(-t * 7.0);
    });
}

std::vector<float> boom(double rate)
{
    return render(2.0, rate, [](double t) {
        const double phase = kTwoPi * (38.0 * t + 30.0 * (1.0 - std::exp(-t * 8.0)) / 8.0);
        return std::tanh(2.0 * std::sin(phase)) * 0.8 * std::exp(-t * 1.6);
    });
}

// One bar at 120 BPM (2 s): kick, snare, hats and a clank, a little driven: made to be sliced.
std::vector<float> breakLoop(double rate, const std::vector<float> &k, const std::vector<float> &s,
                             const std::vector<float> &h, const std::vector<float> &c)
{
    std::vector<float> x(std::size_t(2.0 * rate), 0.0f);
    const auto add = [&](const std::vector<float> &sound, int sixteenth, float gain) {
        const auto at = std::size_t(sixteenth * 0.125 * rate);
        for (std::size_t i = 0; i < sound.size() && at + i < x.size(); ++i)
            x[at + i] += gain * sound[i];
    };
    for (int step : {0, 6, 10})
        add(k, step, 0.9f);
    for (int step : {4, 12, 15})
        add(s, step, step == 15 ? 0.4f : 0.8f);
    for (int step = 0; step < 16; step += 2)
        add(h, step, step % 4 == 2 ? 0.6f : 0.35f);
    add(c, 7, 0.5f);
    for (float &v : x)
        v = float(std::tanh(1.4 * v));
    return x;
}

// Two detuned saws and a fifth, slowly darkening and opening: 4 s (2 bars at 120 BPM), loopable.
std::vector<float> drone(double rate)
{
    double lp = 0.0;
    return render(4.0, rate, [&](double t) {
        const double y = saw(55.0 * t) + saw(55.4 * t) + 0.7 * saw(82.41 * t) + 0.5 * saw(110.2 * t);
        const double sweep = 0.02 + 0.06 * (0.5 - 0.5 * std::cos(kTwoPi * t / 4.0));
        lp += (y - lp) * sweep;
        return 0.3 * lp;
    });
}

// A buzz through three formants: a sung vowel, without words.
std::vector<float> vowel(double rate, double pitch, std::array<double, 3> formants)
{
    std::array<Resonator, 3> f{Resonator(formants[0], 80.0, rate), Resonator(formants[1], 100.0, rate),
                               Resonator(formants[2], 140.0, rate)};
    return render(2.0, rate, [&](double t) {
        const double vibrato = 1.0 + 0.01 * std::sin(kTwoPi * 5.0 * t);
        const double buzz = saw(pitch * vibrato * t);
        const double y = f[0].process(buzz) + 0.7 * f[1].process(buzz) + 0.4 * f[2].process(buzz);
        const double env = std::min(1.0, t * 8.0) * std::min(1.0, (2.0 - t) * 4.0);
        return 3.0 * y * env;
    });
}

std::vector<float> swell(double rate)
{
    Noise n(47);
    double lp = 0.0;
    return render(2.0, rate, [&](double t) {
        lp += (n.next() - lp) * (0.02 + 0.3 * t / 2.0);
        const double tone = std::sin(kTwoPi * 220.0 * t) + 0.5 * std::sin(kTwoPi * 330.0 * t);
        const double rise = std::pow(t / 2.0, 3.0);
        return rise * (0.7 * lp * 3.0 + 0.3 * tone);
    });
}

// A minor chord of detuned saws, short: a trance stab.
std::vector<float> stab(double rate)
{
    double lp = 0.0;
    return render(0.6, rate, [&](double t) {
        double y = 0.0;
        for (double hz : {220.0, 261.63, 329.63})
            y += saw(hz * t) + saw(hz * 1.006 * t);
        lp += (y - lp) * (0.05 + 0.5 * std::exp(-t * 12.0));
        return 0.18 * lp * std::exp(-t * 4.0);
    });
}

std::vector<float> pluck(double rate)
{
    double lp = 0.0;
    return render(0.8, rate, [&](double t) {
        lp += (saw(65.41 * t) - lp) * (0.01 + 0.4 * std::exp(-t * 20.0)); // C2
        return 0.8 * std::tanh(2.0 * lp) * std::exp(-t * 3.0);
    });
}

std::vector<float> glitch(double rate)
{
    Noise n(59);
    double value = 0.0;
    int hold = 0;
    return render(0.6, rate, [&](double t) {
        if (--hold <= 0) {
            value = n.next() > 0.0 ? 0.6 : -0.6;
            hold = 20 + int(std::fabs(n.next()) * 200.0);
        }
        const double gate = std::fmod(t, 0.075) < 0.04 ? 1.0 : 0.0;
        return value * gate * std::exp(-t * 2.0);
    });
}

Sample mono(std::vector<float> x, double rate, const std::string &name)
{
    return Sample::fromChannels(std::move(x), {}, rate, name);
}

} // namespace

std::vector<StarterSound> starterSounds(double rate)
{
    const auto k = kick(rate), s = snare(rate), h = hat(rate, 0.04, 3), c = metal(rate, {317.0, 523.0, 811.0, 1297.0}, 0.25, 0.8);
    std::vector<StarterSound> sounds;
    const auto add = [&](const char *file, std::vector<float> x) { sounds.push_back({file, mono(std::move(x), rate, file)}); };
    add("01 kick.wav", k);
    add("02 snare.wav", s);
    add("03 closed hat.wav", h);
    add("04 open hat.wav", hat(rate, 0.25, 5));
    add("05 clank.wav", c);
    add("06 pipe.wav", metal(rate, {587.0, 1621.0, 3119.0, 4703.0}, 0.6, 0.6));
    add("07 zap.wav", zap(rate));
    add("08 boom.wav", boom(rate));
    add("09 break 120.wav", breakLoop(rate, k, s, h, c));
    add("10 drone 120.wav", drone(rate));
    add("11 voice ah.wav", vowel(rate, 110.0, {730.0, 1090.0, 2440.0}));
    add("12 voice oo.wav", vowel(rate, 98.0, {300.0, 870.0, 2240.0}));
    add("13 swell.wav", swell(rate));
    add("14 stab.wav", stab(rate));
    add("15 pluck C2.wav", pluck(rate));
    add("16 glitch.wav", glitch(rate));
    return sounds;
}

void applyStarterPad(Engine &e, int pad)
{
    namespace p = positions;
    const auto set = [&](ParamId id, float v) { e.setPadParam(pad, id, v); };
    switch (pad) {
    case 1: set(ParamId::ReverbSend, 0.2f); break;                                   // snare
    case 2: set(ParamId::Choke, position(1, p::kChoke)); set(ParamId::PadLevel, 0.65f); break; // closed hat
    case 3: set(ParamId::Choke, position(1, p::kChoke)); set(ParamId::PadLevel, 0.6f); break;  // open hat
    case 4: set(ParamId::DelaySend, 0.35f); set(ParamId::PadPan, 0.35f); break;       // clank
    case 5: set(ParamId::Ring, 0.4f); set(ParamId::RingFreq, 0.55f); set(ParamId::PadPan, 0.65f); break; // pipe
    case 6: set(ParamId::ReverbSend, 0.3f); break;                                    // zap
    case 8: // the break: a one-bar loop, locked to the tempo
        set(ParamId::Mode, position(2, p::kMode));
        set(ParamId::Sync, position(3, p::kSync)); // 4 beats
        break;
    case 9: // the drone: two bars, gated and filtered
        set(ParamId::Mode, position(2, p::kMode));
        set(ParamId::Sync, position(4, p::kSync)); // 8 beats
        set(ParamId::Gate, 1.0f);
        set(ParamId::Filter, position(1, p::kFilter));
        set(ParamId::Cutoff, 0.55f);
        set(ParamId::Res, 0.5f);
        set(ParamId::Drive, 0.3f);
        set(ParamId::PadLevel, 0.7f);
        break;
    case 10: set(ParamId::DelaySend, 0.4f); set(ParamId::ReverbSend, 0.3f); break;   // voice ah
    case 11: set(ParamId::Bits, position(10, p::kBits)); set(ParamId::Downsample, position(2, p::kDownsample)); break; // voice oo, crushed
    case 12: set(ParamId::ReverbSend, 0.4f); break;                                   // swell
    case 13: set(ParamId::Gate, 1.0f); set(ParamId::ReverbSend, 0.3f); set(ParamId::DelaySend, 0.2f); break; // stab
    case 15: set(ParamId::Downsample, position(3, p::kDownsample)); set(ParamId::DelaySend, 0.3f); break; // glitch
    default: break;
    }
}

Pattern starterPattern(int index)
{
    Pattern pattern;
    const auto on = [&](int pad, std::initializer_list<int> steps) {
        for (int s : steps)
            pattern.steps[std::size_t(pad)][std::size_t(s)].on = true;
    };
    const auto step = [&](int pad, int s) -> Step & { return pattern.steps[std::size_t(pad)][std::size_t(s)]; };
    if (index == 0) { // A: an industrial beat, the drone gated, stabs
        on(0, {0, 4, 8, 11, 12});
        step(0, 0).accent = true;
        on(1, {4, 12});
        step(1, 12).accent = true;
        on(2, {2, 6, 10, 14});
        step(2, 14).repeat = 3;
        on(3, {15});
        on(4, {3, 11});
        step(4, 11).reverse = true;
        on(6, {7});
        step(6, 7).pitch = -5;
        on(9, {0});
        on(13, {0, 3, 6, 10});
        step(13, 6).pitch = 3;
        step(13, 10).pitch = -2;
        step(13, 0).accent = true;
        pattern.gate = 0x6DB6; // off, on, on, off, on, on ...: a 3-against-4 chop
    } else if (index == 1) { // B: the break looped, the voices over it
        on(8, {0});
        on(0, {0, 8});
        on(10, {6});
        step(10, 6).reverse = true;
        on(11, {12});
        step(11, 12).pitch = -12;
        on(12, {8});
        pattern.gate = 0x5555;
    }
    return pattern;
}

} // namespace klang
