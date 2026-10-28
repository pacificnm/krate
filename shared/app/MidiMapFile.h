#pragma once

#include <array>
#include <span>

#include <QString>

#include "app/JsonFile.h"
#include "core/ParamDescriptor.h"

namespace krate::files {

// An instrument's saved MIDI mappings (midimap.json; format in
// instruments/k303/docs/MIDI.md#mapping-file).
inline constexpr int kMidiMapFileVersion = 1;

struct MidiMap
{
    QString inputPortMatch = QStringLiteral("Impact LX49");
    std::array<int, 128> ccToParam; // parameter index per CC, -1 = unmapped

    MidiMap() { ccToParam.fill(-1); }
};

// Parameters are stored by key and resolved through the instrument's
// descriptor table, so this works for any instrument. On anything but Loaded,
// *map is set to MidiMap{}. Unknown keys and CC numbers outside 0..127 are skipped.
LoadResult loadMidiMap(const QString &path, std::span<const ParamDescriptor> params, MidiMap *map);
bool saveMidiMap(const QString &path, std::span<const ParamDescriptor> params, const MidiMap &map);

} // namespace krate::files
