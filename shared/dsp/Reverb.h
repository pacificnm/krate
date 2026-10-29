#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace krate {

// A small feedback-delay-network reverb (Krimson's docs/DSP.md#effects): eight
// delay lines of mutually prime lengths, mixed through a Hadamard matrix each
// sample, each line damped by a one-pole low-pass, the feedback gain set per
// line from the decay time. Stereo in (its mid feeds the network), stereo out
// (from different lines). Buffers are allocated in the constructor; process()
// never allocates.
class Reverb
{
public:
    explicit Reverb(double maxSampleRate = 192000.0);

    void prepare(double sampleRate) noexcept;
    void clear() noexcept;
    // In place: adds `level` (0..1) of the reverb to the dry signal. size 0..1:
    // decay (RT60) 0.4 .. 8 s.
    void process(float *left, float *right, std::uint32_t frames, double size, double level) noexcept;

    static double decaySeconds(double size) noexcept;

private:
    static constexpr int kLines = 8;
    std::array<std::vector<float>, kLines> m_lines;
    std::array<std::size_t, kLines> m_length{};
    std::array<std::size_t, kLines> m_pos{};
    std::array<double, kLines> m_damp{};
    double m_rate = 48000.0;
    double m_dampCoefficient = 0.0;
    double m_lastSize = -1.0;
    std::array<double, kLines> m_gain{};
};

} // namespace krate
