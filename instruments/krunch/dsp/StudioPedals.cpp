#include "dsp/StudioPedals.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace krunch {

namespace {

std::string tenth(float v) { return std::format("{:.1f}", double(v) * 10.0); }
std::string percent(float v) { return std::format("{}%", std::lround(double(v) * 100.0)); }
std::string signedDb(double db) { return std::format("{:+.1f} dB", std::round(db * 10.0) / 10.0 + 0.0); }

double attackMs(float v) { return 0.5 * std::pow(100.0, double(v)); } // 0.5 .. 50 ms
std::string attackText(float v) { return std::format("{:.1f} ms", attackMs(v)); }
double levelDb(float v) { return -12.0 + 24.0 * double(v); }
std::string levelText(float v) { return signedDb(levelDb(v)); }

// The compressor's curve for a Sustain setting.
double threshold(float s) { return -12.0 - 36.0 * double(s); } // -12 .. -48 dBFS
double ratio(float s) { return 2.0 + 10.0 * double(s); }       // 2:1 .. 12:1
constexpr double kKneeDb = 8.0;

template<int Bands>
std::string bandText(float v)
{
    return signedDb(GraphicEq<Bands>::bandDb(v));
}
template<int Bands>
std::string eqLevelText(float v)
{
    return signedDb((double(v) - 0.5) * 24.0);
}

} // namespace

// --- Compressor -------------------------------------------------------------------

const PedalInfo Compressor::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"sustain", "SUSTAIN", 0.5f, &tenth},
        {"attack", "ATTACK", 0.4f, &attackText},
        {"level", "LEVEL", 0.5f, &levelText},
        {"blend", "BLEND", 1.0f, &percent},
    };
    return PedalInfo{"compressor", "Compressor", "#C4372F", knobs, false};
}();

double Compressor::gainDb(double in, float sustain)
{
    const double t = threshold(sustain), r = ratio(sustain);
    // Soft knee: a quadratic blend over kKneeDb around the threshold.
    double reduction = 0.0;
    const double over = in - t;
    if (2.0 * over >= kKneeDb)
        reduction = over * (1.0 - 1.0 / r);
    else if (2.0 * over > -kKneeDb)
        reduction = (1.0 - 1.0 / r) * (over + kKneeDb / 2.0) * (over + kKneeDb / 2.0) / (2.0 * kKneeDb);
    // Make-up: about half of what a full-scale signal loses, so more sustain
    // isn't quieter.
    const double makeup = 0.5 * (-t) * (1.0 - 1.0 / r);
    return makeup - reduction;
}

void Compressor::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_release = std::exp(-1.0 / (0.200 * sampleRate));
}

void Compressor::resetEffect() noexcept { m_envelope = 0.0; }

void Compressor::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    const float sustain = knob(Sustain);
    const double attack = std::exp(-1.0 / (attackMs(knob(Attack)) * 0.001 * m_rate));
    const double level = std::pow(10.0, levelDb(knob(Level)) / 20.0);
    const double blend = knob(Blend);
    for (std::uint32_t n = 0; n < frames; ++n) {
        const double x = buffer[n];
        const double a = std::fabs(x);
        m_envelope = a > m_envelope ? a + attack * (m_envelope - a) : a + m_release * (m_envelope - a);
        const double inDb = 20.0 * std::log10(std::max(m_envelope, 1e-6));
        const double gain = std::pow(10.0, gainDb(inDb, sustain) / 20.0);
        buffer[n] = float((x * (1.0 - blend) + x * gain * blend) * level);
    }
}

// --- Graphic EQs ------------------------------------------------------------------

template<> const std::array<double, 5> Eq5::kFrequencies{120.0, 360.0, 800.0, 2000.0, 4500.0};
template<> const double Eq5::kRangeDb = 15.0;
template<> const double Eq5::kQ = 0.9;
template<> const PedalInfo Eq5::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"band120", "120", 0.5f, &bandText<5>},  {"band360", "360", 0.5f, &bandText<5>},
        {"band800", "800", 0.5f, &bandText<5>},  {"band2k", "2K", 0.5f, &bandText<5>},
        {"band4k5", "4.5K", 0.5f, &bandText<5>}, {"level", "LEVEL", 0.5f, &eqLevelText<5>},
    };
    return PedalInfo{"eq5", "5-Band EQ", "#D8D4C8", knobs, false, 1.3f, true};
}();

template<> const std::array<double, 10> Eq10::kFrequencies{31.25, 62.5, 125.0, 250.0, 500.0,
                                                           1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
template<> const double Eq10::kRangeDb = 12.0;
template<> const double Eq10::kQ = 1.41; // an octave wide
template<> const PedalInfo Eq10::kInfo = []() {
    static const KnobInfo knobs[] = {
        {"band31", "31", 0.5f, &bandText<10>},   {"band62", "62", 0.5f, &bandText<10>},
        {"band125", "125", 0.5f, &bandText<10>}, {"band250", "250", 0.5f, &bandText<10>},
        {"band500", "500", 0.5f, &bandText<10>}, {"band1k", "1K", 0.5f, &bandText<10>},
        {"band2k", "2K", 0.5f, &bandText<10>},   {"band4k", "4K", 0.5f, &bandText<10>},
        {"band8k", "8K", 0.5f, &bandText<10>},   {"band16k", "16K", 0.5f, &bandText<10>},
        {"level", "LEVEL", 0.5f, &eqLevelText<10>},
    };
    return PedalInfo{"eq10", "10-Band EQ", "#2B2D31", knobs, false, 2.4f, true};
}();

// --- Parametric EQ -----------------------------------------------------------------

namespace {
const char *const kTypeNames[] = {"Off", "Peak", "Low shelf", "High shelf", "Low cut", "High cut"};
std::string typeText(float v) { return kTypeNames[int(ParametricEq::type(v))]; }
std::string freqText(float v)
{
    const double hz = ParametricEq::freqHz(v);
    return hz < 1000.0 ? std::format("{} Hz", std::lround(hz)) : std::format("{:.2g} kHz", hz / 1000.0);
}
std::string gainText(float v) { return std::format("{:+.1f} dB", std::round(ParametricEq::gainDb(v) * 10.0) / 10.0 + 0.0); }
std::string qText(float v) { return std::format("{:.2f}", ParametricEq::q(v)); }
std::string peqLevelText(float v) { return std::format("{:+.1f} dB", std::round(ParametricEq::levelDb(v) * 10.0) / 10.0 + 0.0); }
// Position of a type on its switch, and of a frequency on its log knob.
constexpr float typePosition(int t) { return float(t) / float(ParametricEq::TypeCount - 1); }
float freqPosition(double hz) { return float(std::log(hz / 20.0) / std::log(1000.0)); }
float qPosition(double q) { return float(std::log(q / 0.3) / std::log(10.0 / 0.3)); }
} // namespace

ParametricEq::Type ParametricEq::type(float v)
{
    return Type(std::clamp(int(std::lround(double(v) * (TypeCount - 1))), 0, TypeCount - 1));
}
double ParametricEq::freqHz(float v) { return 20.0 * std::pow(1000.0, std::clamp(double(v), 0.0, 1.0)); }
double ParametricEq::gainDb(float v) { return (std::clamp(double(v), 0.0, 1.0) - 0.5) * 36.0; }
double ParametricEq::q(float v) { return 0.3 * std::pow(10.0 / 0.3, std::clamp(double(v), 0.0, 1.0)); }
double ParametricEq::levelDb(float v) { return (std::clamp(double(v), 0.0, 1.0) - 0.5) * 24.0; }

// Flat to start with: a low shelf, four peaks, a high shelf, all at 0 dB.
const PedalInfo ParametricEq::kInfo = []() {
    struct Band
    {
        const char *type, *freq, *gain, *q;
        const char *label;
        int kind;
        double hz;
    };
    static const Band bands[kBands] = {
        {"b1_type", "b1_freq", "b1_gain", "b1_q", "1", LowShelf, 100.0},  {"b2_type", "b2_freq", "b2_gain", "b2_q", "2", Peak, 250.0},
        {"b3_type", "b3_freq", "b3_gain", "b3_q", "3", Peak, 700.0},      {"b4_type", "b4_freq", "b4_gain", "b4_q", "4", Peak, 1800.0},
        {"b5_type", "b5_freq", "b5_gain", "b5_q", "5", Peak, 4000.0},     {"b6_type", "b6_freq", "b6_gain", "b6_q", "6", HighShelf, 8000.0},
    };
    static const std::array<KnobInfo, kLevel + 1> knobs = [] {
        std::array<KnobInfo, kLevel + 1> k{};
        for (int b = 0; b < kBands; ++b) {
            const Band &d = bands[b];
            k[std::size_t(knobOf(b, TypeKnob))] = {d.type, "TYPE", typePosition(d.kind), &typeText, TypeCount};
            k[std::size_t(knobOf(b, Freq))] = {d.freq, "FREQ", freqPosition(d.hz), &freqText};
            k[std::size_t(knobOf(b, Gain))] = {d.gain, "GAIN", 0.5f, &gainText};
            k[std::size_t(knobOf(b, Q))] = {d.q, "Q", qPosition(0.707), &qText};
        }
        k[kLevel] = {"level", "LEVEL", 0.5f, &peqLevelText};
        return k;
    }();
    PedalInfo info{"parametric_eq", "Parametric EQ", "#1E2A33", knobs, false, 1.3f, false};
    info.editor = true;
    return info;
}();

void ParametricEq::tune(Biquad &f, double rate, Type type, double hz, double db, double q) noexcept
{
    switch (type) {
    case Peak: f.setPeak(rate, hz, q, db); break;
    case LowShelf: f.setLowShelf(rate, hz, db, q); break;
    case HighShelf: f.setHighShelf(rate, hz, db, q); break;
    case LowCut: f.setHighpass(rate, hz, q); break;
    case HighCut: f.setLowpass(rate, hz, q); break;
    case Off:
    case TypeCount: f.setPeak(rate, 1000.0, 0.707, 0.0); break; // (flat: not run)
    }
}

double ParametricEq::responseDb(const float *knobs, double rate, double hz)
{
    double db = levelDb(knobs[kLevel]);
    for (int b = 0; b < kBands; ++b) {
        const Type t = type(knobs[knobOf(b, TypeKnob)]);
        if (t == Off)
            continue;
        Biquad f;
        tune(f, rate, t, freqHz(knobs[knobOf(b, Freq)]), gainDb(knobs[knobOf(b, Gain)]), q(knobs[knobOf(b, Q)]));
        db += 20.0 * std::log10(std::max(f.magnitude(rate, hz), 1e-9));
    }
    return db;
}

void ParametricEq::prepareEffect(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_seen.fill(-1.0f);
    m_level = -1.0f;
    resetEffect();
}

void ParametricEq::resetEffect() noexcept
{
    for (Biquad &b : m_bands)
        b.reset();
}

void ParametricEq::processEffect(float *buffer, std::uint32_t frames) noexcept
{
    std::array<Type, kBands> types{};
    for (int b = 0; b < kBands; ++b) {
        const float t = knob(knobOf(b, TypeKnob)), f = knob(knobOf(b, Freq)), g = knob(knobOf(b, Gain)), qq = knob(knobOf(b, Q));
        types[std::size_t(b)] = type(t);
        float *seen = &m_seen[std::size_t(knobOf(b, TypeKnob))];
        if (seen[0] != t || seen[1] != f || seen[2] != g || seen[3] != qq) {
            seen[0] = t;
            seen[1] = f;
            seen[2] = g;
            seen[3] = qq;
            tune(m_bands[std::size_t(b)], m_rate, types[std::size_t(b)], freqHz(f), gainDb(g), q(qq));
        }
    }
    for (int b = 0; b < kBands; ++b)
        if (types[std::size_t(b)] != Off)
            for (std::uint32_t n = 0; n < frames; ++n)
                buffer[n] = m_bands[std::size_t(b)].process(buffer[n]);
    // Level, ramped.
    const float target = float(std::pow(10.0, levelDb(knob(kLevel)) / 20.0));
    if (m_level < 0.0f)
        m_level = target;
    const float step = (target - m_level) / float(frames);
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_level += step;
        buffer[n] *= m_level;
    }
    m_level = target;
}

} // namespace krunch
