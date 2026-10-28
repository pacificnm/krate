#pragma once

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

} // namespace krate
