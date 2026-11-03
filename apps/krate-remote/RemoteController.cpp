#include "RemoteController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QProcess>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QVariantMap>

#include "RemoteSettings.h"

Q_LOGGING_CATEGORY(lcRemote, "krate.remote")

namespace loc = krate::locations;
using loc::Folder;

namespace {

// What the player is: tab-separated lines.
constexpr char kProbe[] = R"sh(printf 'home\t%s\n' "$HOME"
printf 'config\t%s\n' "${XDG_CONFIG_HOME:-$HOME/.config}"
printf 'data\t%s\n' "${XDG_DATA_HOME:-$HOME/.local/share}"
# The Music folder as Qt finds it: user-dirs.dirs, else ~/Music.
music=$(. "${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs" 2>/dev/null; printf %s "$XDG_MUSIC_DIR")
printf 'music\t%s\n' "${music:-$HOME/Music}"
printf 'host\t%s\n' "$(hostname 2>/dev/null || uname -n)"
printf 'version\t%s\n' "$(dpkg-query -W -f '${Version}' krate 2>/dev/null)"
if systemctl is-active --quiet "krate-kiosk@$(id -un)" 2>/dev/null; then s=kiosk
elif pgrep -x krate >/dev/null 2>&1; then s=running
else s=stopped; fi
printf 'krate\t%s\n' "$s"
)sh";

// Krate's own folder is the kiosk's to start again; elsewhere it isn't.
constexpr char kRestart[] = R"sh(if systemctl is-active --quiet "krate-kiosk@$(id -un)" 2>/dev/null; then
  pkill -TERM -x krate && echo restarted || echo stopped
elif pgrep -x krate >/dev/null 2>&1; then echo desktop
else echo stopped; fi
)sh";

// A public key read from stdin, added to authorized_keys once.
constexpr char kAddKey[] = R"sh(umask 077
read -r key
mkdir -p "$HOME/.ssh" && touch "$HOME/.ssh/authorized_keys" || exit 1
grep -qxF "$key" "$HOME/.ssh/authorized_keys" || printf '%s\n' "$key" >> "$HOME/.ssh/authorized_keys"
)sh";

} // namespace

RemoteController::RemoteController(Shell &shell, QObject *parent)
    : QObject(parent)
    , m_shell(shell)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    QSettings saved;
    m_host = saved.value(QStringLiteral("host"), QStringLiteral("krate.local")).toString();
    m_user = saved.value(QStringLiteral("user"), qEnvironmentVariable("USER")).toString();
    m_port = saved.value(QStringLiteral("port"), 22).toInt();
    // Krate updated or restarted on the player: the top bar follows within seconds.
    m_poll.setInterval(20000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (m_state != QLatin1String("connected") || m_busy > 0)
            return;
        run(QString::fromLatin1(kProbe), [this](const Shell::Result &r) {
            if (r.ok())
                readInfo(r);
        });
    });
}

RemoteController::~RemoteController() { s_instance = nullptr; }

RemoteController *RemoteController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void RemoteController::setState(const QString &state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

void RemoteController::setMessage(const QString &message)
{
    m_message = message;
    if (!message.isEmpty())
        qCInfo(lcRemote).noquote() << message;
    emit messageChanged();
}

QString RemoteController::failure(const Shell::Result &r) const
{
    // ssh's own errors say what to do least of all: say it plainly.
    const QString e = r.error;
    if (r.exitCode == 255) {
        if (e.contains(QLatin1String("Permission denied")))
            return QStringLiteral("The player didn't accept the login: check the user and password.");
        if (e.contains(QLatin1String("Could not resolve")) || e.contains(QLatin1String("Name or service not known")))
            return QStringLiteral("No computer called %1 was found on the network.").arg(m_host);
        if (e.contains(QLatin1String("timed out")) || e.contains(QLatin1String("No route")))
            return QStringLiteral("%1 didn't answer: is it on, and on this network?").arg(m_host);
        if (e.contains(QLatin1String("Connection refused")))
            return QStringLiteral("%1 doesn't take SSH logins (is openssh-server installed there?).").arg(m_host);
        if (e.contains(QLatin1String("REMOTE HOST IDENTIFICATION HAS CHANGED")) || e.contains(QLatin1String("Host key verification failed")))
            return QStringLiteral("%1's identity isn't the one this computer knows (reinstalled?). If that's expected: "
                                  "ssh-keygen -R %1").arg(m_host);
    }
    return e.isEmpty() ? QStringLiteral("It didn't work (%1).").arg(r.exitCode) : e.section(u'\n', -1);
}

void RemoteController::run(const QString &command, Shell::Done done, const QByteArray &input)
{
    ++m_busy;
    if (m_busy == 1)
        emit busyChanged();
    const int generation = m_generation;
    m_shell.run(
        command,
        [this, done, generation](const Shell::Result &r) {
            if (--m_busy == 0)
                emit busyChanged();
            if (generation != m_generation)
                return; // for a connection that's gone
            done(r);
        },
        input);
}

// ---- Connecting

void RemoteController::connectTo(const QString &host, const QString &user, int port, const QString &password)
{
    m_host = host.trimmed();
    m_user = user.trimmed();
    m_port = port > 0 ? port : 22;
    emit targetChanged();
    if (m_host.isEmpty()) {
        setMessage(QStringLiteral("Which computer? Its name on the network (krate.local) or its address."));
        return;
    }
    QSettings saved;
    saved.setValue(QStringLiteral("host"), m_host);
    saved.setValue(QStringLiteral("user"), m_user);
    saved.setValue(QStringLiteral("port"), m_port);

    ++m_generation;
    m_shell.setTarget(m_host, m_user, m_port);
    m_shell.setPassword(password);
    m_needsPassword = false;
    setState(QStringLiteral("connecting"));
    setMessage({});
    run(QString::fromLatin1(kProbe), [this](const Shell::Result &r) {
        if (!r.ok()) {
            const bool refused = r.exitCode == 255 && r.error.contains(QLatin1String("Permission denied"));
            m_needsPassword = refused;
            setState(QStringLiteral("disconnected"));
            setMessage(refused && !m_shell.hasPassword() ? QStringLiteral("The player wants a password (or set up a key once logged in).")
                                                         : failure(r));
            return;
        }
        readInfo(r);
        setState(QStringLiteral("connected"));
        m_poll.start();
        setMessage(QStringLiteral("Connected to %1.").arg(m_hostName.isEmpty() ? m_host : m_hostName));
        readSettings([this] {
            readReports();
            emit connected();
        });
    });
}

void RemoteController::disconnectFrom()
{
    m_poll.stop();
    ++m_generation;
    m_shell.disconnect();
    m_shell.setPassword({});
    m_edits.clear();
    m_editing = false;
    m_needsPassword = false;
    setState(QStringLiteral("disconnected"));
    setMessage({});
}

void RemoteController::refresh()
{
    if (m_state != QLatin1String("connected"))
        return;
    run(QString::fromLatin1(kProbe), [this](const Shell::Result &r) {
        if (!r.ok()) {
            setMessage(failure(r));
            return;
        }
        readInfo(r);
        readSettings([this] { readReports(); });
    });
}

void RemoteController::readInfo(const Shell::Result &r)
{
    QString dataHome, music;
    for (const QString &line : QString::fromUtf8(r.out).split(u'\n')) {
        const QString key = line.section(u'\t', 0, 0), value = line.section(u'\t', 1);
        if (key == QLatin1String("home"))
            m_home = value;
        else if (key == QLatin1String("config"))
            m_configHome = value;
        else if (key == QLatin1String("data"))
            dataHome = value;
        else if (key == QLatin1String("music"))
            music = value;
        else if (key == QLatin1String("host"))
            m_hostName = value;
        else if (key == QLatin1String("version"))
            m_version = value;
        else if (key == QLatin1String("krate"))
            m_krateState = value;
    }
    m_roots = {dataHome, music};
    emit infoChanged();
}

// ---- Settings

void RemoteController::readSettings(std::function<void()> then)
{
    run(QStringLiteral("cat -- %1 2>/dev/null; true").arg(Shell::quote(settingsPath())), [this, then](const Shell::Result &r) {
        if (!r.ok()) {
            setMessage(failure(r));
            return;
        }
        m_settings = r.out.trimmed().isEmpty() ? QJsonObject() : QJsonDocument::fromJson(r.out).object();
        emit settingsChanged();
        if (then)
            then();
    });
}

void RemoteController::edit(std::function<QJsonObject(QJsonObject)> change)
{
    if (m_state != QLatin1String("connected"))
        return;
    m_edits.push_back(std::move(change));
    if (!m_editing)
        nextEdit();
}

void RemoteController::nextEdit()
{
    if (m_edits.empty()) {
        m_editing = false;
        return;
    }
    m_editing = true;
    auto change = std::move(m_edits.front());
    m_edits.pop_front();
    const QString file = settingsPath();
    run(QStringLiteral("cat -- %1 2>/dev/null; true").arg(Shell::quote(file)), [this, change, file](const Shell::Result &r) {
        if (!r.ok()) {
            setMessage(failure(r));
            nextEdit();
            return;
        }
        QJsonObject json;
        if (!r.out.trimmed().isEmpty()) {
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(r.out, &error);
            if (!doc.isObject()) { // never write over what can't be read
                setMessage(QStringLiteral("%1 there isn't readable (%2): nothing changed.").arg(file, error.errorString()));
                m_edits.clear();
                m_editing = false;
                return;
            }
            json = doc.object();
        }
        json = change(json);
        json[QStringLiteral("version")] = 1;
        const QString part = file + QStringLiteral(".remote");
        const QString write = QStringLiteral("mkdir -p -- %1 && cat > %2 && mv -f -- %2 %3")
                                  .arg(Shell::quote(QFileInfo(file).path()), Shell::quote(part), Shell::quote(file));
        run(write, [this, json](const Shell::Result &w) {
            if (w.ok()) {
                m_settings = json;
                emit settingsChanged();
            } else {
                setMessage(failure(w));
            }
            nextEdit();
        }, QJsonDocument(json).toJson());
    });
}

QVariantList RemoteController::folders() const { return remote::folderRows(m_settings, m_roots); }

QStringList RemoteController::pluginFolders() const
{
    QStringList out;
    for (const QJsonValue &v : m_settings.value(u"pluginFolders").toArray())
        out << v.toString();
    return out;
}

QString RemoteController::folder(Folder f) const { return loc::folder(f, m_settings, m_roots); }

QString RemoteController::logPath() const
{
    return m_roots.dataHome.isEmpty() ? QString() : folder(Folder::Data) + QStringLiteral("/krate.log");
}

QVariantList RemoteController::places() const
{
    QVariantList list;
    if (m_home.isEmpty())
        return list;
    auto add = [&list](const QString &label, const QString &path) {
        list << QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("path"), path}};
    };
    add(QStringLiteral("Home"), m_home);
    // Krate's sounds folder: what lands there is in the Sounds library by itself.
    add(QStringLiteral("Sounds (the library's)"), folder(Folder::Data) + QStringLiteral("/sounds"));
    // Krunch's amp captures (.nam): what lands there is in its AMP list by itself.
    add(QStringLiteral("Amp captures (Krunch)"), folder(Folder::Instruments) + QStringLiteral("/krunch/captures"));
    // Krunch's amp heads (.json, from krunch-fit): likewise in its AMP list.
    add(QStringLiteral("Amp heads (Krunch)"), folder(Folder::Instruments) + QStringLiteral("/krunch/heads"));
    for (const Folder f : loc::kShown)
        add(loc::label(f), folder(f));
    for (const QString &p : pluginFolders())
        add(QStringLiteral("Plugins: ") + QFileInfo(p).fileName(), p);
    add(QStringLiteral("CLAP plugins (~/.clap)"), m_home + QStringLiteral("/.clap"));
    return list;
}

void RemoteController::setFolder(const QString &key, const QString &path, bool move)
{
    const loc::Roots roots = m_roots;
    // Moving into a folder with files in it: Krate would refuse at its start; say so now.
    const QJsonObject next = remote::setFolder(m_settings, key, path, move, roots);
    QString to;
    for (const QVariant &row : remote::folderRows(next, roots))
        if (row.toMap().value(QStringLiteral("key")).toString() == key)
            to = row.toMap().value(QStringLiteral("pending")).toString();
    auto apply = [this, key, path, move, roots] {
        edit([key, path, move, roots](QJsonObject s) { return remote::setFolder(s, key, path, move, roots); });
    };
    if (!move || to.isEmpty() || key == loc::key(Folder::Instruments)) {
        apply();
        return;
    }
    run(QStringLiteral("d=%1; [ ! -d \"$d\" ] || [ -z \"$(ls -A -- \"$d\")\" ]").arg(Shell::quote(to)), [this, apply, to](const Shell::Result &r) {
        if (r.exitCode == 1) {
            setMessage(QStringLiteral("%1 has files in it already: Krate only moves into a new or empty folder. "
                                      "Pick another, or USE WHAT'S THERE.").arg(to));
            return;
        }
        apply();
    });
}

void RemoteController::cancel(const QString &key)
{
    edit([key](QJsonObject s) { return remote::cancel(s, key); });
}

void RemoteController::setPluginFolders(const QStringList &folders)
{
    edit([folders](QJsonObject s) { return remote::setPluginFolders(s, folders); });
}

void RemoteController::addPluginFolder(const QString &folder)
{
    edit([folder](QJsonObject s) {
        QStringList list;
        for (const QJsonValue &v : s.value(u"pluginFolders").toArray())
            list << v.toString();
        return remote::setPluginFolders(s, list << folder);
    });
}

void RemoteController::removePluginFolder(const QString &folder)
{
    edit([folder](QJsonObject s) {
        QStringList list;
        for (const QJsonValue &v : s.value(u"pluginFolders").toArray())
            if (v.toString() != folder)
                list << v.toString();
        return remote::setPluginFolders(s, list);
    });
}

void RemoteController::setMidiInputs(const QString &pattern)
{
    edit([pattern](QJsonObject s) {
        s[QStringLiteral("midiInputs")] = pattern.trimmed();
        return s;
    });
}

void RemoteController::setProgramChangeFocus(bool on)
{
    edit([on](QJsonObject s) {
        s[QStringLiteral("programChangeFocus")] = on;
        return s;
    });
}

void RemoteController::setAutoCheckUpdates(bool on)
{
    edit([on](QJsonObject s) {
        s[QStringLiteral("autoCheckUpdates")] = on;
        return s;
    });
}

// ---- Krate itself

void RemoteController::restartKrate()
{
    if (m_state != QLatin1String("connected"))
        return;
    run(QString::fromLatin1(kRestart), [this](const Shell::Result &r) {
        const QString answer = QString::fromUtf8(r.out).trimmed();
        if (answer == QLatin1String("restarted")) {
            setMessage(QStringLiteral("Krate is starting again (a few seconds)."));
            QTimer::singleShot(8000, this, &RemoteController::refresh); // its folders moved, its new state
        } else if (answer == QLatin1String("desktop")) {
            setMessage(QStringLiteral("Krate runs on the player's desktop, not its kiosk: quit it and start it there "
                                      "(or use its own RESTART KRATE NOW)."));
        } else if (answer == QLatin1String("stopped")) {
            setMessage(QStringLiteral("Krate isn't running there: whatever waits happens when it starts."));
        } else {
            setMessage(failure(r));
        }
    });
}

void RemoteController::readReports()
{
    const QString dir = folder(Folder::Data) + QStringLiteral("/crashes");
    run(QStringLiteral("[ -d %1 ] && find %1 -maxdepth 1 -type f -name '*.txt' -printf '%T@\\t%f\\n'; true").arg(Shell::quote(dir)),
        [this, dir](const Shell::Result &r) {
            struct Row
            {
                double when;
                QString name;
            };
            QList<Row> rows;
            for (const QString &line : QString::fromUtf8(r.out).split(u'\n', Qt::SkipEmptyParts))
                rows.append({line.section(u'\t', 0, 0).toDouble(), line.section(u'\t', 1)});
            std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.when > b.when; });
            m_reports.clear();
            for (const Row &row : std::as_const(rows))
                m_reports << QVariantMap{
                    {QStringLiteral("name"), row.name},
                    {QStringLiteral("path"), dir + u'/' + row.name},
                    {QStringLiteral("when"), QDateTime::fromMSecsSinceEpoch(qint64(row.when * 1000)).toString(QStringLiteral("yyyy-MM-dd HH:mm"))},
                };
            emit reportsChanged();
        });
}

void RemoteController::readText(const QString &path)
{
    // The end of a big file (a long session's log) is what matters.
    run(QStringLiteral("tail -c 1000000 -- %1").arg(Shell::quote(path)), [this, path](const Shell::Result &r) {
        emit textRead(path, r.ok() ? QString::fromUtf8(r.out) : failure(r));
    });
}

void RemoteController::setUpKey()
{
    if (m_state != QLatin1String("connected"))
        return;
    const QString key = Shell::keyFile();
    auto install = [this, key] {
        QFile pub(key + QStringLiteral(".pub"));
        if (!pub.open(QIODevice::ReadOnly)) {
            setMessage(QStringLiteral("No %1.pub to put on the player.").arg(key));
            return;
        }
        run(QString::fromLatin1(kAddKey), [this](const Shell::Result &r) {
            if (!r.ok()) {
                setMessage(failure(r));
                return;
            }
            m_shell.setPassword({}); // the key from now on
            emit stateChanged();
            setMessage(QStringLiteral("Key login set up: no password from now on (the key is %1).").arg(Shell::keyFile()));
        }, pub.readAll().trimmed() + '\n');
    };
    if (QFileInfo::exists(key)) {
        install();
        return;
    }
    QDir().mkpath(QFileInfo(key).path());
    QFile::setPermissions(QFileInfo(key).path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    auto *keygen = new QProcess(this);
    connect(keygen, &QProcess::finished, this, [this, keygen, install](int code) {
        keygen->deleteLater();
        if (code != 0) {
            setMessage(QStringLiteral("ssh-keygen failed: ") + QString::fromUtf8(keygen->readAllStandardError()).trimmed());
            return;
        }
        install();
    });
    keygen->start(QStringLiteral("ssh-keygen"), {QStringLiteral("-q"), QStringLiteral("-t"), QStringLiteral("ed25519"), QStringLiteral("-N"),
                                                 QString(), QStringLiteral("-C"), QStringLiteral("krate-remote@") + QSysInfo::machineHostName(),
                                                 QStringLiteral("-f"), key});
}
