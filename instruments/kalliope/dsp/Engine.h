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

// Kalliope's realtime core (docs/DSP.md): eight voices at 2x, decimated once;
// Poly, Unison and Solo key modes (docs/PLAY.md#key-modes), sustain pedal,
// pitch bend, mod wheel, MIDI Learn. Effects arrive in P4.
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

    static constexpr int kVoices = 8;
    enum class KeyMode { Poly, Unison, Solo };

    // Inspection (tests): what each voice is doing.
    bool voiceActive(int v) const noexcept { return m_voices[std::size_t(v)].active(); }
    bool voiceGate(int v) const noexcept { return m_voices[std::size_t(v)].gate(); }
    int voiceNote(int v) const noexcept { return m_voices[std::size_t(v)].note(); }
    double voiceLevel(int v) const noexcept { return m_voices[std::size_t(v)].level(); }
    int activeVoices() const noexcept;

private:
    static constexpr std::uint32_t kChunk = 128; // frames rendered at a time (fixed buffers)

    void handleMidi(const krate::MidiEvent &event) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void setSustain(bool down) noexcept;
    void allNotesOff() noexcept;
    void render(float *out, std::uint32_t frames, const VoiceSettings &s) noexcept;
    KeyMode keyMode() const noexcept;
    void applyKeyMode(KeyMode mode) noexcept;
    int pickVoice(int note) noexcept; // Poly: which voice plays `note`
    void playMono(bool retrigger) noexcept; // Unison/Solo: the voices follow the top held key
    void releaseVoice(int v) noexcept;      // key up: release, or hold for the pedal

    krate::ParamValues<ParamCount> m_params{kParams};
    krate::MidiLearnMap m_learn{kParams}; // parameters are ParamId values
    double m_sampleRate = 48000.0;

    std::array<Voice, kVoices> m_voices;
    std::array<std::uint64_t, kVoices> m_started{};  // note-on order, for stealing
    std::array<bool, kVoices> m_sustained{};         // key up, held by the pedal
    std::uint64_t m_counter = 0;
    KeyMode m_mode = KeyMode::Poly;
    bool m_sustain = false;
    std::uint32_t m_phaseSeed = 0x9E3779B9u; // start phases of new Poly notes (deterministic)
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
