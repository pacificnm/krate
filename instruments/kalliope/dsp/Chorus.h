#pragma once

#include <array>
#include <cstdint>

namespace kalliope {

// Kalliope's chorus (docs/DSP.md#effects), its own: mono in, stereo out, one
// bucket-brigade-style delay line read at two taps swept by one triangle in
// opposite directions (the left copy rises while the right falls), so the
// sides move against each other. The copies go through the chips' gentle
// saturation and darkening (two one-pole low-passes, ~7 kHz), read with
// hermite interpolation. I: a slow, gentle sweep; II: faster and deeper.
class Chorus
{
public:
    enum Mode { Off = 0, I = 1, II = 2 };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // in may equal left.
    void process(const float *in, float *left, float *right, std::uint32_t frames, int mode) noexcept;

private:
    static constexpr std::size_t kSize = 4096; // > 15 ms at 192 kHz; a power of two
    float read(double delaySamples) const noexcept;

    std::array<float, kSize> m_line{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_phase = 0.0;                    // the sweep, 0..1
    double m_tone = 0.0;                     // the chips' low-pass coefficient
    double m_inTone = 0.0;                   // into the chips
    double m_toneL1 = 0.0, m_toneL2 = 0.0;   // out of them
    double m_toneR1 = 0.0, m_toneR2 = 0.0;
    int m_lastMode = 0;
};

} // namespace kalliope
