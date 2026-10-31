#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"
#include "dsp/Sequencer.h"

namespace k303::persistence {

using krate::files::LoadResult;

// patterns.json (docs/SEQUENCER.md#pattern-storage).
// v2: same format as v1; loading a v1 file adds the factory riffs added in v2.
inline constexpr int kPatternFileVersion = 2;
// 8 banks (A-H) of 8 patterns. A file from before banks (8 patterns) loads as bank A.
inline constexpr int kBanks = 8;
inline constexpr int kPatternsPerBank = 8;
inline constexpr int kPatternCount = kBanks * kPatternsPerBank;

struct PatternBank
{
    std::array<Pattern, kPatternCount> patterns{};
    int current = 0;
};

// Patterns 0..3 are the factory riffs; the others are empty (all rests, length 16).
PatternBank defaultBank();

bool isEmpty(const Pattern &pattern); // every step a rest

// On anything but Loaded, *bank is set to defaultBank().
LoadResult loadPatterns(const QString &path, PatternBank *bank);
// Atomic: writes a temp file and renames it over the target.
bool savePatterns(const QString &path, const PatternBank &bank);

// JSON conversion. Unknown keys are ignored; out-of-range values are clamped;
// missing steps are rests.
QJsonObject toJson(const PatternBank &bank);
PatternBank fromJson(const QJsonObject &json);

} // namespace k303::persistence
