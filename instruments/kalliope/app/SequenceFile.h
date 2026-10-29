#pragma once

#include <array>

#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"
#include "kalliope/dsp/Sequence.h"

namespace kalliope::persistence {

using krate::files::LoadResult;

// sequences.json (docs/SEQUENCER.md#storage).
inline constexpr int kSequenceFileVersion = 1;

struct SequenceBank
{
    std::array<SeqPattern, kSeqPatterns> patterns{};
    int current = 0;

    bool operator==(const SequenceBank &) const = default;
};

// On anything but Loaded, *bank is empty patterns (16 rests each).
LoadResult loadSequences(const QString &path, SequenceBank *bank);
bool saveSequences(const QString &path, const SequenceBank &bank);

QJsonObject toJson(const SequenceBank &bank);
SequenceBank sequencesFromJson(const QJsonObject &json);

} // namespace kalliope::persistence
