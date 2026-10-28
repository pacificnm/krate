#include "dsp/Oscillator.h"

#include <algorithm>

#include "dsp/PolyBlep.h"

namespace k303 {

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

    if (m_shape == Shape::Saw) {
        v = 2.0 * t - 1.0;
        v -= krate::polyBlep(t, dt);
    } else {
        v = t < 0.5 ? 1.0 : -1.0;
        v += krate::polyBlep(t, dt);
        double t2 = t + 0.5;
        if (t2 >= 1.0)
            t2 -= 1.0;
        v -= krate::polyBlep(t2, dt);
    }

    m_phase += dt;
    if (m_phase >= 1.0)
        m_phase -= 1.0;

    return static_cast<float>(v);
}

} // namespace k303
