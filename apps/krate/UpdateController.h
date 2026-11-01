#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

// QML singleton "Updates": Krate's updates from its GitHub releases
// (docs/UPDATES.md). It asks GitHub for the latest release (soon after
// starting, then twice a day, or on CHECK), and UPDATE starts the installed
// updater (krate-update.service, as root: it fetches, checks and installs the
// release itself, then restarts the kiosk), whose progress it shows.
class UpdateController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Updates)
    QML_SINGLETON

    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed) // "" until known
    Q_PROPERTY(bool available READ available NOTIFY changed)            // latest is newer
    Q_PROPERTY(QString notes READ notes NOTIFY changed)                 // the release's notes
    Q_PROPERTY(bool canInstall READ canInstall CONSTANT)                // the updater is installed
    // "", "checking", "installing" (and the updater's: "downloading", ...), "done", "current", "failed"
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)             // what's happening, in words

public:
    // `updaterPath`: the updater the package installs (absent: running from a build).
    explicit UpdateController(QString currentVersion, QString updaterPath = QStringLiteral("/usr/libexec/krate/krate-update"),
                              QString statusPath = QStringLiteral("/var/lib/krate/update-status"),
                              QObject *parent = nullptr);
    ~UpdateController() override;
    static UpdateController *create(QQmlEngine *, QJSEngine *);

    QString currentVersion() const { return m_current; }
    QString latestVersion() const { return m_latest; }
    bool available() const;
    QString notes() const { return m_notes; }
    bool canInstall() const;
    QString state() const { return m_state; }
    QString message() const { return m_message; }

    Q_INVOKABLE void check();
    // Asking by itself (soon after starting, then twice a day): Settings' switch.
    void setAutoCheck(bool on);
    Q_INVOKABLE void install();

    // GitHub's latest-release answer: its version ("v0.3.0" -> "0.3.0") and
    // notes; whether it carries a Krate package. False if it isn't one.
    struct Release
    {
        QString version, notes;
        bool hasPackage = false;
    };
    static bool parseRelease(const QByteArray &json, Release *release);
    // Whether `latest` is after `current` ("0.10.0" after "0.9.1").
    static bool newer(const QString &latest, const QString &current);
    // The updater's status line, "<state> <text>".
    static void parseStatus(const QString &line, QString *state, QString *text);

signals:
    void changed();

private:
    void set(const QString &state, const QString &message);
    void pollStatus();

    static inline UpdateController *s_instance = nullptr;
    QString m_current, m_updater, m_statusPath;
    QString m_latest, m_notes, m_state, m_message;
    QNetworkAccessManager *m_network = nullptr;
    QTimer m_checkTimer, m_statusTimer;
    QDateTime m_statusBefore; // the status file's time when UPDATE was pressed (an older one is a past run's)
};
