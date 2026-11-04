#pragma once

#include "dsp/Filters.h"

namespace krunch {

// The built-in speaker, for a channel without cabinet IRs: a 4x12 in broad
// strokes (lows from 80 Hz with the cabinet's bump, a presence peak, steep top cut).
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
