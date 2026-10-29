#include "dsp/ToneStack.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace krunch {

void ToneStack::set(double sampleRate, const Parts &p, double bass, double mid, double treble) noexcept
{
    const double t = std::clamp(treble, 0.0, 1.0);
    const double m = std::clamp(mid, 0.0, 1.0);
    const double l = std::exp((std::clamp(bass, 0.0, 1.0) - 1.0) * 3.4); // log taper
    const double R1 = p.r1, R2 = p.r2, R3 = p.r3, R4 = p.r4, C1 = p.c1, C2 = p.c2, C3 = p.c3;

    // H(s) = (b1 s + b2 s^2 + b3 s^3) / (1 + a1 s + a2 s^2 + a3 s^3)
    const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                      + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                      + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                      + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                      + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                      - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                      + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                      + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                      + t * l * C1 * C2 * C3 * R1 * R2 * R4;
    const double a0 = 1.0;
    const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                      + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                      + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                      + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4
                         + C2 * C3 * R3 * R4);
    const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                      - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                      + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                      + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

    // Bilinear transform: s = c (1 - z^-1) / (1 + z^-1). Each s^k term becomes
    // c^k (1 - z^-1)^k (1 + z^-1)^(3-k), expanded below.
    const double c = 2.0 * sampleRate;
    const std::array<double, 4> bs{0.0, b1, b2, b3}, as{a0, a1, a2, a3};
    // (1 - z^-1)^k (1 + z^-1)^(3-k) for k = 0..3, coefficients of z^0..z^-3.
    static constexpr double kExpand[4][4] = {{1, 3, 3, 1}, {1, 1, -1, -1}, {1, -1, -1, 1}, {1, -3, 3, -1}};
    std::array<double, 4> B{}, A{};
    double ck = 1.0;
    for (int k = 0; k < 4; ++k) {
        for (int n = 0; n < 4; ++n) {
            B[std::size_t(n)] += bs[std::size_t(k)] * ck * kExpand[k][n];
            A[std::size_t(n)] += as[std::size_t(k)] * ck * kExpand[k][n];
        }
        ck *= c;
    }
    for (int n = 0; n < 4; ++n) {
        m_b[std::size_t(n)] = B[std::size_t(n)] / A[0];
        m_a[std::size_t(n)] = A[std::size_t(n)] / A[0];
    }
}

double ToneStack::magnitude(double sampleRate, double f) const noexcept
{
    const double w = 2.0 * std::numbers::pi * f / sampleRate;
    std::complex<double> num = 0.0, den = 0.0;
    for (int k = 0; k < 4; ++k) {
        num += m_b[std::size_t(k)] * std::polar(1.0, -w * k);
        den += m_a[std::size_t(k)] * std::polar(1.0, -w * k);
    }
    return std::abs(num / den);
}

} // namespace krunch
