#pragma once

#include <cstdint>

#include "dsp/Filters.h"
#include "dsp/Head.h"
#include "dsp/Oversampler.h"
#include "dsp/Tube.h"

namespace krunch {

// The power amp (docs/DSP.md#power-amp): Master drives a push-pull pair of
// the chosen power tubes (dsp/Tube.h: their headroom, knee, crossover and
// voicing, at 4x) whose supply sags under load (it compresses as you dig in,
// over 20 ms in and 200 ms out), with Resonance lifting the lows the speaker
// pushes back. Mono, in place.
class PowerAmp
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // Controls 0..1, between blocks. A head with power tubes of its own
    // (not the stepper's) brings them, how hard Master drives them, and its
    // output voicing and level.
    void set(float master, float sag, float resonance, PowerTube tube = PowerTube::BritishPentode,
             const HeadDesign *head = nullptr) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    double m_rate = 48000.0;
    float m_master = -1.0f, m_sag = -1.0f, m_resonance = -1.0f;
    PowerTube m_tube = PowerTube::Count;
    const HeadDesign *m_head = nullptr; // with its own power tubes, or none
    const PowerTubeDesign *m_design = nullptr;
    Biquad m_lowCut, m_highCut, m_peak; // the head's output voicing
    double m_output = 1.0;
    Biquad m_voicing; // the tube's own voicing
    double m_drive = 1.0;
    double m_sagAmount = 0.0;
    double m_attack = 0.0, m_release = 0.0; // sag follower coefficients
    double m_supply = 0.0;                  // load on the supply, 0..
    Biquad m_resonanceFilter;
    Oversampler4x m_oversampler;
};

// Until cabinets (G2): a fixed speaker response, a 4x12 in broad strokes
// (lows from 80 Hz with the cabinet's bump, a presence peak, steep top cut).
class SpeakerFilter
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    float process(float x) noexcept
    {
        for (Biquad &b : m_sections)
            x = b.process(x);
        return x;
    }

private:
    Biquad m_sections[6];
};

} // namespace krunch
