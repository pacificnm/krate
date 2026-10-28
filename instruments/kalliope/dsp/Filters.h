#pragma once

namespace kalliope {

// 4-pole transistor-ladder low-pass, zero-delay feedback (TPT one-pole stages,
// feedback solved per sample), tanh at the input (docs/DSP.md#building-blocks).
// -24 dB output from the 4th stage, -12 dB from the 2nd; resonance from the 4th.
class Ladder
{
public:
    void reset() noexcept { m_s[0] = m_s[1] = m_s[2] = m_s[3] = 0.0; }
    // cutoff in Hz (clamped below Nyquist), k: feedback 0..~4.2 (self-oscillates above 4).
    void set(double cutoffHz, double k, double sampleRate) noexcept;
    float process(float in, bool slope24) noexcept;

private:
    double m_G = 0.0;      // g / (1 + g)
    double m_invOne = 1.0; // 1 / (1 + g)
    double m_k = 0.0;
    double m_makeup = 1.0;
    double m_s[4] = {0.0, 0.0, 0.0, 0.0};
};

// 1-pole TPT high-pass; cutoff 0 = bypass.
class HighPass
{
public:
    void reset() noexcept { m_s = 0.0; }
    void set(double cutoffHz, double sampleRate) noexcept;
    float process(float in) noexcept;

private:
    double m_G = 0.0;
    bool m_bypass = true;
    double m_s = 0.0;
};

} // namespace kalliope
