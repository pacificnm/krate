#pragma once

#include <cstdint>

#include "core/Instrument.h"
#include "core/SpscQueue.h"

namespace krate {

// The keyboard's notes as the rack hears them, with where they fell on the
// transport, for the arranger to record into MIDI clips (docs/DAW.md, D3).
// The audio thread (RackProcessor) pushes while the transport plays; the GUI
// thread pops. Full: the newest are dropped (the GUI takes them 30 times a second).
class MidiCapture
{
public:
    struct Event
    {
        double beat = 0.0; // on the transport
        std::uint8_t status = 0, data1 = 0, data2 = 0;
    };

    // Audio thread: this block's notes (on and off), if the transport plays.
    void capture(const TransportState &transport, const MidiEvent *midi, std::uint32_t count,
                 double sampleRate) noexcept
    {
        if (!transport.playing || transport.bpm <= 0.0)
            return;
        const double beatsPerFrame = transport.bpm / 60.0 / sampleRate;
        for (std::uint32_t i = 0; i < count; ++i) {
            const MidiEvent &e = midi[i];
            const std::uint8_t type = e.data[0] & 0xF0;
            if (e.size < 3 || (type != 0x90 && type != 0x80))
                continue;
            m_events.push({transport.beatAtBlockStart + double(e.frame) * beatsPerFrame, e.data[0], e.data[1], e.data[2]});
        }
    }

    // GUI thread.
    std::optional<Event> take() noexcept { return m_events.pop(); }

private:
    SpscQueue<Event, 4096> m_events;
};

} // namespace krate
