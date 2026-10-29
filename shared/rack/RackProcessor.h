#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/Transport.h"
#include "host/JackProcessor.h"
#include "rack/Rack.h"

namespace krate {

// The krate app's processor: advances the shared transport and runs the rack,
// with an optional event source (the song player) playing into it.
// Outputs: master_L, master_R (to the speakers), then <device>_L, <device>_R
// for every device (its own output, before the mixer: for recording or a DAW).
// Inputs: in_1, in_2 (an audio interface's two inputs) for the devices that play audio in.
class RackProcessor final : public JackProcessor
{
public:
    RackProcessor(Rack &rack, Transport &transport, std::vector<std::string> deviceIds)
        : m_rack(rack)
        , m_transport(transport)
        , m_ids(std::move(deviceIds))
    {
    }

    // Before the host starts.
    void setEventSource(IEventSource *source) { m_source = source; }

    int outputCount() const override { return 2 + 2 * m_rack.deviceCount(); }
    std::string outputName(int index) const override
    {
        if (index < 2)
            return index == 0 ? "master_L" : "master_R";
        const int device = (index - 2) / 2;
        const std::string id = device < int(m_ids.size()) ? m_ids[std::size_t(device)] : "device" + std::to_string(device + 1);
        return id + ((index - 2) % 2 == 0 ? "_L" : "_R");
    }
    int playbackChannel(int index) const override { return index < 2 ? index : -1; }

    int inputCount() const override { return Rack::kInputs; }

    void prepare(double sampleRate, std::uint32_t) noexcept override { m_rack.prepare(sampleRate); }

    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames, const MidiEvent *midi,
                 std::uint32_t midiCount, double sampleRate) noexcept override
    {
        if (m_source)
            m_source->beforeTransport(m_transport);
        const TransportState transport = m_transport.advance(frames, sampleRate);
        RackBlock block;
        block.frames = frames;
        block.midi = midi;
        block.midiCount = midiCount;
        if (m_source) {
            block.deviceMidi = m_sourceEvents.data();
            block.deviceMidiCount = m_source->render(transport, frames, sampleRate, m_sourceEvents.data(),
                                                     std::uint32_t(m_sourceEvents.size()));
        }
        block.transport = &transport;
        block.inputs = inputs;
        block.masterL = outputs[0];
        block.masterR = outputs[1];
        block.deviceOutputs = outputs + 2;
        m_rack.process(block);
    }

    float takePeak() noexcept override { return m_rack.takeMasterPeak(); }

private:
    Rack &m_rack;
    Transport &m_transport;
    std::vector<std::string> m_ids;
    IEventSource *m_source = nullptr;
    std::array<DeviceEvent, 4096> m_sourceEvents{};
};

} // namespace krate
