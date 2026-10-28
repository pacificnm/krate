#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "core/Instrument.h"
#include "core/ParamValues.h"

namespace klick {

// Klick: a metronome instrument for the Krate rack. Clicks on every beat of the
// transport, accents the first beat of each bar. Silent while stopped.
enum class ParamId : int { Level, Pitch, Accent, Beats, Count };
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string level(float v);
std::string pitch(float v);
std::string accent(float v);
std::string beats(float v);
} // namespace format

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"level", "Level", 0.7f, 0, &format::level},
    {"pitch", "Pitch", 0.5f, 0, &format::pitch},
    {"accent", "Accent", 0.6f, 0, &format::accent},
    {"beats", "Beats", 0.4f, 6, &format::beats}, // 2..7 beats per bar; default 4
}};

namespace units {
inline constexpr double kPitchMinHz = 600.0;
inline constexpr double kPitchMaxHz = 3000.0;
double gain(float level);         // level^2
double pitchHz(float v);          // exponential 600..3000 Hz
int beatsPerBar(float v);         // 2..7
} // namespace units

class Klick final : public krate::IInstrument
{
public:
    Klick() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    void process(const krate::AudioBlock &block) noexcept override;

private:
    void trigger(bool accented) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    double m_sampleRate = 48000.0;
    double m_decay = 0.0;          // per-sample envelope multiplier

    // Realtime thread only.
    bool m_wasPlaying = false;
    std::int64_t m_lastBeat = -1;  // index of the last beat that clicked
    double m_phase = 0.0;          // sine phase, 0..1
    double m_increment = 0.0;      // phase per sample
    double m_envelope = 0.0;
};

} // namespace klick
