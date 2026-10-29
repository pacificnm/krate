#pragma once

#include <array>

#include "dsp/Filters.h"
#include "dsp/Pedal.h"

namespace krunch {

// The sweeping pedals (docs/PEDALS.md).

// Flanger: the classic analog bucket-brigade flanger, in broad strokes: the
// guitar mixed with a copy through a short delay (0.25 .. 10 ms) swept by a
// slow triangle, evenly in pitch (exponentially), with Color feeding the
// copy back for the resonant, metallic sweep; the copy loses its top above
// about 8 kHz, as through the chip. FILTER MATRIX stops the sweep: Range then
// sets the delay by hand, for a fixed comb.
class Flanger final : public Pedal
{
public:
    enum Knob : int { Rate, Range, Color, Matrix };
    static const PedalInfo kInfo;

    Flanger() : Pedal(kInfo) {}

    // Delay in ms at sweep position 0..1 for a Range setting (tests).
    static double delayMs(float range, double sweep);

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    static constexpr std::size_t kSize = 4096; // > 10 ms at 192 kHz; a power of two
    std::array<float, kSize> m_line{};
    std::size_t m_write = 0;
    double m_rate = 48000.0;
    double m_phase = 0.25; // the triangle starts at the bottom of its sweep
    double m_position = -1.0; // the sweep position actually used, smoothed (0..1)
    double m_smoothing = 0.0;
    float m_feedback = 0.0f; // the copy's last sample, going round again
    Biquad m_chip;           // the wet path's treble loss
};

// Phaser: the classic one-knob four-stage phaser: four all-pass stages swept
// together by an LFO (their corner from about 140 Hz to 1.6 kHz, as a FET
// would move it), mixed half and half with the guitar for two moving notches.
// DEEP feeds the last stage back to the first, as the later version does,
// for stronger, more vocal peaks; SMOOTH leaves it out.
class Phaser final : public Pedal
{
public:
    enum Knob : int { Speed, Mode };
    static const PedalInfo kInfo;

    Phaser() : Pedal(kInfo) {}

    static double speedHz(float speed); // 0.1 .. 8 Hz

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    static constexpr int kStages = 4;
    std::array<double, kStages> m_state{};
    double m_rate = 48000.0;
    double m_phase = 0.0;
    double m_last = 0.0; // the last stage's output, for the feedback
};

} // namespace krunch
