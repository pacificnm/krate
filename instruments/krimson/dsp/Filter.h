#pragma once

namespace krimson {

// The multi-mode filter (docs/DSP.md#voice-runs-at-2-and-is-decimated-as-kalliope):
// a zero-delay-feedback (TPT) state-variable core with resonance, giving LP 12,
// band-pass, high-pass and notch (LP + HP); LP 24 and notch + LP add a second,
// unresonant 12 dB low-pass stage. Drive saturates the input; the output is
// soft-limited so full resonance stays bounded.
class MultiFilter
{
public:
    enum class Type { Lp12, Lp24, Bp, Hp, NotchLp };

    void reset() noexcept { m_a1 = m_a2 = m_b1 = m_b2 = 0.0; }
    // res 0..1 (near self-oscillation at 1); drive 0..1.
    void set(double cutoffHz, double res, double drive, double sampleRate) noexcept;
    float process(float in, Type type) noexcept;

private:
    double m_g = 0.0, m_k = 2.0;    // resonant stage
    double m_h1 = 0.0, m_h2 = 0.0, m_h3 = 0.0;
    double m_bh1 = 0.0, m_bh2 = 0.0, m_bh3 = 0.0; // second stage (Butterworth)
    double m_drive = 1.0;           // input gain into the saturation
    double m_a1 = 0.0, m_a2 = 0.0;  // resonant stage state
    double m_b1 = 0.0, m_b2 = 0.0;  // second stage state
};

} // namespace krimson
