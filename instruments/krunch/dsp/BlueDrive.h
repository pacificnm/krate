#pragma once

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/Pedal.h"

namespace krunch {

// Blue Drive: the classic blue amp-like overdrive (docs/PEDALS.md#blue-drive).
// Unlike the green and yellow drives there's no mid hump: the whole range
// (from about 100 Hz up) gets the gain, so it sounds like a small amp turned
// up rather than a boosted one. Modelled on its circuit in broad strokes: two
// discrete gain stages in a row, both turned up together by GAIN (a dual-gang
// pot), each clipped softly in its feedback and then harder by diodes to
// ground, the transistors leaving a slight lean to one side (a few even
// harmonics). Played softly it cleans up; dug in it breaks up. Then a tone
// control that tilts (darker and fuller, or brighter and leaner), and Level.
// The stages run at 4x.
class BlueDrive final : public Pedal
{
public:
    enum Knob : int { Level, Tone, Gain };
    static const PedalInfo kInfo;

    BlueDrive() : Pedal(kInfo) {}

protected:
    void prepareEffect(double sampleRate) noexcept override;
    void resetEffect() noexcept override;
    void processEffect(float *buffer, std::uint32_t frames) noexcept override;

private:
    struct Stage
    {
        OnePole coupling; // the coupling capacitor into the stage (high-pass)
        OnePole rolloff;  // its treble roll-off
        float process(float x, double gain, double bias) noexcept;
        void reset() noexcept { coupling.reset(); rolloff.reset(); }
    };
    void retune(float gain, float tone) noexcept;

    double m_rate = 48000.0;
    float m_gainKnob = -1.0f, m_toneKnob = -1.0f;
    double m_gain = 1.0; // each stage's
    OnePole m_input;
    Stage m_first, m_second; // at 4x
    Oversampler4x m_oversampler;
    OnePole m_output;        // the output coupling capacitor (takes the bias's DC off)
    Biquad m_toneLow, m_toneHigh;
    float m_level = -1.0f;   // ramped output gain
};

} // namespace krunch
