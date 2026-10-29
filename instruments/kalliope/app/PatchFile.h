#pragma once

#include <array>

#include <QHash>
#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"

namespace kalliope::persistence {

using krate::files::LoadResult;

// patches.json (docs/PLAY.md#patches).
inline constexpr int kPatchFileVersion = 1;
inline constexpr int kBanks = 8;
inline constexpr int kPatchesPerBank = 8;
inline constexpr int kPatchCount = kBanks * kPatchesPerBank;

// A patch: a name and a normalized value for every parameter it stores
// (kalliope::inPatch), by parameter key.
struct Patch
{
    QString name;
    QHash<QString, qreal> values;

    bool operator==(const Patch &) const = default;
};

struct PatchBank
{
    std::array<Patch, kPatchCount> patches{};
    int current = 0;     // 0..63: bank * 8 + number
    bool manual = false; // the panel is not tied to a patch

    bool operator==(const PatchBank &) const = default;
};

Patch initPatch();          // every stored parameter at its default
Patch factoryPatch(int index); // 0..15 (banks 1-2); others: initPatch()
PatchBank defaultBank();    // banks 1-2 factory, 3-8 init

// On anything but Loaded, *bank is set to defaultBank().
LoadResult loadPatches(const QString &path, PatchBank *bank);
bool savePatches(const QString &path, const PatchBank &bank);

// Missing keys take the init patch's value; unknown keys are ignored; values clamped.
QJsonObject toJson(const PatchBank &bank);
PatchBank fromJson(const QJsonObject &json);

} // namespace kalliope::persistence
