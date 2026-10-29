#include "dsp/BlepOscillator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace krate {

void Oscillator::reset(double phase) noexcept
{
    m_phase = phase - std::floor(phase);
    m_pending = 0.0;
    m_wrapped = false;
}

double Oscillator::naive(double phase, double width) const noexcept
{
    switch (m_wave) {
    case Wave::Triangle: return 1.0 - 4.0 * std::fabs(phase - 0.5); // -1 at 0, +1 at 0.5
    case Wave::Saw: return 2.0 * phase - 1.0;
    case Wave::Pulse: return (phase < width ? 1.0 : -1.0) - (2.0 * width - 1.0); // without its DC, as AC-coupled
    case Wave::Square: return phase < 0.5 ? 1.0 : -1.0;
    case Wave::Sine: return std::sin(2.0 * std::numbers::pi * phase);
    case Wave::Noise: return 0.0;
    }
    return 0.0;
}

void Oscillator::jump(double height, double t) noexcept
{
    m_value += height * t * t * 0.5;
    m_pending -= height * (1.0 - t) * (1.0 - t) * 0.5;
}

void Oscillator::addJumps(double from, double to, double increment, double width, double after) noexcept
{
    if (increment <= 0.0)
        return;
    const auto at = [&](double edge, double height) {
        // The edge, or the same edge one cycle on, inside (from, to].
        for (double e : {edge, edge + 1.0})
            if (e > from && e <= to)
                jump(height, (to - e) / increment + after);
    };
    switch (m_wave) {
    case Wave::Saw:
        at(1.0, -2.0);
        break;
    case Wave::Square:
        at(0.5, -2.0);
        at(1.0, 2.0);
        break;
    case Wave::Pulse:
        if (width > 0.0 && width < 1.0) {
            at(width, -2.0);
            at(1.0, 2.0);
        }
        break;
    case Wave::Triangle:
    case Wave::Sine:
    case Wave::Noise:
        break; // continuous
    }
}

float Oscillator::next(double increment, double width, double syncT) noexcept
{
    increment = std::clamp(increment, 0.0, 0.45);
    width = std::clamp(width, 0.02, 0.98);

    if (m_wave == Wave::Noise) {
        m_noise ^= m_noise << 13;
        m_noise ^= m_noise >> 17;
        m_noise ^= m_noise << 5;
        m_phase += increment; // keeps wrapping (as a sync master) consistent
        m_wrapped = m_phase >= 1.0;
        m_wrapT = m_wrapped ? (m_phase - 1.0) / std::max(increment, 1e-12) : 0.0;
        m_phase -= std::floor(m_phase);
        return float(std::int32_t(m_noise)) * (1.0f / 2147483648.0f);
    }

    m_value = naive(m_phase, width) + m_pending;
    m_pending = 0.0;
    const double from = m_phase;

    if (syncT < 0.0) {
        const double to = from + increment;
        addJumps(from, to, increment, width, 0.0);
        m_wrapped = to >= 1.0;
        m_wrapT = m_wrapped ? (to - 1.0) / increment : 0.0;
        m_phase = to - std::floor(to);
    } else {
        // Up to the master's wrap, then reset to 0 and run on for syncT.
        syncT = std::clamp(syncT, 0.0, 1.0);
        const double atReset = from + increment * (1.0 - syncT);
        addJumps(from, atReset, increment, width, syncT);
        const double before = naive(atReset - std::floor(atReset), width);
        const double resetHeight = naive(0.0, width) - before;
        if (resetHeight != 0.0)
            jump(resetHeight, syncT);
        const double to = increment * syncT;
        addJumps(0.0, to, increment, width, 0.0); // a pulse edge right after the reset
        m_wrapped = atReset >= 1.0;
        m_wrapT = m_wrapped ? (atReset - 1.0) / increment + syncT : 0.0;
        m_phase = to;
    }
    return float(m_value);
}

} // namespace krate
