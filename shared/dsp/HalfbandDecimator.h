#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace krate {

// 2:1 decimator: windowed-sinc half-band FIR (Blackman window), N taps.
// Feed two input samples, get one output at half the rate. Cascade two for 4:1.
// With N = 31 the passband is flat to ~0.16 of the input rate and the stopband
// (above 0.34) is down more than 70 dB.
template<std::size_t N>
class HalfbandDecimator
{
    static_assert(N % 4 == 3, "half-band length must be 4k+3 so the end taps are non-zero");

public:
    HalfbandDecimator() noexcept
    {
        constexpr double c = (N - 1) / 2.0;
        double sum = 0.0;
        for (std::size_t n = 0; n < N; ++n) {
            const double m = double(n) - c;
            const double ideal = m == 0.0 ? 0.5 : std::sin(std::numbers::pi * m / 2.0) / (std::numbers::pi * m);
            const double phase = 2.0 * std::numbers::pi * double(n) / double(N - 1);
            const double window = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
            m_coeffs[n] = ideal * window;
            sum += m_coeffs[n];
        }
        for (auto &h : m_coeffs)
            h /= sum; // unity DC gain
    }

    void reset() noexcept
    {
        m_buffer.fill(0.0);
        m_pos = 0;
    }

    float process(float first, float second) noexcept
    {
        push(first);
        push(second);
        // Newest sample at m_pos; the doubled buffer keeps the window contiguous.
        double y = 0.0;
        for (std::size_t i = 0; i < N; ++i)
            y += m_coeffs[i] * m_buffer[m_pos + i];
        return static_cast<float>(y);
    }

private:
    void push(float x) noexcept
    {
        m_pos = m_pos == 0 ? N - 1 : m_pos - 1;
        m_buffer[m_pos] = x;
        m_buffer[m_pos + N] = x;
    }

    std::array<double, N> m_coeffs{};
    std::array<double, 2 * N> m_buffer{};
    std::size_t m_pos = 0;
};

} // namespace krate
