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
// (docs/BRIEF.md#parameters-one-slot-at-a-time). C1: one voice playing the
// edited slot's sound, last-note priority, at 2x and decimated; pitch bend and
// the mod wheel (vibrato). Polyphony arrives in C2, the slots playing in C3.
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
    void render(float *out, std::uint32_t frames, const SlotSettings &s) noexcept;

    static constexpr std::uint32_t kChunk = 128;

    std::array<Store, SlotCount> m_slots{Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams}};
    Store m_global{kParams};
    std::atomic<int> m_edited{0};
    std::atomic<std::uint64_t> m_forcedChanges{0};
    krate::MidiLearnMap m_learn{kParams};
    double m_sampleRate = 48000.0;

    Voice m_voice;
    Lfo m_lfo2;     // per slot from C3; slot A's for now
    Lfo m_vibrato;  // the mod wheel's vibrato
    krate::HalfbandDecimator<31> m_decimator;
    std::array<float, 2 * kChunk> m_buffer2x{};
    std::array<float, kChunk> m_lfo2Values{}, m_vibratoValues{};
    std::array<std::uint8_t, 16> m_held{};
    int m_heldCount = 0;
    float m_lastVelocity = 1.0f;
    double m_bend = 0.0;  // -1..1
    double m_wheel = 0.0; // 0..1
    double m_volume = -1.0;
    double m_volumeSmoothing = 0.0;
};

} // namespace krimson
