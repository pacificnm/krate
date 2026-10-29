#pragma once

#include <array>
#include <vector>

#include "dsp/Filters.h"
#include "dsp/Pedal.h"
#include "dsp/Reverb.h"

namespace krunch {

// The time-based pedals, at home in the effects loop (docs/PEDALS.md). Mono,
// like the amp up to its cabinet.

// Chorus: the guitar mixed with a copy of itself through a short delay
// (about 7 ms) swept by a sine LFO.
class Chorus final : public Pedal
{
public:
    enum Knob : int { Rate, Depth, Mix };
    static const PedalInfo kInfo;

    Chorus() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    static constexpr std::size_t kSize = 4096; // > 12 ms at 192 kHz; a power of two
    std::array<float, kSize> m_line{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_phase = 0.0;
};

// Delay: repeats that darken and soften as they go (a low-pass, a high-pass
// and a gentle saturation in the feedback), the time gliding when it's turned.
class Delay final : public Pedal
{
public:
    enum Knob : int { Time, Feedback, Tone, Mix };
    static const PedalInfo kInfo;
    static constexpr double kMaxSeconds = 1.5;

    Delay();

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    std::vector<float> m_line; // kMaxSeconds at 192 kHz, allocated once
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_delay = -1.0; // samples, smoothed
    double m_glide = 0.0;
    OnePole m_toneLowpass, m_lowCut;
    float m_tone = -1.0f;
};

// Reverb: a room-to-hall reverb (Krimson's feedback-delay network) added to
// the dry signal, its tail darkened by Tone.
class Reverb final : public Pedal
{
public:
    enum Knob : int { Decay, Tone, Mix };
    static const PedalInfo kInfo;

    Reverb() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    krate::Reverb m_reverb;
    std::array<float, kMaxFrames> m_left{}, m_right{};
    double m_rate = 48000.0;
    OnePole m_toneLowpass;
    float m_tone = -1.0f;
};

} // namespace krunch
