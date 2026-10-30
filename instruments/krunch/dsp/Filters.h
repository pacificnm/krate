#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>

#include "dsp/Biquad.h"

namespace krunch {

// The shared second-order section (shared/dsp/Biquad.h).
using Biquad = krate::Biquad;

// One-pole low-pass and high-pass (a coupling capacitor, a stage's treble roll-off).
class OnePole
{
public:
    void reset() noexcept { m_z = 0.0; }
    void setCutoff(double fs, double f) noexcept { m_a = std::exp(-2.0 * std::numbers::pi * std::min(f, 0.49 * fs) / fs); }
    float lowpass(float x) noexcept
    {
        m_z = x + m_a * (m_z - x);
        return float(m_z);
    }
    float highpass(float x) noexcept { return x - lowpass(x); }

private:
    double m_a = 0.0;
    double m_z = 0.0;
};

} // namespace krunch
