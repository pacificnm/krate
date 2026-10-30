#pragma once

#include <array>

#include <QHash>
#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"

namespace k909::persistence {

using krate::files::LoadResult;

// kits.json (docs/KITS.md): saved sets of every sound and mix knob.
inline constexpr int kKitFileVersion = 1;
inline constexpr int kKitCount = 16;
inline constexpr int kFactoryKitCount = 6;

// A kit: a name and a normalized value for every parameter it keeps
// (inKit), by parameter key.
struct Kit
{
    QString name;
    QHash<QString, qreal> values;
    QString sample; // the SAMPLE voice's file in the library; "" = the kit leaves the sample as it is

    bool operator==(const Kit &) const = default;
};

struct KitBank
{
    std::array<Kit, kKitCount> kits{};
    int current = 0;

    bool operator==(const KitBank &) const = default;
};

// True for the parameters a kit keeps: all but Volume, Tempo and Shuffle.
bool inKit(int param);

Kit initKit();             // every kept parameter at its default ("Init")
Kit factoryKit(int index); // 0..kFactoryKitCount-1; others: initKit()
KitBank defaultKitBank();     // the factory kits, then Init

// On anything but Loaded, *bank is set to defaultKitBank().
LoadResult loadKits(const QString &path, KitBank *bank);
bool saveKits(const QString &path, const KitBank &bank);

// Missing keys take the default; unknown keys are ignored; values clamped.
QJsonObject kitsToJson(const KitBank &bank);
KitBank kitsFromJson(const QJsonObject &json);

} // namespace k909::persistence
