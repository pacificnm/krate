#include "dsp/Preamp.h"

#include <algorithm>
#include <cmath>

namespace krunch {

namespace {

double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

// The shape every stage clips with: smooth, with a bias for asymmetry.
float clip(double x) { return float(std::tanh(x)); }

struct VoiceDesign
{
    int stages;
    double preHighpassHz; // into the first stage
    double preMidHz, preMidDb;
    double firstGainMinDb, firstGainMaxDb; // the Gain knob's range on the first stage
    double laterGainDb;                    // each later stage
    double bias;
    double couplingHz; // between stages
    double rolloffHz;  // each stage's treble roll-off
    ToneStack::Parts stack;
    double trimDb;
};

// Clean: one gentle stage, the scooped stack. Crunch: two stages. Heavy: three,
// with the bass tightened and the mids pushed going in (tight, for rhythm).
// Lead: four, smoother and more mids, for sustain.
const VoiceDesign &design(Voice voice)
{
    static const VoiceDesign kDesigns[int(Voice::Count)] = {
        {1, 30.0, 800.0, 0.0, -6.0, 18.0, 0.0, 0.08, 20.0, 14000.0, ToneStack::kAmerican, 20.0},
        {2, 60.0, 900.0, 2.0, 0.0, 30.0, 12.0, 0.18, 40.0, 9000.0, ToneStack::kBritish, 3.0},
        {3, 140.0, 850.0, 5.0, 6.0, 40.0, 14.0, 0.22, 70.0, 7500.0, ToneStack::kBritish, 0.0},
        {4, 100.0, 750.0, 6.0, 6.0, 42.0, 12.0, 0.18, 50.0, 6000.0, ToneStack::kBritish, -1.0},
    };
    return kDesigns[std::clamp(int(voice), 0, int(Voice::Count) - 1)];
}

} // namespace

float Preamp::Stage::process(float x) noexcept
{
    const float y = clip(gain * x + bias) - float(offset);
    return rolloff.lowpass(coupling.highpass(y));
}

void Preamp::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate;
    m_tuned = false;
    retune();
    reset();
}

void Preamp::reset() noexcept
{
    m_preHighpass.reset();
    m_preMid.reset();
    m_oversampler.reset();
    for (auto &s : m_stages) {
        s.coupling.reset();
        s.rolloff.reset();
    }
    m_stack.reset();
    m_presence.reset();
    m_presenceCut.reset();
}

void Preamp::set(const Settings &settings) noexcept
{
    if (m_tuned && settings == m_settings)
        return;
    m_settings = settings;
    retune();
}

void Preamp::retune() noexcept
{
    const VoiceDesign &d = design(m_settings.voice);
    const double fs = m_rate;
    const double fsUp = m_rate * Oversampler4x::kFactor;

    m_preHighpass.setHighpass(fs, d.preHighpassHz, 0.707);
    m_preMid.setPeak(fs, d.preMidHz, 0.8, d.preMidDb);

    m_stageCount = d.stages;
    const double gain = std::clamp(double(m_settings.gain), 0.0, 1.0);
    for (int i = 0; i < kMaxStages; ++i) {
        Stage &s = m_stages[std::size_t(i)];
        const double db = i == 0 ? d.firstGainMinDb + gain * (d.firstGainMaxDb - d.firstGainMinDb) : d.laterGainDb;
        s.gain = dbToGain(db);
        s.bias = i == 0 ? d.bias : d.bias * 0.5;
        s.offset = clip(s.bias);
        s.coupling.setCutoff(fsUp, d.couplingHz);
        s.rolloff.setCutoff(fsUp, d.rolloffHz);
    }

    m_stack.set(fs, d.stack, m_settings.bass, m_settings.mid, m_settings.treble);
    // Presence: -6..+9 dB above 3 kHz.
    m_presence.setHighShelf(fs, 3000.0, -6.0 + 15.0 * std::clamp(double(m_settings.presence), 0.0, 1.0));
    m_presenceCut.setLowpass(fs, 11000.0, 0.707);
    // More gain is louder going into the clippers; the clippers cap it, so the
    // trim only evens the voices out (and the clean voice's knob stays a volume).
    m_trim = float(dbToGain(d.trimDb));
    m_tuned = true;
}

void Preamp::process(float *buffer, std::uint32_t frames) noexcept
{
    float up[Oversampler4x::kFactor];
    for (std::uint32_t n = 0; n < frames; ++n) {
        const float x = m_preMid.process(m_preHighpass.process(buffer[n]));
        m_oversampler.up(x, up);
        for (float &u : up)
            for (int i = 0; i < m_stageCount; ++i)
                u = m_stages[std::size_t(i)].process(u);
        float y = m_oversampler.down(up);
        y = m_presenceCut.process(m_presence.process(m_stack.process(y)));
        buffer[n] = y * m_trim;
    }
}

} // namespace krunch
