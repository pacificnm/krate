#pragma once

#include <functional>

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "app/Locations.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Settings": the Settings screen's (docs/SETTINGS.md), kept in
// ~/.config/krate/settings.json.
// - Folders (krate::locations): where Krate keeps its data, recordings, song
//   library, exports and the instruments' sounds. A new folder for the ones
//   Krate has open applies at the next start, its files moved there first if
//   asked (built before anything opens them); exports apply at once.
// - Plugin folders searched besides the standard ones; which MIDI inputs to
//   connect; Program Change switching focus; checking for updates.
class SettingsController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON

    // [{key, label, about, path, defaultPath, isDefault, pending, pendingMove, pendingDefault, atOnce}]
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
    Q_PROPERTY(QStringList pluginFolders READ pluginFolders WRITE setPluginFolders NOTIFY changed)
    Q_PROPERTY(QStringList standardPluginFolders READ standardPluginFolders CONSTANT)
    Q_PROPERTY(QString midiInputs READ midiInputs WRITE setMidiInputs NOTIFY changed) // "" = every MIDI device
    Q_PROPERTY(bool programChangeFocus READ programChangeFocus WRITE setProgramChangeFocus NOTIFY changed)
    Q_PROPERTY(bool autoCheckUpdates READ autoCheckUpdates WRITE setAutoCheckUpdates NOTIFY changed)
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY changed) // folders waiting for a start
    Q_PROPERTY(QStringList startNotes READ startNotes CONSTANT)     // what this start did with them
    Q_PROPERTY(QString settingsPath READ settingsPath CONSTANT)
    Q_PROPERTY(QString configFolder READ configFolder CONSTANT)
    Q_PROPERTY(bool kiosk READ kiosk CONSTANT) // started by the player's kiosk (it restarts Krate)

public:
    // Reads `path` and does what waits for this start (moving folders).
    // `instrumentIds`: the instrument folders a moved instruments' folder takes along.
    SettingsController(QString path, QStringList instrumentIds, QObject *parent = nullptr);
    ~SettingsController() override;
    static SettingsController *create(QQmlEngine *, QJSEngine *);

    // Where a folder is for this run.
    QString folder(krate::locations::Folder f) const { return krate::locations::folder(f, m_current); }

    QVariantList folders() const;
    QStringList pluginFolders() const { return m_pluginFolders; }
    void setPluginFolders(const QStringList &folders);
    QStringList standardPluginFolders() const;
    QString midiInputs() const { return m_midiInputs; }
    void setMidiInputs(const QString &pattern);
    bool programChangeFocus() const { return m_programChangeFocus; }
    void setProgramChangeFocus(bool on);
    bool autoCheckUpdates() const { return m_autoCheckUpdates; }
    void setAutoCheckUpdates(bool on);
    bool restartNeeded() const { return !m_pending.isEmpty(); }
    QStringList startNotes() const { return m_notes; }
    QString settingsPath() const { return m_path; }
    QString configFolder() const;
    bool kiosk() const;

    // A folder to use from the next start (now, for exports); `move`: take the files there.
    Q_INVOKABLE void setFolder(const QString &key, const QString &path, bool move);
    Q_INVOKABLE void resetFolder(const QString &key, bool move); // back to its default
    Q_INVOKABLE void cancel(const QString &key);                 // what waited for the start
    Q_INVOKABLE bool emptyOrMissing(const QString &path) const { return krate::locations::isEmptyOrMissing(path); }
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void restart(); // the kiosk starts Krate again; elsewhere Krate does

    // What changes act on (main() connects them).
    std::function<void(const QString &)> onExportsChanged, onMidiInputsChanged;
    std::function<void(const QStringList &)> onPluginFoldersChanged;
    std::function<void(bool)> onProgramChangeFocusChanged, onAutoCheckChanged;

    // The start's moves, for tests: what `settings` waits for, done.
    static QJsonObject applyPending(QJsonObject settings, const QStringList &instrumentIds, QStringList *notes);

signals:
    void changed();

private:
    void save();

    static inline SettingsController *s_instance = nullptr;
    QString m_path;
    QStringList m_ids;
    QJsonObject m_current; // as this run has it ("folders" as of the start)
    QJsonObject m_pending; // key -> {path, move}: for the next start
    QStringList m_pluginFolders, m_notes;
    QString m_midiInputs;
    bool m_programChangeFocus = true;
    bool m_autoCheckUpdates = true;
};
