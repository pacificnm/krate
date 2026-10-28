#pragma once

#include <cmath>
#include <string>
#include <string_view>

namespace krate {

// Everything a host, MIDI Learn, a generic panel or a session file needs to
// know about one parameter without knowing the instrument
// (docs/SHARED_FOUNDATIONS.MD#parameters). An instrument publishes a table of
// these; a parameter's index in the table is its id in the generic API.
struct ParamDescriptor
{
    std::string_view key;     // stable: saved in session and MIDI-map files; never rename
    std::string_view label;   // panel label
    float defaultValue;       // normalized 0..1
    int detents;              // 0 = continuous; N = selector or switch with N positions
    std::string (*format)(float normalized); // display text, e.g. "1.25 kHz"; not realtime-safe
};

// Snap a normalized value to the nearest detent position (unchanged if continuous).
inline float snapToDetent(const ParamDescriptor &param, float normalized) noexcept
{
    if (param.detents < 2)
        return normalized;
    const double positions = param.detents - 1;
    return float(double(std::lround(double(normalized) * positions)) / positions);
}

} // namespace krate
