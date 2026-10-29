#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace krate {

// A low-frequency oscillator, output -1..1. Waves: Sine, Up saw, Down saw,
// Square, Random (a new value each cycle), Triangle, Soft random (glides from
// one random value to the next over each cycle). Kalliope uses one for all
// its voices; Krimson one per voice and one per slot. Deterministic.
class Lfo
{
public:
    enum class Wave : std::uint8_t { Sine, UpSaw, DownSaw, Square, Random, Triangle, SoftRandom };

    void reset(double phase = 0.0) noexcept { m_phase = phase; }
    double next(double hz, Wave wave, double sampleRate) noexcept
    {
        double v = 0.0;
        switch (wave) {
        case Wave::Sine: v = std::sin(2.0 * std::numbers::pi * m_phase); break;
        case Wave::UpSaw: v = 2.0 * m_phase - 1.0; break;
        case Wave::DownSaw: v = 1.0 - 2.0 * m_phase; break;
        case Wave::Square: v = m_phase < 0.5 ? 1.0 : -1.0; break;
        case Wave::Random: v = m_held; break;
        case Wave::Triangle: v = 1.0 - 4.0 * std::fabs(m_phase - 0.5); break;
        case Wave::SoftRandom: {
            const double t = 0.5 - 0.5 * std::cos(std::numbers::pi * m_phase); // eased
            v = m_previous + (m_held - m_previous) * t;
            break;
        }
        }
        m_phase += hz / sampleRate;
        if (m_phase >= 1.0) {
            m_phase -= std::floor(m_phase);
            m_previous = m_held;
            m_rng ^= m_rng << 13;
            m_rng ^= m_rng >> 17;
            m_rng ^= m_rng << 5;
            m_held = double(std::int32_t(m_rng)) / 2147483648.0;
        }
        return v;
    }

private:
    double m_phase = 0.0;
    double m_held = 0.0;
    double m_previous = 0.0;
    std::uint32_t m_rng = 0x6C8E9CF5u;
};

} // namespace krate
