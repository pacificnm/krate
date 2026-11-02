#pragma once

#include "dsp/Pedal.h"

namespace krunch {

// Dual Head (docs/PEDALS.md#dual-head): two amps set apart. The amp's sound,
// after its cabinets, plays as the left amp; the right is a copy, DELAY
// later (0-40 ms: a few ms wide, up to two amps across a room), voiced by
// TONE (darker or brighter than the first) at BALANCE. With SPACE on the
// cabinets, each amp has its own cabinet. The pedal passes the signal
// through where it sits; Krunch does the split at its output (where it's stereo).
class DualHead final : public Pedal
{
public:
    enum Knob : int { Delay, Tone, Balance };
    static const PedalInfo kInfo;

    DualHead() : Pedal(kInfo) {}

    static double delayMs(float v);   // 0..40 ms
    static double tilt(float v);      // -0.7 darker .. +0.7 brighter (0 at noon)
    static double balanceDb(float v); // the second amp, -12..0 dB

protected:
    void prepareEffect(double) noexcept override {}
    void resetEffect() noexcept override {}
    void processEffect(float *, std::uint32_t) noexcept override {} // through, as it is
};

// The split itself, at the amp's stereo output (Krunch). Audio thread; no allocation.
class DualHeadSplit
{
public:
    static constexpr int kMaxDelay = 8192; // samples: 40 ms up to 192 kHz

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // pedal: the board's Dual Head that's on, or nullptr (then it fades out).
    void process(const DualHead *pedal, float *left, float *right, std::uint32_t frames) noexcept;

private:
    double m_rate = 48000.0;
    float m_buffer[kMaxDelay] = {};
    int m_write = 0;
    double m_delay = -1.0; // samples, gliding to the knob's
    double m_glide = 0.0, m_lowA = 0.0, m_low = 0.0;
    double m_mix = 0.0, m_mixStep = 0.0;
};

} // namespace krunch
