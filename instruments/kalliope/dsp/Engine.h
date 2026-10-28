#pragma once

#include <array>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "dsp/HalfbandDecimator.h"
#include "kalliope/dsp/Lfo.h"
#include "kalliope/dsp/Params.h"
#include "kalliope/dsp/Voice.h"

namespace kalliope {

// Kalliope's realtime core (docs/DSP.md). P1: one voice, played by MIDI with
// last-note priority, at 2x and decimated; pitch bend, mod wheel, MIDI Learn.
// Polyphony arrives in P2, effects in P4.
class Engine final : public krate::IInstrument
{
public:
    Engine() = default;

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    float param(int index) const noexcept override { return m_params.get(index); }
    void setParam(int index, float normalized) noexcept override { m_params.set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { m_params.setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override { return m_params.takeChanged(); }
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    float param(ParamId id) const noexcept { return m_params.get(int(id)); }
    void setParam(ParamId id, float normalized) noexcept { m_params.set(int(id), normalized); }

    // The panel's settings in DSP units (what the voices play with).
    VoiceSettings settings() const noexcept;

private:
    static constexpr std::uint32_t kChunk = 128; // frames rendered at a time (fixed buffers)

    void handleMidi(const krate::MidiEvent &event) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void render(float *out, std::uint32_t frames, const VoiceSettings &s) noexcept;

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    double m_sampleRate = 48000.0;

    Voice m_voice;
    Lfo m_lfo;
    krate::HalfbandDecimator<31> m_decimator;
    std::array<float, 2 * kChunk> m_buffer2x{};
    std::array<float, kChunk> m_lfoValues{};

    // Held keys, oldest first (last-note priority).
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;
    float m_lastVelocity = 1.0f;
    double m_bend = 0.0;  // -1..1
    double m_wheel = 0.0; // 0..1
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;
};

} // namespace kalliope
