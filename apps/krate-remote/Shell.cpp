#include "Shell.h"

#include <cstdio>
#include <cstdlib>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

constexpr char kPasswordVar[] = "KRATE_REMOTE_PASSWORD";

} // namespace

Shell::Shell(QObject *parent)
    : QObject(parent)
{
    // Short (a socket's path has a limit): /run/user/<uid>, else /tmp.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    m_controlPath = dir + QStringLiteral("/krate-remote-%C");
}

Shell::~Shell() { disconnect(); }

void Shell::setTarget(const QString &host, const QString &user, int port)
{
    if (host != m_host || user != m_user || port != m_port)
        disconnect();
    m_host = host;
    m_user = user;
    m_port = port;
}

QString Shell::destination() const { return m_user.isEmpty() ? m_host : m_user + u'@' + m_host; }

QString Shell::program()
{
    const QString fake = qEnvironmentVariable("KRATE_REMOTE_SSH");
    return fake.isEmpty() ? QStringLiteral("ssh") : fake;
}

QString Shell::keyFile() { return QDir::homePath() + QStringLiteral("/.ssh/krate-remote"); }

QString Shell::quote(const QString &s)
{
    QString out = s;
    out.replace(u'\'', QStringLiteral("'\\''"));
    return u'\'' + out + u'\'';
}

QStringList Shell::arguments(const QString &command) const
{
    QStringList args;
    auto option = [&args](const QString &o) { args << QStringLiteral("-o") << o; };
    // No questions on a terminal there isn't: a password through askpass, a
    // new player's host key taken (a changed one still refused).
    option(m_password.isEmpty() ? QStringLiteral("BatchMode=yes") : QStringLiteral("BatchMode=no"));
    option(QStringLiteral("StrictHostKeyChecking=accept-new"));
    option(QStringLiteral("ConnectTimeout=10"));
    option(QStringLiteral("ServerAliveInterval=15"));
    option(QStringLiteral("NumberOfPasswordPrompts=1"));
    option(QStringLiteral("ControlMaster=auto"));
    option(QStringLiteral("ControlPath=") + m_controlPath);
    option(QStringLiteral("ControlPersist=600"));
    if (QFileInfo::exists(keyFile()))
        args << QStringLiteral("-i") << keyFile();
    args << QStringLiteral("-p") << QString::number(m_port) << destination();
    // Whatever the login shell is: sh.
    args << QStringLiteral("exec sh -c ") + quote(command);
    return args;
}

void Shell::prepare(QProcess *process, const QString &command) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!m_password.isEmpty()) {
        env.insert(QStringLiteral("SSH_ASKPASS"), QCoreApplication::applicationFilePath());
        env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
        env.insert(QString::fromLatin1(kPasswordVar), m_password);
        env.insert(QStringLiteral("KRATE_REMOTE_ASKPASS"), QStringLiteral("1"));
    }
    process->setProcessEnvironment(env);
    process->setProgram(program());
    process->setArguments(arguments(command));
}

void Shell::run(const QString &command, Done done, const QByteArray &input)
{
    auto *process = new QProcess(this);
    prepare(process, command);
    connect(process, &QProcess::finished, this, [process, done](int code, QProcess::ExitStatus status) {
        Result r;
        r.exitCode = status == QProcess::NormalExit ? code : -1;
        r.out = process->readAllStandardOutput();
        r.error = QString::fromUtf8(process->readAllStandardError()).trimmed();
        process->deleteLater();
        if (done)
            done(r);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        process->deleteLater();
        if (done)
            done({-1, {}, QStringLiteral("ssh isn't installed (the openssh-client package)")});
    });
    process->start();
    if (!input.isEmpty())
        process->write(input);
    process->closeWriteChannel();
}

QProcess *Shell::start(const QString &command)
{
    auto *process = new QProcess;
    prepare(process, command);
    process->start();
    return process;
}

void Shell::disconnect()
{
    if (m_host.isEmpty() || !qEnvironmentVariableIsEmpty("KRATE_REMOTE_SSH"))
        return;
    QProcess::execute(program(), {QStringLiteral("-o"), QStringLiteral("ControlPath=") + m_controlPath, QStringLiteral("-O"),
                                  QStringLiteral("exit"), QStringLiteral("-p"), QString::number(m_port), destination()});
}

bool Shell::isAskpass() { return qEnvironmentVariableIsSet("KRATE_REMOTE_ASKPASS"); }

int Shell::askpassMain(int argc, char **argv)
{
    // Only a password: anything else (a host key's yes/no) gets no answer.
    const QString prompt = argc >= 2 ? QString::fromLocal8Bit(argv[argc - 1]) : QString();
    if (!prompt.contains(QLatin1String("password"), Qt::CaseInsensitive))
        return 1;
    const char *password = std::getenv(kPasswordVar);
    if (!password)
        return 1;
    std::fputs(password, stdout);
    std::fputs("\n", stdout);
    return 0;
}
