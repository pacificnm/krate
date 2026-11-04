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
    m_head = nullptr; // (it may be gone by now: the next set() brings the playing channel's)
    reset();
}

void PowerAmp::reset() noexcept
{
    m_supply = 0.0;
    m_resonanceFilter.reset();
    m_voicing.reset();
    m_lowCut.reset();
    m_highCut.reset();
    m_peak.reset();
    m_oversampler.reset();
}

void PowerAmp::set(float master, float sag, float resonance, PowerTube tube, const HeadDesign *head) noexcept
{
    if (head && head->channelPower)
        head = nullptr; // the stepper's tubes: as without a head
    if (master == m_master && sag == m_sag && resonance == m_resonance && tube == m_tube && head == m_head)
        return;
    const bool rewire = head != m_head;
    m_master = master;
    m_sag = sag;
    m_resonance = resonance;
    m_tube = tube;
    m_head = head;
    m_design = head ? &head->power : &powerTubeDesign(tube);
    // Master: -30..+12 dB into the output pair (it clips from about its headroom).
    m_drive = std::pow(10.0, (-30.0 + 42.0 * std::clamp(double(master), 0.0, 1.0)) / 20.0);
    if (head) {
        m_drive *= std::pow(10.0, head->masterOffsetDb / 20.0);
        if (head->lowCutHz > 0.0)
            m_lowCut.setHighpass(m_rate, head->lowCutHz, 0.707);
        if (head->highCutHz > 0.0)
            m_highCut.setLowpass(m_rate, head->highCutHz, 0.707);
        m_peak.setPeak(m_rate, head->peakHz, head->peakQ, head->peakDb);
        m_output = std::pow(10.0, head->outputDb / 20.0);
        if (rewire) {
            m_lowCut.reset();
            m_highCut.reset();
            m_peak.reset();
        }
    }
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
        if (m_head) { // the head's output transformer and speaker pull, its level
            float v = m_peak.process(y);
            if (m_head->lowCutHz > 0.0)
                v = m_lowCut.process(v);
            if (m_head->highCutHz > 0.0)
                v = m_highCut.process(v);
            buffer[n] = float(double(v) * m_output);
        } else {
            buffer[n] = y;
        }
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
