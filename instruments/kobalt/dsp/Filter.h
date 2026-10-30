#pragma once

namespace kobalt {

// Kobalt's low-pass (docs/BRIEF.md#sound-engine): four cascaded one-pole
// stages, zero-delay feedback (TPT stages, the linear feedback solved per
// sample), -24 dB/octave. Each stage's input passes through a soft limit, as
// a chain of transconductance amplifiers saturates, and so does the
// feedback: a loud resonant note rounds off and buzzes instead of growing.
// Resonance self-oscillates above k = 4.
class Filter
{
public:
    void reset() noexcept { m_s[0] = m_s[1] = m_s[2] = m_s[3] = 0.0; }
    // cutoff in Hz (clamped below Nyquist), k: feedback 0..~4.4.
    void set(double cutoffHz, double k, double sampleRate) noexcept;
    float process(float in) noexcept;

private:
    double m_G = 0.0;      // g / (1 + g)
    double m_invOne = 1.0; // 1 / (1 + g)
    double m_k = 0.0;
    double m_makeup = 1.0;
    double m_s[4] = {0.0, 0.0, 0.0, 0.0};
};

} // namespace kobalt
