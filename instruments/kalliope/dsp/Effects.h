#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace kalliope {

// Stereo chorus from a mono input (docs/DSP.md#effects): two modulated delay
// lines, one per side, their LFOs a quarter cycle apart, mixed with the dry
// signal. Mode 0 = off (both sides dry), 1 = I, 2 = II. Realtime-safe.
class Chorus
{
public:
    void prepare(double sampleRate) noexcept;
    void process(const float *in, float *left, float *right, std::uint32_t frames, int mode) noexcept;

private:
    static constexpr std::size_t kSize = 8192; // > 15 ms at 192 kHz; power of two
    float read(double delaySamples) const noexcept;

    std::array<float, kSize> m_line{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_phase = 0.0;
};

// Tempo-synced stereo ping-pong delay (docs/DSP.md#effects). The first echo is
// on the left, the next on the right, and so on; the feedback path darkens each
// repeat (one-pole low-pass at 4 kHz). The buffer is allocated once, in the
// constructor; process() never allocates.
class Delay
{
public:
    explicit Delay(double maxSeconds = 2.0, double maxSampleRate = 192000.0);

    void prepare(double sampleRate) noexcept;
    void clear() noexcept;
    // seconds: delay time (clamped to the buffer); level 0..1 wet; feedback 0..0.9.
    void process(float *left, float *right, std::uint32_t frames, double seconds, double level,
                 double feedback) noexcept;

private:
    std::vector<float> m_left, m_right;
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_delay = -1.0;      // smoothed delay, samples
    double m_smoothing = 0.0;
    double m_lowpass = 0.0;     // feedback low-pass coefficient
    double m_lpLeft = 0.0, m_lpRight = 0.0;
};

} // namespace kalliope
