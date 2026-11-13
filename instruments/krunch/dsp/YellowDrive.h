#pragma once

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/Pedal.h"

namespace krunch {

// Yellow Drive: the classic yellow overdrive (docs/PEDALS.md#yellow-drive), the
// green one's brighter, edgier cousin. Modelled on its circuit in broad
// strokes: the same idea of a clipping stage that amplifies only what's above
// about 720 Hz and adds it to the clean signal, but with more gain on tap (a
// 1 M Drive pot) and asymmetric clipping (two diodes one way, one the other),
// which brings even harmonics and a grittier, more open sound; then a wider
// tone control that can go past flat into bright, and Level. The clipping runs
// at 4x.
class YellowDrive final : public Pedal
{
public:
    enum Knob : int { Drive, Tone, Level };
    static const PedalInfo kInfo;

    YellowDrive() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    void retune(float drive) noexcept;

    double m_rate = 48000.0;
    float m_drive = -1.0f;
    double m_gain = 1.0;       // the clipping stage's gain on the high-passed signal
    OnePole m_input;           // the input coupling capacitor (high-pass)
    OnePole m_legHighpass;     // the feedback ground leg: only above ~720 Hz gets gain (at 4x)
    OnePole m_feedbackLowpass; // the feedback capacitor's treble roll-off (at 4x)
    Oversampler4x m_oversampler;
    OnePole m_toneLowpass;     // the fixed low-pass the tone control blends against
    OnePole m_output;          // the output coupling capacitor: takes off the DC the asymmetric clipping makes
    float m_level = -1.0f;     // ramped output gain
};

} // namespace krunch
