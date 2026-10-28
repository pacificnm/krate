#pragma once

#include <cstdint>
#include <string>

#include "core/Instrument.h"
#include "core/MasterStage.h"
#include "core/Transport.h"

namespace krate {

// What JackHost runs in its realtime callback: one instrument (standalone apps)
// or the whole rack. Names the output ports, says which go to the speakers,
// and renders each block. No JACK types: processors are testable offline.
class JackProcessor
{
public:
    virtual ~JackProcessor() = default;

    virtual int outputCount() const = 0;
    virtual std::string outputName(int index) const = 0;
    // Physical playback channel (0, 1, ...) this output auto-connects to, or -1.
    virtual int playbackChannel(int index) const = 0;

    // Realtime-thread contract as IInstrument::prepare / process.
    virtual void prepare(double sampleRate, std::uint32_t maxFrames) noexcept = 0;
    virtual void process(float *const *outputs, std::uint32_t frames, const MidiEvent *midi,
                         std::uint32_t midiCount, double sampleRate) noexcept = 0;

    virtual float takePeak() noexcept = 0; // master output peak since last call (GUI thread)
};

// A standalone app's processor: advance the transport, run the instrument, then
// the master stage. Outputs are the instrument's channels (out_L/out_R for stereo).
class InstrumentProcessor final : public JackProcessor
{
public:
    InstrumentProcessor(IInstrument &instrument, Transport &transport)
        : m_instrument(instrument)
        , m_transport(transport)
    {
    }

    int outputCount() const override { return m_instrument.outputChannels(); }
    std::string outputName(int index) const override
    {
        if (outputCount() == 2)
            return index == 0 ? "out_L" : "out_R";
        return "out_" + std::to_string(index + 1);
    }
    int playbackChannel(int index) const override { return index; }

    void prepare(double sampleRate, std::uint32_t maxFrames) noexcept override
    {
        m_instrument.prepare(sampleRate, maxFrames);
    }

    void process(float *const *outputs, std::uint32_t frames, const MidiEvent *midi, std::uint32_t midiCount,
                 double sampleRate) noexcept override
    {
        const TransportState transport = m_transport.advance(frames, sampleRate);
        m_instrument.process(AudioBlock{outputs, frames, midi, midiCount, &transport});
        m_master.process(outputs, outputCount(), frames);
    }

    float takePeak() noexcept override { return m_master.takePeak(); }

private:
    IInstrument &m_instrument;
    Transport &m_transport;
    MasterStage m_master;
};

} // namespace krate
