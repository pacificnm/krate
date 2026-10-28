#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"
#include "k909/dsp/Sequencer.h"

namespace k909::persistence {

using krate::files::LoadResult;

// patterns.json (docs/SEQUENCER.md#storage).
inline constexpr int kPatternFileVersion = 1;
inline constexpr int kPatternCount = 8; // A..H

struct PatternBank
{
    std::array<Pattern, kPatternCount> patterns{};
    int current = 0;

    bool operator==(const PatternBank &) const = default;
};

// A..D are the factory patterns; E..H are empty.
PatternBank defaultBank();

// The JSON key of each track, in DrumId order: "bd", "sd", ... "rd".
QString trackKey(int track);

// On anything but Loaded, *bank is set to defaultBank().
LoadResult loadPatterns(const QString &path, PatternBank *bank);
// Atomic: writes a temp file and renames it over the target.
bool savePatterns(const QString &path, const PatternBank &bank);

// JSON conversion. Unknown keys are ignored; out-of-range values are clamped;
// missing tracks and steps are off.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace k909::persistence
