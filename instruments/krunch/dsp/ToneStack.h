#pragma once

#include <array>

namespace krunch {

// The passive bass / mid / treble network of a valve amp, modelled from its
// circuit (D. T. Yeh and J. O. Smith, the tone stack paper, DAFx 2006): a
// third-order filter whose three controls
// interact as on the real thing, discretised with the bilinear transform.
class ToneStack
{
public:
    // Component values: two classic layouts.
    struct Parts
    {
        double r1, r2, r3, r4, c1, c2, c3;
    };
    static constexpr Parts kAmerican{250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9}; // scooped, glassy
    static constexpr Parts kBritish{220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9};  // more mids

    void reset() noexcept { m_z.fill(0.0); }

    // Controls 0..1 (bass follows a log pot, as on the amp).
    void set(double sampleRate, const Parts &parts, double bass, double mid, double treble) noexcept;

    float process(float x) noexcept
    {
        const double y = m_b[0] * x + m_z[0];
        m_z[0] = m_b[1] * x - m_a[1] * y + m_z[1];
        m_z[1] = m_b[2] * x - m_a[2] * y + m_z[2];
        m_z[2] = m_b[3] * x - m_a[3] * y;
        return float(y);
    }

    double magnitude(double sampleRate, double f) const noexcept; // tests

private:
    std::array<double, 4> m_b{1.0, 0.0, 0.0, 0.0};
    std::array<double, 4> m_a{1.0, 0.0, 0.0, 0.0};
    std::array<double, 3> m_z{};
};

} // namespace krunch
