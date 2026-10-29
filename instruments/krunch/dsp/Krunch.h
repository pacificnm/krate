#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "core/Instrument.h"
#include "core/ParamValues.h"

namespace krunch {

// Krunch: a guitar amp for the Krate rack (docs/ROADMAP.md). It plays the
// rack's audio inputs (a guitar through the audio interface) rather than
// notes. G0: the input it plays, straight through at the Level knob, with
// input and output peaks for the panel's meters.
enum class ParamId : int { Input, Level, Count };
inline constexpr int ParamCount = int(ParamId::Count);

namespace format {
std::string input(float v);
std::string level(float v);
} // namespace format

inline constexpr std::array<krate::ParamDescriptor, ParamCount> kParams{{
    {"input", "Input", 0.0f, 2, &format::input},       // the interface's input 1 or 2
    {"level", "Level", 60.0f / 72.0f, 0, &format::level}, // -60..+12 dB, default 0 dB
}};

namespace units {
inline constexpr int kInputs = 2;
inline constexpr double kLevelMinDb = -60.0;
inline constexpr double kLevelMaxDb = 12.0;
int input(float v);        // 0 or 1
double levelDb(float v);   // kLevelMinDb..kLevelMaxDb; the bottom is silence
double levelGain(float v); // 0 at the bottom
} // namespace units

class Krunch final : public krate::IInstrument
{
public:
    Krunch() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    int inputChannels() const noexcept override { return units::kInputs; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    void process(const krate::AudioBlock &block) noexcept override;

    // Peaks since the last call (any thread): the chosen input as it arrives,
    // and the amp's output.
    float takeInputPeak() noexcept { return m_inputPeak.exchange(0.0f, std::memory_order_relaxed); }
    float takeOutputPeak() noexcept { return m_outputPeak.exchange(0.0f, std::memory_order_relaxed); }

private:
    krate::ParamValues<ParamCount> m_params{kParams};
    double m_sampleRate = 48000.0;
    float m_gain = -1.0f; // current (ramped) output gain; < 0 until the first block

    std::atomic<float> m_inputPeak{0.0f};
    std::atomic<float> m_outputPeak{0.0f};
};

} // namespace krunch
