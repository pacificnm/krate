#include "dsp/Preamp.h"

#include <algorithm>
#include <cmath>

namespace krunch {

namespace {

double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

} // namespace

void Preamp::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tuned = false;
    retune();
    reset();
}

void Preamp::reset() noexcept
{
    m_inputHighpass.reset();
    m_outCoupling.reset();
    m_oversampler.reset();
    for (TriodeStage &s : m_stages)
        s.reset();
    m_stack.reset();
    m_presence.reset();
    m_presenceCut.reset();
}

void Preamp::set(const Settings &settings) noexcept
{
    if (m_tuned && settings == m_settings)
        return;
    const bool rewire = !m_tuned || settings.voice != m_settings.voice || settings.head != m_settings.head
                        || settings.tube != m_settings.tube;
    m_settings = settings;
    retune();
    if (rewire) // new stages start from rest
        for (TriodeStage &s : m_stages)
            s.reset();
}

void Preamp::retune() noexcept
{
    const HeadDesign &d = m_settings.head ? *m_settings.head : builtInHead(m_settings.voice);
    const double fs = m_rate;
    const double fsUp = m_rate * Oversampler4x::kFactor;
    m_inputHighpass.setHighpass(fs, 20.0, 0.707);

    m_stageCount = d.stages;
    for (int i = 0; i < d.stages; ++i)
        m_stages[std::size_t(i)].prepare(fsUp, d.channelTube ? m_settings.tube : d.tube[std::size_t(i)], d.stage[std::size_t(i)]);
    // The Gain pot between the first two stages (log taper); fixed dividers after.
    const double gain = std::clamp(double(m_settings.gain), 0.0, 1.0);
    m_into[0] = kInputVolts;
    m_into[1] = dbToGain(d.potMinDb * (1.0 - gain)) * d.padIntoSecond;
    for (int i = 2; i < kMaxStages; ++i)
        m_into[std::size_t(i)] = d.divider[std::size_t(i)];
    // Out of the last stage (tens of volts) to about the level the tone stack had before.
    m_outScale = 1.0 / 60.0;
    m_outCoupling.setCutoff(fsUp, 30.0);

    m_stack.set(fs, d.stack, m_settings.bass, m_settings.mid, m_settings.treble);
    // Presence: -6..+9 dB above 3 kHz.
    m_presence.setHighShelf(fs, 3000.0, -6.0 + 15.0 * std::clamp(double(m_settings.presence), 0.0, 1.0));
    m_presenceCut.setLowpass(fs, d.topCutHz, 0.707);
    m_trim = float(dbToGain(d.trimDb));
    m_tuned = true;
}

void Preamp::process(float *buffer, std::uint32_t frames) noexcept
{
    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        m_oversampler.up(m_inputHighpass.process(buffer[n]), up);
        for (float &u : up) {
            double v = double(u);
            for (int i = 0; i < m_stageCount; ++i)
                v = m_stages[std::size_t(i)].process(v * m_into[std::size_t(i)]);
            u = m_outCoupling.highpass(float(v * m_outScale));
        }
        float y = m_oversampler.down(up);
        y = m_presenceCut.process(m_presence.process(m_stack.process(y)));
        buffer[n] = y * m_trim;
    }
}

} // namespace krunch
