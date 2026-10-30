#pragma once

namespace krimson {

// The multi-mode filter (docs/DSP.md#voice-runs-at-2-and-is-decimated-as-kalliope):
// a zero-delay-feedback (TPT) state-variable core with resonance, giving LP 12,
// band-pass, high-pass and notch (LP + HP; notch + LP adds a second, unresonant
// 12 dB low-pass). Its resonant state saturates softly, so high resonance
// compresses and growls rather than whistling. LP 24 is a four-pole ladder
// (TPT one-poles, the resonance fed back around all four through a soft
// saturator, the bass made up as resonance rises). Drive saturates the input;
// the output is soft-limited so full resonance stays bounded.
class MultiFilter
{
public:
    enum class Type { Lp12, Lp24, Bp, Hp, NotchLp };

    void reset() noexcept
    {
        m_a1 = m_a2 = m_b1 = m_b2 = 0.0;
        m_ladder[0] = m_ladder[1] = m_ladder[2] = m_ladder[3] = 0.0;
    }
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
    // The ladder (LP 24).
    double m_ladderG = 0.0;         // one-pole gain g / (1 + g)
    double m_ladderK = 0.0;         // resonance feedback, 0 .. ~3.9
    double m_ladder[4] = {0.0, 0.0, 0.0, 0.0};
    float ladder(double x) noexcept;
};

} // namespace krimson
