#pragma once

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/Pedal.h"

namespace krunch {

// Taiga Fuzz: the big four-transistor sustaining fuzz in its green,
// Russian-made voicing (docs/PEDALS.md#taiga-fuzz): bassier and smoother than
// the original, with a shallower mid scoop. Modelled on the circuit in broad
// strokes: an input booster, two cascaded clipping stages (a gain into two
// diodes each way, the feedback capacitor rolling the treble off; SUSTAIN sets
// the first one's gain), the passive tone stack (a low-pass blended against a
// high-pass, which scoops the mids at noon), and the recovery stage into
// VOLUME. The large coupling capacitors keep the low end. The three gain
// stages run at 4x.
class TaigaFuzz final : public Pedal
{
public:
    enum Knob : int { Volume, Tone, Sustain };
    static const PedalInfo kInfo;

    TaigaFuzz() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    OnePole m_input;               // the input coupling capacitor
    OnePole m_couple1, m_couple2;  // between the stages (at 4x)
    OnePole m_rolloff1, m_rolloff2; // the clipping stages' feedback capacitors (at 4x)
    Oversampler4x m_oversampler;
    OnePole m_toneLow, m_toneHigh; // the tone stack's low-pass, and the one its high-pass is the rest of
    OnePole m_output;              // the output coupling capacitor
    float m_level = -1.0f;         // ramped output gain
};

} // namespace krunch
