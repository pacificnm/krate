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

} // namespace krunch
