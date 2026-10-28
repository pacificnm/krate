#include "dsp/Klick.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace klick {

namespace {
constexpr double kDecaySeconds = 0.015;  // envelope time constant: a short tick
constexpr double kAccentPitch = 1.5;     // bar clicks sound a fifth higher
constexpr double kEpsilon = 1e-9;        // absorbs beat-arithmetic rounding (as krate::StepClock)
} // namespace

namespace units {
double gain(float level) { return double(level) * level; }
double pitchHz(float v) { return kPitchMinHz * std::pow(kPitchMaxHz / kPitchMinHz, v); }
int beatsPerBar(float v) { return 2 + int(std::lround(double(v) * 5.0)); }
} // namespace units

namespace format {
std::string level(float v)
{
    const double g = units::gain(v);
    return g <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(g));
}
std::string pitch(float v) { return std::format("{} Hz", std::lround(units::pitchHz(v))); }
std::string accent(float v) { return std::format("{}%", std::lround(v * 100.0f)); }
std::string beats(float v) { return std::format("{}/4", units::beatsPerBar(v)); }
} // namespace format

void Klick::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
    m_decay = std::exp(-1.0 / (kDecaySeconds * sampleRate));
}

void Klick::trigger(bool accented) noexcept
{
    const double accent = m_params.get(int(ParamId::Accent));
    const double hz = units::pitchHz(m_params.get(int(ParamId::Pitch))) * (accented ? 1.0 + (kAccentPitch - 1.0) * accent : 1.0);
    m_increment = std::min(hz / m_sampleRate, 0.45);
    m_phase = 0.0;
    m_envelope = accented ? 1.0 : 1.0 - 0.5 * accent; // the bar click stands out by the Accent amount
}

void Klick::process(const krate::AudioBlock &block) noexcept
{
    float *outL = block.outputs[0];
    float *outR = block.outputs[1];
    static constexpr krate::TransportState kStopped{};
    const krate::TransportState &t = block.transport ? *block.transport : kStopped;

    if (t.playing && (t.restarted || !m_wasPlaying))
        m_lastBeat = -1; // the first beat of a fresh start clicks
    m_wasPlaying = t.playing;

    const double gain = units::gain(m_params.get(int(ParamId::Level)));
    const int beatsPerBar = units::beatsPerBar(m_params.get(int(ParamId::Beats)));
    const double beatsPerSample = t.bpm / 60.0 / m_sampleRate;

    for (std::uint32_t i = 0; i < block.frames; ++i) {
        if (t.playing) {
            const double beat = t.beatAtBlockStart + double(i) * beatsPerSample;
            const auto index = std::int64_t(std::floor(beat + kEpsilon));
            if (index > m_lastBeat) {
                m_lastBeat = index;
                trigger(index % beatsPerBar == 0);
            }
        }
        const double s = std::sin(2.0 * std::numbers::pi * m_phase) * m_envelope * gain;
        m_phase += m_increment;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
        m_envelope *= m_decay;
        outL[i] = float(s);
        outR[i] = float(s);
    }
}

} // namespace klick
