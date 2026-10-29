#pragma once

#include <cstdint>

#include "core/Instrument.h"

namespace krate {

class Transport;

// A MIDI event for one device of the rack, past the rack's MIDI routing (a
// song track assigned to an instrument goes to it whatever its channel or focus).
struct DeviceEvent
{
    int device = -1;
    MidiEvent event; // frame within the block

    std::uint32_t frame() const noexcept { return event.frame; }
};

// Something that plays into the rack on the audio thread, alongside the MIDI
// input: the song player (shared/song/SongPlayer.h). The host calls, per block:
// beforeTransport() (it may set the tempo), then Transport::advance(), then
// render() with that block's transport state, and hands the events to the rack.
class IEventSource
{
public:
    virtual ~IEventSource() = default;
    virtual void beforeTransport(Transport &transport) noexcept = 0;
    // Writes up to `capacity` events sorted by frame; returns how many.
    virtual std::uint32_t render(const TransportState &transport, std::uint32_t frames, double sampleRate,
                                 DeviceEvent *out, std::uint32_t capacity) noexcept = 0;
};

} // namespace krate
