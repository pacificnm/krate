#include "dsp/Engine.h"

#include <algorithm>

namespace k909 {

void Engine::prepare(double sampleRate, std::uint32_t) noexcept
{
    m_sampleRate = sampleRate;
}

void Engine::handleMidi(const krate::MidiEvent &event) noexcept
{
    const krate::MidiMessage m = krate::decode(event); // all channels (omni)
    switch (m.type) {
    case krate::MidiMessage::Type::ControlChange:
        if (const auto r = m_learn.controlChange(m.data1, m.data2); r.param != krate::MidiLearnMap::kUnmapped)
            m_params.setFromAudio(r.param, r.value);
        break;
    case krate::MidiMessage::Type::NoteOn:
    case krate::MidiMessage::Type::NoteOff:
    case krate::MidiMessage::Type::ProgramChange:
    case krate::MidiMessage::Type::Other:
        break; // notes play the voices from D2
    }
}

void Engine::process(const krate::AudioBlock &block) noexcept
{
    for (std::uint32_t i = 0; i < block.midiCount; ++i)
        handleMidi(block.midi[i]);
    for (int channel = 0; channel < outputChannels(); ++channel)
        std::fill_n(block.outputs[channel], block.frames, 0.0f);
}

} // namespace k909
