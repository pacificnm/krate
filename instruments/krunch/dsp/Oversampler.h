#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace krunch {

// Half-band FIR (Blackman-windowed sinc, as krate::HalfbandDecimator): the
// anti-imaging and anti-aliasing filter of each 2x stage.
template<std::size_t N>
class Halfband
{
    static_assert(N % 4 == 3, "half-band length must be 4k+3");

public:
    Halfband() noexcept
    {
        constexpr double c = (N - 1) / 2.0;
        double sum = 0.0;
        for (std::size_t n = 0; n < N; ++n) {
            const double m = double(n) - c;
            const double ideal = m == 0.0 ? 0.5 : std::sin(std::numbers::pi * m / 2.0) / (std::numbers::pi * m);
            const double phase = 2.0 * std::numbers::pi * double(n) / double(N - 1);
            m_coeffs[n] = ideal * (0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase));
            sum += m_coeffs[n];
        }
        for (auto &h : m_coeffs)
            h /= sum; // unity DC gain
    }

    void reset() noexcept
    {
        m_buffer.fill(0.0f);
        m_pos = 0;
    }

    void push(float x) noexcept
    {
        m_pos = m_pos == 0 ? N - 1 : m_pos - 1;
        m_buffer[m_pos] = x;
        m_buffer[m_pos + N] = x;
    }

    float output() const noexcept
    {
        double y = 0.0;
        for (std::size_t i = 0; i < N; ++i)
            y += m_coeffs[i] * m_buffer[m_pos + i];
        return float(y);
    }

private:
    std::array<double, N> m_coeffs{};
    std::array<float, 2 * N> m_buffer{};
    std::size_t m_pos = 0;
};

// 4x up and down around a nonlinearity: two 2x stages each way. Each base-rate
// sample becomes four (up), and four come back as one (down). About 0.5 ms
// of delay in all.
class Oversampler4x
{
public:
    static constexpr int kFactor = 4;

    void reset() noexcept
    {
        m_up1.reset();
        m_up2.reset();
        m_down1.reset();
        m_down2.reset();
    }

    void up(float x, float *out) noexcept
    {
        float half[2];
        interpolate(m_up1, x, half);
        interpolate(m_up2, half[0], out);
        interpolate(m_up2, half[1], out + 2);
    }

    float down(const float *in) noexcept
    {
        const float a = decimate(m_down2, in[0], in[1]); // in order: one stream
        const float b = decimate(m_down2, in[2], in[3]);
        return decimate(m_down1, a, b);
    }

private:
    using Filter = Halfband<23>;

    // Zero-stuff, filter, make up the halved level.
    static void interpolate(Filter &f, float x, float *out) noexcept
    {
        f.push(x);
        out[0] = 2.0f * f.output();
        f.push(0.0f);
        out[1] = 2.0f * f.output();
    }
    static float decimate(Filter &f, float a, float b) noexcept
    {
        f.push(a);
        f.push(b);
        return f.output();
    }

    Filter m_up1, m_up2, m_down1, m_down2;
};

} // namespace krunch
