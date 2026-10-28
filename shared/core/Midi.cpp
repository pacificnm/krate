#include "core/Midi.h"

#include <cmath>

namespace krate {

MidiMessage decode(const MidiEvent &event) noexcept
{
    MidiMessage m;
    if (event.size < 1)
        return m;
    const int status = event.data[0] & 0xF0;
    m.channel = event.data[0] & 0x0F;
    m.data1 = event.size > 1 ? event.data[1] & 0x7F : 0;
    m.data2 = event.size > 2 ? event.data[2] & 0x7F : 0;

    switch (status) {
    case 0x90: m.type = m.data2 > 0 ? MidiMessage::Type::NoteOn : MidiMessage::Type::NoteOff; break;
    case 0x80: m.type = MidiMessage::Type::NoteOff; break;
    case 0xB0: m.type = MidiMessage::Type::ControlChange; break;
    case 0xC0: m.type = MidiMessage::Type::ProgramChange; break;
    default: m.type = MidiMessage::Type::Other; break;
    }
    return m;
}

MidiLearnMap::MidiLearnMap(std::span<const ParamDescriptor> params) noexcept
    : m_params(params)
{
    for (auto &m : m_map)
        m.store(kUnmapped, std::memory_order_relaxed);
}

void MidiLearnMap::setMapping(int cc, int param) noexcept
{
    if (cc >= 0 && cc < 128)
        m_map[std::size_t(cc)].store(std::int16_t(valid(param) ? param : kUnmapped), std::memory_order_relaxed);
}

int MidiLearnMap::mapping(int cc) const noexcept
{
    return (cc >= 0 && cc < 128) ? m_map[std::size_t(cc)].load(std::memory_order_relaxed) : kUnmapped;
}

void MidiLearnMap::clearAll() noexcept
{
    for (auto &m : m_map)
        m.store(kUnmapped, std::memory_order_relaxed);
}

void MidiLearnMap::setLearnTarget(int param) noexcept
{
    m_learnTarget.store(std::int16_t(valid(param) ? param : kUnmapped), std::memory_order_relaxed);
}

MidiLearnMap::Result MidiLearnMap::controlChange(int cc, int value) noexcept
{
    Result r;
    if (cc < 0 || cc >= 128)
        return r;

    int param = m_learnTarget.load(std::memory_order_relaxed);
    if (valid(param)) {
        m_map[std::size_t(cc)].store(std::int16_t(param), std::memory_order_relaxed);
        m_learnTarget.store(kUnmapped, std::memory_order_relaxed);
        m_learned.push({std::uint8_t(cc), std::int16_t(param)});
    } else {
        param = m_map[std::size_t(cc)].load(std::memory_order_relaxed);
    }
    if (!valid(param))
        return r;

    // Switches and selectors snap to their positions; knobs follow the value.
    r.param = param;
    r.value = snapToDetent(m_params[std::size_t(param)], float(value & 0x7F) / 127.0f);
    return r;
}

} // namespace krate
