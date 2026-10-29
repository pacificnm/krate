#pragma once

#include <cstdint>

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"

namespace krunch {

// The power amp (docs/DSP.md#power-amp): Master drives a push-pull output
// stage (symmetric soft clipping at 4x) whose supply sags under load (it
// compresses as you dig in, over 20 ms in and 200 ms out), with Resonance lifting the lows the speaker
// pushes back. Mono, in place.
class PowerAmp
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // Controls 0..1, between blocks.
    void set(float master, float sag, float resonance) noexcept;
    void process(float *buffer, std::uint32_t frames) noexcept;

private:
    double m_rate = 48000.0;
    float m_master = -1.0f, m_sag = -1.0f, m_resonance = -1.0f;
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
