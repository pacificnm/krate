#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "dsp/Sequencer.h"

namespace k303::persistence {

// v2: same format as v1; loading a v1 file adds the factory riffs added in v2.
inline constexpr int kPatternFileVersion = 2;
inline constexpr int kPatternCount = 8;

struct PatternBank
{
    std::array<Pattern, kPatternCount> patterns{};
    int current = 0;
};

// Patterns 0..3 are the factory riffs; the others are empty (all rests, length 16).
PatternBank defaultBank();

bool isEmpty(const Pattern &pattern); // every step a rest

enum class LoadResult {
    Loaded,
    Missing,       // no file yet: defaults
    Invalid,       // unreadable or not our format: defaults
    NewerVersion,  // written by a newer k303: defaults, and don't overwrite it
};

// On anything but Loaded, *bank is set to defaultBank().
LoadResult loadPatterns(const QString &path, PatternBank *bank);
// Atomic: writes a temp file and renames it over the target.
bool savePatterns(const QString &path, const PatternBank &bank);

// --- MIDI mappings (docs/MIDI.md#mapping-file) ---------------------------------

inline constexpr int kMidiMapFileVersion = 1;

struct MidiMap
{
    QString inputPortMatch = QStringLiteral("Impact LX49");
    std::array<int, 128> ccToParam; // ParamId per CC, -1 = unmapped

    MidiMap() { ccToParam.fill(-1); }
};

// On anything but Loaded, *map is set to MidiMap{}. Unknown parameter keys and
// CC numbers outside 0..127 are skipped.
LoadResult loadMidiMap(const QString &path, MidiMap *map);
bool saveMidiMap(const QString &path, const MidiMap &map);

// JSON conversion (docs/SEQUENCER.md#pattern-storage). Unknown keys are
// ignored; out-of-range values are clamped; missing steps are rests.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace k303::persistence
