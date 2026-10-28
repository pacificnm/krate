#pragma once

#include <cstdint>
#include <vector>

#include "core/Instrument.h"
#include "core/Transport.h"

// Drives an instrument with a standalone transport, the way the app's JACK
// callback does. Output of the last run() is in left/right.
struct TestHost
{
    krate::IInstrument &instrument;
    double sampleRate;
    krate::Transport transport;
    std::vector<float> left;
    std::vector<float> right;

    explicit TestHost(krate::IInstrument &i, double rate = 48000.0, double bpm = 120.0)
        : instrument(i)
        , sampleRate(rate)
    {
        instrument.prepare(rate, 0);
        transport.setBpm(bpm);
    }

    void play(bool on = true) { transport.setPlaying(on); }

    void run(std::uint32_t frames, const std::vector<krate::MidiEvent> &midi = {})
    {
        left.assign(frames, 0.0f);
        right.assign(frames, 0.0f);
        const krate::TransportState state = transport.advance(frames, sampleRate);
        krate::processStereo(instrument, left.data(), right.data(), frames, midi.data(),
                             std::uint32_t(midi.size()), &state);
    }
};
