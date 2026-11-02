#include "dsp/PowerAmp.h"

#include <algorithm>
#include <cmath>

namespace krunch {

void PowerAmp::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    // The supply follows how hard you play (over tens of milliseconds), not each wave.
    m_attack = std::exp(-1.0 / (0.020 * sampleRate));
    m_release = std::exp(-1.0 / (0.200 * sampleRate));
    m_master = m_sag = m_resonance = -1.0f; // retune on the next set()
    m_tube = PowerTube::Count;
    reset();
}

void PowerAmp::reset() noexcept
{
    m_supply = 0.0;
    m_resonanceFilter.reset();
    m_voicing.reset();
    m_oversampler.reset();
}

void PowerAmp::set(float master, float sag, float resonance, PowerTube tube) noexcept
{
    if (master == m_master && sag == m_sag && resonance == m_resonance && tube == m_tube)
        return;
    m_master = master;
    m_sag = sag;
    m_resonance = resonance;
    m_tube = tube;
    m_design = &powerTubeDesign(tube);
    // Master: -30..+12 dB into the output pair (it clips from about its headroom).
    m_drive = std::pow(10.0, (-30.0 + 42.0 * std::clamp(double(master), 0.0, 1.0)) / 20.0);
    // Sag: the knob scales the tubes' own droop.
    m_sagAmount = 1.2 * std::clamp(double(sag), 0.0, 1.0);
    m_resonanceFilter.setPeak(m_rate, 100.0, 0.9, 10.0 * std::clamp(double(resonance), 0.0, 1.0));
    m_voicing.setPeak(m_rate, m_design->voiceHz, m_design->voiceQ, m_design->voiceDb);
}

void PowerAmp::process(float *buffer, std::uint32_t frames) noexcept
{
    float up[Oversampler4x::kFactor];
    const PowerTubeDesign &d = *m_design;
    for (std::uint32_t n = 0; n < frames; ++n) {
        const double x = m_voicing.process(m_resonanceFilter.process(buffer[n]));
        const double droop = m_sagAmount * m_supply;
        m_oversampler.up(float(x * m_drive), up);
        for (float &u : up)
            u = float(pushPull(d, double(u), droop));
        const float y = m_oversampler.down(up);
        const double load = std::fabs(y) / d.headroom;
        m_supply = load > m_supply ? load + m_attack * (m_supply - load) : load + m_release * (m_supply - load);
        buffer[n] = y;
    }
}

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
