#pragma once

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/Pedal.h"

namespace krunch {

// Round Fuzz: the classic two-transistor round fuzz, in broad strokes
// (docs/PEDALS.md#round-fuzz). Two transistor stages, each a gain into an
// asymmetric clipper (the transistor saturates one way and cuts off the
// other), at 4x; FUZZ sets how hard the first is driven, so it cleans up as
// you pick lighter or roll the guitar's volume back. BIAS moves the
// transistors' operating point: off centre, quiet notes get stuck and
// sputter (the "dying battery" gate). TYPE: Germanium (softer curve, less
// gain, darker) or Silicon (harder, more gain, brighter).
class RoundFuzz final : public Pedal
{
public:
    enum Knob : int { Fuzz, Volume, Bias, Type };
    static const PedalInfo kInfo;

    RoundFuzz() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    void retune(bool silicon) noexcept;

    double m_rate = 48000.0;
    int m_type = -1;          // the curves and filters are set for this (0 germanium, 1 silicon)
    OnePole m_input;          // the input coupling capacitor
    OnePole m_between;        // between the stages (at 4x)
    OnePole m_rolloff;        // the stages' treble loss (at 4x)
    Oversampler4x m_oversampler;
    OnePole m_output;         // the output coupling capacitor
    float m_volume = -1.0f;   // ramped output gain
};

} // namespace krunch
