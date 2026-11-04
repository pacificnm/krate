#include "dsp/Speaker.h"

namespace krunch {

void SpeakerFilter::prepare(double fs) noexcept
{
    m_sections[0].setHighpass(fs, 75.0, 0.8);
    m_sections[1].setPeak(fs, 110.0, 1.4, 3.0);   // cabinet resonance
    m_sections[2].setPeak(fs, 450.0, 1.0, -2.5);  // a little lower-mid dip
    m_sections[3].setPeak(fs, 2400.0, 1.2, 4.0);  // the cone's presence peak
    m_sections[4].setLowpass(fs, 5000.0, 0.75);   // the steep top end
    m_sections[5].setLowpass(fs, 6000.0, 0.6);
    reset();
}

void SpeakerFilter::reset() noexcept
{
    for (Biquad &b : m_sections)
        b.reset();
}

} // namespace krunch
