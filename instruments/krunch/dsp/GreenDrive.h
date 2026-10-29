#pragma once

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/Pedal.h"

namespace krunch {

// Green Drive: the classic green mid-boost overdrive (docs/PEDALS.md#green-drive).
// Modelled on its circuit in broad strokes: the clipping stage amplifies only
// what's above about 720 Hz (a high-pass in its feedback ground leg), clips it
// softly with diodes in the feedback loop and adds it to the clean signal, so
// the bass stays tight and the mids push; the stage's treble roll-off moves
// down as Drive goes up. Then a 720 Hz low-pass with the Tone control putting
// treble back, and Level. The clipping runs at 4x.
class GreenDrive final : public Pedal
{
public:
    enum Knob : int { Drive, Tone, Level };
    static const PedalInfo kInfo;

    GreenDrive() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    void retune(float drive) noexcept;

    double m_rate = 48000.0;
    float m_drive = -1.0f;
    double m_gain = 1.0;    // the clipping stage's gain on the high-passed signal
    OnePole m_input;        // the input coupling capacitor (high-pass)
    OnePole m_legHighpass;  // the feedback ground leg: only above ~720 Hz gets gain (at 4x)
    OnePole m_feedbackLowpass; // the feedback capacitor's treble roll-off (at 4x)
    Oversampler4x m_oversampler;
    OnePole m_toneLowpass;  // the fixed 720 Hz low-pass before the tone control
    float m_level = -1.0f;  // ramped output gain
};

} // namespace krunch
