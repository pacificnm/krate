#pragma once

#include <array>

#include <QHash>
#include <QJsonObject>
#include <QString>

#include "app/JsonFile.h"
#include "krimson/dsp/Params.h"

namespace krimson::persistence {

using krate::files::LoadResult;

// programs.json and performances.json (docs/PLAY.md#programs-and-performances).
inline constexpr int kFileVersion = 1;
inline constexpr int kBanks = 8;
inline constexpr int kProgramsPerBank = 8;
inline constexpr int kProgramCount = kBanks * kProgramsPerBank;
inline constexpr int kPerformanceCount = 8;

// A program: one slot's sound. `values` has every slot parameter by key;
// `morph` the morph targets that are set.
struct Program
{
    QString name;
    QHash<QString, qreal> values;
    QHash<QString, qreal> morph;

    bool operator==(const Program &) const = default;
};

struct ProgramBank
{
    std::array<Program, kProgramCount> programs{};
    std::array<int, SlotCount> current{}; // each slot's program number, 0..63

    bool operator==(const ProgramBank &) const = default;
};

// A performance: every slot's sound and setup (SlotController::toJson()); empty = unused.
struct Performance
{
    QString name;
    QJsonObject state;
};

struct PerformanceBank
{
    std::array<Performance, kPerformanceCount> performances{};
    int current = 0;
};

Program initProgram();        // every slot parameter at its default, no morph
Program factoryProgram(int index); // 0..63: every bank has factory sounds
ProgramBank defaultBank();     // the factory sounds; slots on 11, 12, 13, 14

QJsonObject toJson(const ProgramBank &bank);
ProgramBank programsFromJson(const QJsonObject &json); // missing keys: init values; clamped
LoadResult loadPrograms(const QString &path, ProgramBank *bank);
bool savePrograms(const QString &path, const ProgramBank &bank);

QJsonObject toJson(const PerformanceBank &bank);
PerformanceBank performancesFromJson(const QJsonObject &json);
LoadResult loadPerformances(const QString &path, PerformanceBank *bank);
bool savePerformances(const QString &path, const PerformanceBank &bank);

} // namespace krimson::persistence
