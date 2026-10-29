#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "dsp/HalfbandDecimator.h"
#include "krimson/dsp/Params.h"
#include "krimson/dsp/Voice.h"

namespace krimson {

// Krimson's realtime core (docs/DSP.md). Four slots, each a full set of sound
// parameters; the IInstrument parameter table shows the slot being edited
// (docs/BRIEF.md#parameters-one-slot-at-a-time). C2: 16 voices with Poly,
// Mono, Legato and Unison, glide, sustain, pitch bend and the mod wheel
// (vibrato), at 2x and decimated, playing the edited slot. Each voice remembers
// its slot, so the slots can share the voices in C3.
class Engine final : public krate::IInstrument
{
public:
    Engine();

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override;
    std::span<const krate::ParamDescriptor> parameters() const noexcept override { return kParams; }
    // The edited slot's value for slot parameters, the instrument's for the rest.
    float param(int index) const noexcept override { return store(index).get(index); }
    void setParam(int index, float normalized) noexcept override { store(index).set(index, normalized); }
    void setParamFromHost(int index, float normalized) noexcept override { store(index).setFromAudio(index, normalized); }
    std::uint64_t takeParamsChangedByAudio() noexcept override;
    krate::MidiLearnMap *midiLearnMap() noexcept override { return &m_learn; }
    void process(const krate::AudioBlock &block) noexcept override;

    // Any thread: which slot (0..3 = A..D) the parameter table shows. Changing
    // it reports every slot parameter as changed, so the panel follows.
    void setEditedSlot(int slot) noexcept;
    int editedSlot() const noexcept { return m_edited.load(std::memory_order_relaxed); }

    // Any thread: a given slot's parameter (tests, slot files, the voices).
    float slotParam(int slot, ParamId id) const noexcept { return m_slots[std::size_t(slot)].get(int(id)); }
    void setSlotParam(int slot, ParamId id, float v) noexcept { m_slots[std::size_t(slot)].set(int(id), v); }
    float param(ParamId id) const noexcept { return param(int(id)); }
    void setParam(ParamId id, float v) noexcept { setParam(int(id), v); }

    // A slot's sound in DSP units.
    SlotSettings settings(int slot) const noexcept;

    static constexpr int kVoices = 16;
    static constexpr int kUnisonVoices = 3; // voices stacked per note in Unison
    enum class VoiceMode { Poly, Mono, Legato, Unison };

    // Inspection (tests).
    int activeVoices() const noexcept;
    bool voiceActive(int v) const noexcept { return m_voices[std::size_t(v)].active(); }
    bool voiceGate(int v) const noexcept { return m_voices[std::size_t(v)].gate(); }
    int voiceNote(int v) const noexcept { return m_voices[std::size_t(v)].note(); }
    double voiceLevel(int v) const noexcept { return m_voices[std::size_t(v)].level(); }

private:
    using Store = krate::ParamValues<ParamCount>;
    Store &store(int index) noexcept { return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global; }
    const Store &store(int index) const noexcept
    {
        return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global;
    }
    void handleMidi(const krate::MidiEvent &event) noexcept;
    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void setSustain(bool down) noexcept;
    void allNotesOff() noexcept;
    void render(float *left, float *right, std::uint32_t frames) noexcept;
    VoiceMode voiceMode(int slot) const noexcept;
    int pickVoice(int note) noexcept;        // Poly: a voice for the note (stealing if needed)
    void startVoice(int v, int note, double glideFrom) noexcept;
    void playMono(bool retrigger) noexcept;  // Mono / Legato / Unison: follow the top held key
    void releaseVoice(int v) noexcept;       // key up: release, or hold for the pedal

    static constexpr std::uint32_t kChunk = 128;

    std::array<Store, SlotCount> m_slots{Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams}};
    Store m_global{kParams};
    std::atomic<int> m_edited{0};
    std::atomic<std::uint64_t> m_forcedChanges{0};
    krate::MidiLearnMap m_learn{kParams};
    double m_sampleRate = 48000.0;

    std::array<Voice, kVoices> m_voices;
    std::array<int, kVoices> m_voiceSlot{};          // which slot each voice plays
    std::array<std::uint64_t, kVoices> m_started{};  // note-on order, for stealing
    std::array<bool, kVoices> m_sustained{};         // key up, held by the pedal
    std::uint64_t m_counter = 0;
    VoiceMode m_mode = VoiceMode::Poly;
    bool m_sustain = false;
    int m_lastNote = -1;  // for glide
    std::array<Lfo, SlotCount> m_lfo2;   // per slot
    Lfo m_vibrato;                       // the mod wheel's vibrato
    krate::HalfbandDecimator<31> m_decimatorL, m_decimatorR;
    std::array<float, 2 * kChunk> m_bufferL{}, m_bufferR{};
    std::array<std::array<float, kChunk>, SlotCount> m_lfo2Values{};
    std::array<float, kChunk> m_vibratoValues{};
    std::array<SlotSettings, SlotCount> m_settings{};
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;
    float m_lastVelocity = 1.0f;
    double m_bend = 0.0;  // -1..1
    double m_wheel = 0.0; // 0..1
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;
};

} // namespace krimson
