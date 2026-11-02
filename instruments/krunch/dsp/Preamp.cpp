#include "dsp/Preamp.h"

#include <algorithm>
#include <cmath>

namespace krunch {

namespace {

double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

using Stage = TriodeStage::Design;

struct VoiceDesign
{
    int stages;
    Stage stage[Preamp::kMaxStages];
    double divider[Preamp::kMaxStages]; // into each stage after the second (the Gain pot is into the second)
    double potMinDb;                    // the Gain pot's range (log taper, up to 0 dB)
    ToneStack::Parts stack;
    double trimDb;
};

// Each voice wired as its kind of amp is. A stage: {cathode ohms, bypass Hz
// (below it the cathode isn't bypassed: less gain), coupling capacitor Hz in
// front, treble roll-off Hz}.
// - Clean: two stages, fully bypassed, big coupling caps, the tone stack's
//   loss between them; it only breaks up with the Gain high.
// - Crunch: a partly bypassed first stage (less bass into the drive), the
//   Gain pot, a cold-biased clipper (unbypassed 10 k: asymmetric), a third.
// - Heavy: four stages; small coupling caps and partial bypasses at every one
//   (each clips only from the low mids up: tight palm mutes, no mud), the
//   cold clipper second, more treble rolled off as it goes (no fizz).
// - Lead: four stages, more bass let through and rounder (bigger caps, lower
//   roll-offs): thick, sustaining.
const VoiceDesign &design(Voice voice)
{
    static const VoiceDesign kDesigns[int(Voice::Count)] = {
        {2, {{1500, 4, 25, 14000}, {1500, 4, 20, 11000}, {}, {}}, {0, 0, 0, 0}, -32.0, ToneStack::kAmerican, 20.0},
        {3, {{2700, 40, 8, 12000}, {10000, 1500, 20, 6000}, {1500, 50, 25, 3500}, {}}, {0, 0, 0.45, 0}, -30.0,
         ToneStack::kBritish, 0.0},
        {4, {{1500, 60, 15, 12000}, {10000, 1500, 40, 6000}, {1500, 60, 30, 3500}, {1500, 60, 30, 3000}},
         {0, 0, 0.22, 0.25}, -40.0, ToneStack::kBritish, 0.0},
        {4, {{1500, 40, 10, 11000}, {2700, 100, 30, 6000}, {1500, 70, 30, 3800}, {1500, 100, 40, 3200}},
         {0, 0, 0.25, 0.2}, -38.0, ToneStack::kBritish, 0.0},
    };
    return kDesigns[std::clamp(int(voice), 0, int(Voice::Count) - 1)];
}

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
    const bool rewire = !m_tuned || settings.voice != m_settings.voice || settings.tube != m_settings.tube;
    m_settings = settings;
    retune();
    if (rewire) // new stages start from rest
        for (TriodeStage &s : m_stages)
            s.reset();
}

void Preamp::retune() noexcept
{
    const VoiceDesign &d = design(m_settings.voice);
    const double fs = m_rate;
    const double fsUp = m_rate * Oversampler4x::kFactor;
    m_inputHighpass.setHighpass(fs, 20.0, 0.707);

    m_stageCount = d.stages;
    for (int i = 0; i < d.stages; ++i)
        m_stages[std::size_t(i)].prepare(fsUp, m_settings.tube, d.stage[i]);
    // The Gain pot between the first two stages (log taper); fixed dividers after.
    const double gain = std::clamp(double(m_settings.gain), 0.0, 1.0);
    m_into[0] = kInputVolts;
    m_into[1] = dbToGain(d.potMinDb * (1.0 - gain)) * (m_settings.voice == Voice::Clean ? 0.12 : 1.0); // clean: the tone stack's loss
    for (int i = 2; i < kMaxStages; ++i)
        m_into[std::size_t(i)] = d.divider[i];
    // Out of the last stage (tens of volts) to about the level the tone stack had before.
    m_outScale = 1.0 / 60.0;
    m_outCoupling.setCutoff(fsUp, 30.0);

    m_stack.set(fs, d.stack, m_settings.bass, m_settings.mid, m_settings.treble);
    // Presence: -6..+9 dB above 3 kHz.
    m_presence.setHighShelf(fs, 3000.0, -6.0 + 15.0 * std::clamp(double(m_settings.presence), 0.0, 1.0));
    m_presenceCut.setLowpass(fs, 11000.0, 0.707);
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
