#pragma once

#include <array>
#include <span>

#include <QJsonObject>
#include <QString>

#include "dsp/Params.h"
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
    std::array<int, 128> ccToParam; // parameter index per CC, -1 = unmapped

    MidiMap() { ccToParam.fill(-1); }
};

// Parameters are stored by key and resolved through the instrument's
// descriptor table, so this works for any instrument. On anything but Loaded,
// *map is set to MidiMap{}. Unknown keys and CC numbers outside 0..127 are skipped.
LoadResult loadMidiMap(const QString &path, std::span<const krate::ParamDescriptor> params, MidiMap *map);
bool saveMidiMap(const QString &path, std::span<const krate::ParamDescriptor> params, const MidiMap &map);

// --- Panel settings: every knob/switch position (normalized 0..1) -------------

inline constexpr int kPanelFileVersion = 1;
using PanelValues = std::array<float, ParamCount>;

PanelValues defaultPanel();
// On anything but Loaded, *values is set to defaultPanel(). Unknown keys are
// ignored; missing ones keep their default; values are clamped to 0..1.
LoadResult loadPanel(const QString &path, PanelValues *values);
bool savePanel(const QString &path, const PanelValues &values);

// JSON conversion (docs/SEQUENCER.md#pattern-storage). Unknown keys are
// ignored; out-of-range values are clamped; missing steps are rests.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace k303::persistence
