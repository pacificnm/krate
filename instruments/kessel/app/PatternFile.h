#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"
#include "kessel/dsp/Sequencer.h"

namespace kessel::persistence {

using krate::files::LoadResult;

// patterns.json (docs/SEQUENCER.md#storage).
inline constexpr int kPatternFileVersion = 1;
inline constexpr int kPatternCount = 16; // A..P: two banks of eight

struct PatternBank
{
    std::array<Pattern, kPatternCount> patterns{};
    int current = 0;

    bool operator==(const PatternBank &) const = default;
};

// A..D the classic factory patterns, E..H empty, I..P the industrial ones.
PatternBank defaultBank();

// The JSON key of each track, in DrumId order: "bd", "sd", ... "rd".
QString trackKey(int track);

// On anything but Loaded, *bank is set to defaultBank().
LoadResult loadPatterns(const QString &path, PatternBank *bank);
// Atomic: writes a temp file and renames it over the target.
bool savePatterns(const QString &path, const PatternBank &bank);

// JSON conversion. Unknown keys are ignored; out-of-range values are clamped;
// missing tracks and steps are off; patterns missing from the file (an older
// file of 8) are the default bank's.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace Kessel::persistence
