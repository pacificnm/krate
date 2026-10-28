#pragma once

#include <array>
#include <span>

#include <QString>
#include <QStringList>

#include "app/JsonFile.h"
#include "core/ParamDescriptor.h"

namespace krate::files {

// An instrument's saved MIDI mappings (midimap.json; format in
// instruments/k303/docs/MIDI.md#mapping-file).
inline constexpr int kMidiMapFileVersion = 1;

struct MidiMap
{
    QString inputPortMatch = QStringLiteral("Impact LX49");
    // Per CC: parameter index, or an action as krate::MidiLearnMap::actionTarget(n); -1 = unmapped.
    std::array<int, 128> ccToParam;

    MidiMap() { ccToParam.fill(-1); }
};

// Targets are stored by key: a parameter key from the instrument's descriptor
// table, or an action key from `actions` (the app's learnable buttons), so this
// works for any instrument. On anything but Loaded, *map is set to MidiMap{}.
// Unknown keys and CC numbers outside 0..127 are skipped.
LoadResult loadMidiMap(const QString &path, std::span<const ParamDescriptor> params, MidiMap *map,
                       const QStringList &actions = {});
bool saveMidiMap(const QString &path, std::span<const ParamDescriptor> params, const MidiMap &map,
                 const QStringList &actions = {});

} // namespace krate::files
