#include "krimson/dsp/Engine.h"

#include <algorithm>

namespace krimson {

namespace {
constexpr std::uint64_t kSlotParamBits = (std::uint64_t(1) << SlotParamCount) - 1;
} // namespace

Engine::Engine() = default;

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
}

void Engine::setEditedSlot(int slot) noexcept
{
    slot = std::clamp(slot, 0, SlotCount - 1);
    if (m_edited.exchange(slot, std::memory_order_relaxed) != slot)
        m_forcedChanges.fetch_or(kSlotParamBits, std::memory_order_relaxed);
}

std::uint64_t Engine::takeParamsChangedByAudio() noexcept
{
    std::uint64_t changed = m_global.takeChanged() | m_forcedChanges.exchange(0, std::memory_order_relaxed);
    for (Store &slot : m_slots)
        changed |= slot.takeChanged() & kSlotParamBits; // any slot: the edited one is what the panel shows
    return changed;
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event);
    if (m.type == krate::MidiMessage::Type::ControlChange)
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            store(r.param).setFromAudio(r.param, r.value); // learned knobs edit the edited slot
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    for (std::uint32_t i = 0; i < block.midiCount; ++i)
        handleMidi(block.midi[i]);
    for (int c = 0; c < outputChannels(); ++c)
        std::fill_n(block.outputs[c], block.frames, 0.0f);
}

} // namespace krimson
