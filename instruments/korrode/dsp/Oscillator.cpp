#include "dsp/Oscillator.h"

#include <algorithm>

#include <cmath>

#include "dsp/PolyBlep.h"

namespace korrode {

namespace {
constexpr double kSawCurve = 0.12;       // the ramp's bend: v + c (v^2 - 1/3), no DC, no extra jump
constexpr double kShaperGain = 14.0;     // the comparator's steepness (the rising edge's slope)
constexpr double kShaperOffset = 0.06;   // its threshold, above the saw's middle: the pulse a little under 50 %
const double kShaperNorm = 1.0 / std::tanh(kShaperGain);
} // namespace

void Oscillator::setSampleRate(double sampleRate) noexcept
{
    m_sampleRate = sampleRate;
    updateIncrement();
}

void Oscillator::setFrequency(double hz) noexcept
{
    m_frequency = hz;
    updateIncrement();
}

void Oscillator::updateIncrement() noexcept
{
    // Stay below Nyquist so the BLEP window never exceeds half a period.
    m_increment = std::clamp(m_frequency / m_sampleRate, 0.0, 0.45);
}

float Oscillator::next() noexcept
{
    const double t = m_phase;
    const double dt = m_increment;
    double v;

    // The ramp, band-limited at its reset, then bent: the bend is continuous
    // across the reset (v^2 is 1 at both ends), so the step stays the BLEP's.
    const double ramp = 2.0 * t - 1.0;
    double saw = ramp - krate::polyBlep(t, dt);
    saw += kSawCurve * (ramp * ramp - 1.0 / 3.0);
    if (m_shape == Shape::Saw)
        v = saw;
    else
        v = std::tanh(kShaperGain * (saw - kShaperOffset)) * kShaperNorm;

    m_phase += dt;
    if (m_phase >= 1.0)
        m_phase -= 1.0;

    return static_cast<float>(v);
}

} // namespace Korrode
