#pragma once

#include "dsp/Filters.h"
#include "dsp/Pedal.h"

namespace krunch {

// Boost: a clean boost (docs/PEDALS.md#boost). BOOST raises the level 0 ...
// +20 dB with no colour of its own; BRIGHT adds a little sparkle (+4 dB above
// about 3 kHz). In front of the amp it drives the capture harder; in the
// loop, a louder solo.
class Boost final : public Pedal
{
public:
    enum Knob : int { Amount, Bright };
    static const PedalInfo kInfo;
    static constexpr double kMaxDb = 20.0;
    static constexpr double kBrightDb = 4.0;

    Boost() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    double m_rate = 48000.0;
    OnePole m_input;     // the input coupling capacitor
    Biquad m_bright;     // a high shelf, flat with BRIGHT off
    float m_brightOn = -1.0f;
    float m_gain = -1.0f; // ramped
};

} // namespace krunch
