#pragma once

#include <cstdint>
#include <span>

#include "core/ParamDescriptor.h"

namespace krate {

class MidiLearnMap;

// A raw MIDI message (1..3 bytes) at a frame offset inside the current block.
struct MidiEvent
{
    std::uint32_t frame = 0;
    std::uint8_t size = 0;
    std::uint8_t data[3] = {0, 0, 0};
};

// Shared clock, supplied by the host every block (docs/SHARED_FOUNDATIONS.MD#transport).
struct TransportState
{
    bool playing = false;
    bool restarted = false;        // play was pressed this block
    double bpm = 120.0;            // constant for the block
    double beatAtBlockStart = 0.0; // quarter notes since play was pressed
};

// One block of work for an instrument.
struct AudioBlock
{
    float *const *outputs = nullptr;         // outputChannels() buffers, `frames` long; overwritten
    std::uint32_t frames = 0;
    const MidiEvent *midi = nullptr;         // sorted by frame, each frame < frames
    std::uint32_t midiCount = 0;
    const TransportState *transport = nullptr; // nullptr: the instrument keeps its own time (until R3)
};

// The realtime core of an instrument (docs/SHARED_FOUNDATIONS.MD#audio-block-and-instrument).
// Hosts (standalone JACK app, the Krate rack, the offline renderer, tests)
// drive instruments only through this interface.
class IInstrument
{
public:
    virtual ~IInstrument() = default;

    // Called before the first process() and when the sample rate changes, from
    // the processing thread between blocks, never concurrently with process().
    // Must not allocate: size buffers for maxFrames in the constructor.
    virtual void prepare(double sampleRate, std::uint32_t maxFrames) noexcept = 0;

    virtual std::span<const ParamDescriptor> parameters() const noexcept = 0;
    virtual int outputChannels() const noexcept { return 2; }

    // Parameters by index into parameters(), normalized 0..1. Any thread.
    virtual float param(int index) const noexcept = 0;
    virtual void setParam(int index, float normalized) noexcept = 0;
    // Bitmask (bit = index) of parameters the audio thread changed (e.g. from MIDI) since the last call.
    virtual std::uint64_t takeParamsChangedByAudio() noexcept = 0;

    // The instrument's CC map and MIDI Learn state, if it supports MIDI Learn.
    virtual MidiLearnMap *midiLearnMap() noexcept { return nullptr; }

    // Realtime: no allocation, locks, I/O or blocking.
    virtual void process(const AudioBlock &block) noexcept = 0;
};

// Convenience for stereo hosts and tests: one block into two buffers.
inline void processStereo(IInstrument &instrument, float *left, float *right, std::uint32_t frames,
                          const MidiEvent *midi = nullptr, std::uint32_t midiCount = 0,
                          const TransportState *transport = nullptr) noexcept
{
    float *const outputs[2] = {left, right};
    instrument.process(AudioBlock{outputs, frames, midi, midiCount, transport});
}

} // namespace krate
