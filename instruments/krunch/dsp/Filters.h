#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>

namespace krunch {

// Second-order section (transposed direct form II, double state). Coefficients
// from the usual cookbook formulas; set*() keeps the state, so a filter can be
// retuned between blocks without a click.
class Biquad
{
public:
    void reset() noexcept { m_z1 = m_z2 = 0.0; }

    float process(float x) noexcept
    {
        const double y = m_b0 * x + m_z1;
        m_z1 = m_b1 * x - m_a1 * y + m_z2;
        m_z2 = m_b2 * x - m_a2 * y;
        return float(y);
    }

    void setLowpass(double fs, double f, double q) noexcept
    {
        const Pre p(fs, f, q);
        set((1.0 - p.cosw) / 2.0, 1.0 - p.cosw, (1.0 - p.cosw) / 2.0, 1.0 + p.alpha, -2.0 * p.cosw, 1.0 - p.alpha);
    }
    void setHighpass(double fs, double f, double q) noexcept
    {
        const Pre p(fs, f, q);
        set((1.0 + p.cosw) / 2.0, -(1.0 + p.cosw), (1.0 + p.cosw) / 2.0, 1.0 + p.alpha, -2.0 * p.cosw, 1.0 - p.alpha);
    }
    void setPeak(double fs, double f, double q, double db) noexcept
    {
        const Pre p(fs, f, q);
        const double a = std::pow(10.0, db / 40.0);
        set(1.0 + p.alpha * a, -2.0 * p.cosw, 1.0 - p.alpha * a, 1.0 + p.alpha / a, -2.0 * p.cosw, 1.0 - p.alpha / a);
    }
    void setLowShelf(double fs, double f, double db) noexcept { setShelf(fs, f, db, false); }
    void setHighShelf(double fs, double f, double db) noexcept { setShelf(fs, f, db, true); }

    // Magnitude at f (tests and level checks).
    double magnitude(double fs, double f) const noexcept
    {
        const double w = 2.0 * std::numbers::pi * f / fs;
        const std::array<double, 3> b{m_b0, m_b1, m_b2}, a{1.0, m_a1, m_a2};
        return std::abs(poly(b, w) / poly(a, w));
    }

private:
    struct Pre
    {
        double cosw, alpha;
        Pre(double fs, double f, double q)
        {
            const double w = 2.0 * std::numbers::pi * std::min(f, 0.49 * fs) / fs;
            cosw = std::cos(w);
            alpha = std::sin(w) / (2.0 * q);
        }
    };

    template<std::size_t N>
    static std::complex<double> poly(const std::array<double, N> &c, double w)
    {
        std::complex<double> sum = 0.0;
        for (std::size_t k = 0; k < N; ++k)
            sum += c[k] * std::polar(1.0, -w * double(k));
        return sum;
    }

    void setShelf(double fs, double f, double db, bool high) noexcept
    {
        const double a = std::pow(10.0, db / 40.0);
        const double w = 2.0 * std::numbers::pi * std::min(f, 0.49 * fs) / fs;
        const double cosw = std::cos(w);
        const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0); // shelf slope 1
        const double sq = 2.0 * std::sqrt(a) * alpha;
        const double s = high ? -1.0 : 1.0; // the high shelf mirrors the low one
        set(a * ((a + 1.0) - s * (a - 1.0) * cosw + sq), s * 2.0 * a * ((a - 1.0) - s * (a + 1.0) * cosw),
            a * ((a + 1.0) - s * (a - 1.0) * cosw - sq), (a + 1.0) + s * (a - 1.0) * cosw + sq,
            -s * 2.0 * ((a - 1.0) + s * (a + 1.0) * cosw), (a + 1.0) + s * (a - 1.0) * cosw - sq);
    }

    void set(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        m_b0 = b0 / a0;
        m_b1 = b1 / a0;
        m_b2 = b2 / a0;
        m_a1 = a1 / a0;
        m_a2 = a2 / a0;
    }

    double m_b0 = 1.0, m_b1 = 0.0, m_b2 = 0.0, m_a1 = 0.0, m_a2 = 0.0;
    double m_z1 = 0.0, m_z2 = 0.0;
};

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
