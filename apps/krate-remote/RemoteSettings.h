#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include "app/Locations.h"

// The player's settings.json as Krate Remote changes it (docs/REMOTE.md):
// the same file, and the same rules, as Krate's Settings screen
// (docs/SETTINGS.md). A folder waits for Krate's next start in "pending"
// (its files moved there first if asked); exports change at once; the rest
// Krate takes while it runs (it reads the file again when it changes).
namespace remote {

// The folders as the screen shows them, in the player's paths:
// [{key, label, about, path, defaultPath, isDefault, pending, pendingMove, atOnce}]
QVariantList folderRows(const QJsonObject &settings, const krate::locations::Roots &roots);

// A folder from Krate's next start (exports: at once); "" = its default.
// Where it is already: whatever waited for it is cancelled.
QJsonObject setFolder(QJsonObject settings, const QString &key, const QString &path, bool move,
                      const krate::locations::Roots &roots);
QJsonObject cancel(QJsonObject settings, const QString &key);
// Plugin folders cleaned up (trimmed, no repeats).
QJsonObject setPluginFolders(QJsonObject settings, const QStringList &folders);

} // namespace remote
