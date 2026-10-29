#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "core/Instrument.h"
#include "core/Midi.h"
#include "core/ParamValues.h"
#include "krimson/dsp/Params.h"

namespace krimson {

// Krimson's realtime core (docs/DSP.md). Four slots, each a full set of sound
// parameters; the IInstrument parameter table shows the slot being edited
// (docs/BRIEF.md#parameters-one-slot-at-a-time). C0: parameters and MIDI Learn
// only, silent until the voices arrive in C1.
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

private:
    using Store = krate::ParamValues<ParamCount>;
    Store &store(int index) noexcept { return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global; }
    const Store &store(int index) const noexcept
    {
        return isSlotParam(index) ? m_slots[std::size_t(editedSlot())] : m_global;
    }
    void handleMidi(const krate::MidiEvent &event) noexcept;

    std::array<Store, SlotCount> m_slots{Store{kParams}, Store{kParams}, Store{kParams}, Store{kParams}};
    Store m_global{kParams};
    std::atomic<int> m_edited{0};
    std::atomic<std::uint64_t> m_forcedChanges{0};
    krate::MidiLearnMap m_learn{kParams};
    double m_sampleRate = 48000.0;
};

} // namespace krimson
