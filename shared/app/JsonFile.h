#pragma once

#include <QJsonObject>
#include <QString>

namespace krate::files {

enum class LoadResult {
    Loaded,
    Missing,       // no file yet: use defaults
    Invalid,       // unreadable or not our format: use defaults
    NewerVersion,  // written by a newer version: use defaults, and don't overwrite it
};

// Reads a JSON object file with an integer "version" field. Loaded only for
// 1 <= version <= maxVersion; *json and *version are set then.
LoadResult readVersionedJson(const QString &path, int maxVersion, QJsonObject *json, int *version = nullptr);

// Atomic: writes a temp file and renames it over the target. Creates the folder.
bool writeJsonFile(const QString &path, const QJsonObject &json);

} // namespace krate::files
