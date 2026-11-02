#pragma once

#include <deque>
#include <functional>

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "Shell.h"
#include "app/Locations.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Remote" (docs/REMOTE.md): the player Krate Remote is
// connected to, over SSH. What it is (Krate's version, running or not, where
// its folders are), its settings (settings.json, changed as Krate's Settings
// screen changes it: Krate takes them while it runs, folders at its next
// start), restarting Krate, its crash reports and log, and logging in with a
// key instead of a password.
class RemoteController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Remote)
    QML_SINGLETON

    Q_PROPERTY(QString host READ host NOTIFY targetChanged)
    Q_PROPERTY(QString user READ user NOTIFY targetChanged)
    Q_PROPERTY(int port READ port NOTIFY targetChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged) // "disconnected", "connecting", "connected"
    Q_PROPERTY(bool needsPassword READ needsPassword NOTIFY stateChanged)
    Q_PROPERTY(bool usingPassword READ usingPassword NOTIFY stateChanged) // logged in with one (a key would do)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)       // the last thing that went wrong, or happened
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

    // The player.
    Q_PROPERTY(QString hostName READ hostName NOTIFY infoChanged)
    Q_PROPERTY(QString home READ home NOTIFY infoChanged)
    Q_PROPERTY(QString version READ version NOTIFY infoChanged)       // Krate's package there ("" = not installed)
    Q_PROPERTY(QString krateState READ krateState NOTIFY infoChanged) // "kiosk", "running", "stopped"

    // Its settings (docs/SETTINGS.md).
    Q_PROPERTY(QVariantList folders READ folders NOTIFY settingsChanged)
    Q_PROPERTY(QStringList pluginFolders READ pluginFolders NOTIFY settingsChanged)
    Q_PROPERTY(QString midiInputs READ midiInputs NOTIFY settingsChanged)
    Q_PROPERTY(bool programChangeFocus READ programChangeFocus NOTIFY settingsChanged)
    Q_PROPERTY(bool autoCheckUpdates READ autoCheckUpdates NOTIFY settingsChanged)
    Q_PROPERTY(bool restartNeeded READ restartNeeded NOTIFY settingsChanged) // folders waiting for Krate's start
    Q_PROPERTY(QString settingsPath READ settingsPath NOTIFY infoChanged)
    // Places worth going to in Files: [{label, path}]
    Q_PROPERTY(QVariantList places READ places NOTIFY settingsChanged)

    // Crash reports, newest first: [{name, path, when}]; Krate's log.
    Q_PROPERTY(QVariantList crashReports READ crashReports NOTIFY reportsChanged)
    Q_PROPERTY(QString logPath READ logPath NOTIFY settingsChanged)

public:
    explicit RemoteController(Shell &shell, QObject *parent = nullptr);
    ~RemoteController() override;
    static RemoteController *create(QQmlEngine *, QJSEngine *);

    QString host() const { return m_host; }
    QString user() const { return m_user; }
    int port() const { return m_port; }
    QString state() const { return m_state; }
    bool needsPassword() const { return m_needsPassword; }
    bool usingPassword() const { return m_shell.hasPassword(); }
    QString message() const { return m_message; }
    bool busy() const { return m_busy > 0; }
    QString hostName() const { return m_hostName; }
    QString home() const { return m_home; }
    QString version() const { return m_version; }
    QString krateState() const { return m_krateState; }
    QVariantList folders() const;
    QStringList pluginFolders() const;
    QString midiInputs() const { return m_settings.value(u"midiInputs").toString(); }
    bool programChangeFocus() const { return m_settings.value(u"programChangeFocus").toBool(true); }
    bool autoCheckUpdates() const { return m_settings.value(u"autoCheckUpdates").toBool(true); }
    bool restartNeeded() const { return !m_settings.value(u"pending").toObject().isEmpty(); }
    QString settingsPath() const { return m_configHome.isEmpty() ? QString() : m_configHome + QStringLiteral("/krate/settings.json"); }
    QVariantList places() const;
    QVariantList crashReports() const { return m_reports; }
    QString logPath() const;
    QString folder(krate::locations::Folder f) const;
    QJsonObject settings() const { return m_settings; }

    // Connecting: with a password if the player asks for one (a key needs none).
    Q_INVOKABLE void connectTo(const QString &host, const QString &user, int port, const QString &password = {});
    Q_INVOKABLE void disconnectFrom();
    Q_INVOKABLE void refresh(); // everything read again

    Q_INVOKABLE void setFolder(const QString &key, const QString &path, bool move);
    Q_INVOKABLE void resetFolder(const QString &key, bool move) { setFolder(key, QString(), move); }
    Q_INVOKABLE void cancel(const QString &key);
    Q_INVOKABLE void setPluginFolders(const QStringList &folders);
    Q_INVOKABLE void addPluginFolder(const QString &folder);
    Q_INVOKABLE void removePluginFolder(const QString &folder);
    Q_INVOKABLE void setMidiInputs(const QString &pattern);
    Q_INVOKABLE void setProgramChangeFocus(bool on);
    Q_INVOKABLE void setAutoCheckUpdates(bool on);

    // Krate quits and the player's kiosk starts it again (folders move then).
    Q_INVOKABLE void restartKrate();
    // A text file there (a crash report, the log), as `textRead`.
    Q_INVOKABLE void readText(const QString &path);
    // A key of this app's own (~/.ssh/krate-remote) put on the player: no password from then on.
    Q_INVOKABLE void setUpKey();

    // Sends the player's answer to `done` (tests use it, Files too).
    Shell &shell() { return m_shell; }

signals:
    void targetChanged();
    void stateChanged();
    void messageChanged();
    void busyChanged();
    void infoChanged();
    void settingsChanged();
    void reportsChanged();
    void connected();
    void textRead(const QString &path, const QString &text);

private:
    void setState(const QString &state);
    void setMessage(const QString &message);
    void run(const QString &command, Shell::Done done, const QByteArray &input = {});
    void readInfo(const Shell::Result &r);
    void readSettings(std::function<void()> then = {});
    void readReports();
    // Changes the file: read again first (someone may have changed it), the
    // change applied, written whole (renamed into place).
    void edit(std::function<QJsonObject(QJsonObject)> change);
    void nextEdit();
    QString failure(const Shell::Result &r) const;

    static inline RemoteController *s_instance = nullptr;
    Shell &m_shell;
    QString m_host, m_user;
    int m_port = 22;
    QString m_state = QStringLiteral("disconnected");
    bool m_needsPassword = false;
    QString m_message;
    int m_busy = 0;
    QString m_hostName, m_home, m_configHome, m_version, m_krateState;
    krate::locations::Roots m_roots;
    QJsonObject m_settings;
    QVariantList m_reports;
    std::deque<std::function<QJsonObject(QJsonObject)>> m_edits;
    bool m_editing = false;
    int m_generation = 0; // a connection's: answers for an earlier one are dropped
    QTimer m_poll;        // the player's state (Krate's version, running) read again now and then
};
