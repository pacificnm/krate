#pragma once

#include <array>
#include <cstdint>

namespace krimson {

// Krimson's chorus (docs/DSP.md#effects), its own (Kalliope keeps the shared
// one): stereo, in place, a bucket-brigade-style delay per side, the copies
// darkened above ~8 kHz as through the chips and gently saturated.
// CHORUS: one copy per side, swept by a slow triangle a quarter cycle apart
// (wide and smooth). ENSEMBLE: three copies per side on three phases of a
// slow sweep plus a fast shimmer (the thick string-machine ensemble).
class Ensemble
{
public:
    enum Mode { Off = 0, Chorus = 1, Full = 2 };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float *left, float *right, std::uint32_t frames, int mode) noexcept;

private:
    static constexpr std::size_t kSize = 4096; // > 20 ms at 192 kHz; a power of two
    float read(const std::array<float, kSize> &line, double delaySamples) const noexcept;

    std::array<float, kSize> m_left{}, m_right{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_slow = 0.0, m_fast = 0.0; // LFO phases, 0..1
    double m_toneL = 0.0, m_toneR = 0.0, m_tone = 0.0; // the chips' darkening
    int m_lastMode = 0;
};

} // namespace krimson
