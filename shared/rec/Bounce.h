#pragma once

#include <cstdint>
#include <functional>

namespace krate {

class JackProcessor;
class Transport;

// Rendering the rack offline, faster than real time (docs/DAW.md, D6): the
// processor run by this thread instead of the audio host's (which must have
// let go of it), the transport playing from `fromBeat` to `toBeat`, then
// stopped while the sound dies away (until it stays under `silence` for a
// quarter of a second, at most `tailSeconds`).
struct BounceRange
{
    double fromBeat = 0.0;
    double toBeat = 0.0;
    double tailSeconds = 8.0;
    float silence = 1e-4f; // -80 dBFS
};

// `write(left, right, frames)` gets the master output as it's made; it, or
// `progress(0..1)`, returning false stops early. Returns the frames written,
// or -1 if stopped.
std::int64_t bounce(JackProcessor &processor, Transport &transport, double sampleRate, const BounceRange &range,
                    const std::function<bool(const float *, const float *, std::uint32_t)> &write,
                    const std::function<bool(double)> &progress = {});

} // namespace krate
