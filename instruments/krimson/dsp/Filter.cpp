#include "krimson/dsp/Filter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace krimson {

namespace {
constexpr double kButterworthK = 1.4142135623730951; // 1/Q for Q = 0.707
constexpr double kMaxDrive = 8.0;

// One TPT state-variable step: returns lp, bp; updates the two states.
struct Step
{
    double lp, bp;
};
inline Step svf(double x, double &ic1, double &ic2, double a1, double a2, double a3) noexcept
{
    const double v3 = x - ic2;
    const double v1 = a1 * ic1 + a2 * v3;
    const double v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;
    return {v2, v1};
}
} // namespace

void MultiFilter::set(double cutoffHz, double res, double drive, double sampleRate) noexcept
{
    const double fc = std::clamp(cutoffHz, 10.0, 0.42 * sampleRate);
    m_g = std::tan(std::numbers::pi * fc / sampleRate);
    m_k = 2.0 * (1.0 - 0.97 * std::clamp(res, 0.0, 1.0)); // 2 (no peak) .. 0.06 (ringing)
    const auto coefficients = [this](double k, double &h1, double &h2, double &h3) {
        h1 = 1.0 / (1.0 + m_g * (m_g + k));
        h2 = m_g * h1;
        h3 = m_g * h2;
    };
    coefficients(m_k, m_h1, m_h2, m_h3);
    coefficients(kButterworthK, m_bh1, m_bh2, m_bh3);
    m_drive = 1.0 + (kMaxDrive - 1.0) * std::clamp(drive, 0.0, 1.0);
}

float MultiFilter::process(float in, Type type) noexcept
{
    // Drive: a tanh with gain, scaled so quiet signals keep their level and loud ones grind.
    double x = double(in);
    if (m_drive > 1.0)
        x = std::tanh(m_drive * x) / std::sqrt(m_drive);

    const Step s = svf(x, m_a1, m_a2, m_h1, m_h2, m_h3);
    const double hp = x - m_k * s.bp - s.lp;
    double y = 0.0;
    switch (type) {
    case Type::Lp12: y = s.lp; break;
    case Type::Bp: y = s.bp; break;
    case Type::Hp: y = hp; break;
    case Type::Lp24: y = svf(s.lp, m_b1, m_b2, m_bh1, m_bh2, m_bh3).lp; break;
    case Type::NotchLp: y = svf(s.lp + hp, m_b1, m_b2, m_bh1, m_bh2, m_bh3).lp; break;
    }
    // A soft limit keeps self-oscillation and hot drive bounded (gentle below 0.5).
    if (!std::isfinite(y)) {
        reset();
        return 0.0f;
    }
    return float(std::tanh(y));
}

} // namespace krimson
