#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "dsp/Sequencer.h"

namespace k303::persistence {

inline constexpr int kPatternFileVersion = 1;
inline constexpr int kPatternCount = 8;

struct PatternBank
{
    std::array<Pattern, kPatternCount> patterns{};
    int current = 0;
};

// Pattern 0 is the demo riff; the others are empty (all rests, length 16).
PatternBank defaultBank();

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

// JSON conversion (docs/SEQUENCER.md#pattern-storage). Unknown keys are
// ignored; out-of-range values are clamped; missing steps are rests.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace k303::persistence
