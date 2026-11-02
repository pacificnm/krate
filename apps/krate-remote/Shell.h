#pragma once

#include <functional>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// Commands on the player over SSH (docs/REMOTE.md): the system's ssh, so the
// player is reached as from a terminal (~/.ssh/config, known hosts, keys).
// One connection is opened and kept (ssh's ControlMaster) so each command
// doesn't log in again. A password, if the player asks for one, is handed to
// ssh by this app run as its askpass helper (askpassMain()).
class Shell : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        int exitCode = -1;
        QByteArray out;
        QString error; // ssh's or the command's stderr, trimmed
        bool ok() const { return exitCode == 0; }
    };
    using Done = std::function<void(const Result &)>;

    explicit Shell(QObject *parent = nullptr);
    ~Shell() override;

    void setTarget(const QString &host, const QString &user, int port);
    QString destination() const; // "jaimie@krate.local"
    void setPassword(const QString &password) { m_password = password; }
    bool hasPassword() const { return !m_password.isEmpty(); }

    // Runs `command` (POSIX sh) on the player; `input` is its stdin.
    void run(const QString &command, Done done, const QByteArray &input = {});
    // The same, for streaming (the caller writes and reads it, and deletes it).
    QProcess *start(const QString &command);
    // Closes the kept connection.
    void disconnect();

    // A string quoted for sh: 'it'\''s'.
    static QString quote(const QString &s);
    // The key this app logs in with once set up (~/.ssh/krate-remote).
    static QString keyFile();

    // The askpass helper: ssh runs this app with the prompt (and
    // KRATE_REMOTE_ASKPASS set); it prints the password the app gave its ssh,
    // for a password prompt only.
    static bool isAskpass();
    static int askpassMain(int argc, char **argv);

    // ssh, or KRATE_REMOTE_SSH (tests: a stand-in that runs the command here).
    static QString program();

private:
    QStringList arguments(const QString &command) const;
    void prepare(QProcess *process, const QString &command) const;

    QString m_host, m_user;
    int m_port = 22;
    QString m_password;
    QString m_controlPath;
};
