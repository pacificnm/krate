#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "core/Transport.h"
#include "host/JackProcessor.h"
#include "rack/Automation.h"
#include "rack/MidiCapture.h"
#include "rack/Preview.h"
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
    // Before the host starts: the keyboard's notes for the arranger to record.
    void setMidiCapture(MidiCapture *capture) { m_capture = capture; }
    // Before the host starts: automation, played into the instruments each block (part).
    void setAutomation(AutomationPlayer *automation) { m_automation = automation; }
    // Before the host starts: sounds auditioned from the sound library, over the master.
    void setPreview(Previewer *preview) { m_preview = preview; }

    int outputCount() const override { return 2 + 2 * m_rack.deviceCount(); }
    std::string outputName(int index) const override
    {
        if (index < 2)
            return index == 0 ? "master_L" : "master_R";
        const int device = (index - 2) / 2;
        const std::string id = device < int(m_ids.size()) ? m_ids[std::size_t(device)] : "device" + std::to_string(device + 1);
        return id + ((index - 2) % 2 == 0 ? "_L" : "_R");
    }
    // A device's own output gets its port once the device is in the rack.
    bool outputWanted(int index) const override { return index < 2 || m_rack.present((index - 2) / 2); }
    int playbackChannel(int index) const override { return index < 2 ? index : -1; }

    int inputCount() const override { return Rack::kInputs; }

    void prepare(double sampleRate, std::uint32_t) noexcept override { m_rack.prepare(sampleRate); }

    void process(const float *const *inputs, float *const *outputs, std::uint32_t frames, const MidiEvent *midi,
                 std::uint32_t midiCount, double sampleRate) noexcept override
    {
        // A loop's end inside this block (docs/DAW.md, D3): the block is run in
        // parts, split there, so the transport wraps on the exact sample.
        const std::uint32_t first = m_transport.framesUntilLoop(frames, sampleRate);
        if (first >= frames) {
            processPart(inputs, outputs, frames, midi, midiCount, sampleRate);
            return;
        }
        const int outs = std::min<int>(outputCount(), int(m_outs.size()));
        std::uint32_t done = 0, m = 0;
        for (int guard = 0; done < frames && guard < 64; ++guard) {
            const std::uint32_t n = done == 0 ? first : m_transport.framesUntilLoop(frames - done, sampleRate);
            for (int c = 0; c < outs; ++c)
                m_outs[std::size_t(c)] = outputs[c] ? outputs[c] + done : nullptr;
            for (int c = 0; c < Rack::kInputs; ++c)
                m_ins[std::size_t(c)] = inputs && inputs[c] ? inputs[c] + done : nullptr;
            std::uint32_t count = 0; // this part's MIDI, its frames from the part's start
            for (; m < midiCount && midi[m].frame < done + n; ++m)
                if (count < m_midi.size()) {
                    m_midi[count] = midi[m];
                    m_midi[count++].frame -= done;
                }
            processPart(inputs ? m_ins.data() : nullptr, m_outs.data(), n, m_midi.data(), count, sampleRate);
            done += n;
        }
    }

    float takePeak() noexcept override { return m_rack.takeMasterPeak(); }
    Transport *transport() noexcept override { return &m_transport; }

private:
    void processPart(const float *const *inputs, float *const *outputs, std::uint32_t frames, const MidiEvent *midi,
                     std::uint32_t midiCount, double sampleRate) noexcept
    {
        if (m_source)
            m_source->beforeTransport(m_transport);
        const TransportState transport = m_transport.advance(frames, sampleRate);
        if (m_capture)
            m_capture->capture(transport, midi, midiCount, sampleRate);
        if (m_automation)
            m_automation->apply(transport);
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
        if (m_preview)
            m_preview->mix(outputs[0], outputs[1], frames, sampleRate);
    }

    Rack &m_rack;
    Transport &m_transport;
    std::vector<std::string> m_ids;
    IEventSource *m_source = nullptr;
    MidiCapture *m_capture = nullptr;
    AutomationPlayer *m_automation = nullptr;
    Previewer *m_preview = nullptr;
    std::array<DeviceEvent, 4096> m_sourceEvents{};
    // A split block's parts: the buffers from the split on, its MIDI.
    std::array<float *, 2 + 2 * Rack::kMaxDevices> m_outs{};
    std::array<const float *, Rack::kInputs> m_ins{};
    std::array<MidiEvent, 1024> m_midi{};
};

} // namespace krate
