#include "RemoteFiles.h"

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJSEngine>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

namespace {

RemoteFiles *s_instance = nullptr;
constexpr qint64 kChunk = 256 * 1024;
constexpr qint64 kAhead = 1024 * 1024; // written ahead of ssh, at most
const QString kPart = QStringLiteral(".krate-remote-part");

QString parentOf(const QString &path)
{
    const QString p = QFileInfo(path).path();
    return p.isEmpty() ? QStringLiteral("/") : p;
}

// `path`, or "path (2)", "path (3)"... whichever is free here.
QString freeLocal(const QString &path)
{
    if (!QFileInfo::exists(path))
        return path;
    const QFileInfo info(path);
    const QString base = info.completeBaseName(), suffix = info.suffix();
    for (int n = 2;; ++n) {
        const QString name = info.isDir() || suffix.isEmpty() ? QStringLiteral("%1 (%2)").arg(info.fileName()).arg(n)
                                                              : QStringLiteral("%1 (%2).%3").arg(base).arg(n).arg(suffix);
        const QString candidate = QDir(info.path()).filePath(name);
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
}

} // namespace

RemoteFiles::RemoteFiles(Shell &shell, QObject *parent)
    : QObject(parent)
    , m_shell(shell)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloads.isEmpty())
        downloads = QDir::homePath();
    m_downloadFolder = QSettings().value(QStringLiteral("downloadFolder"), downloads).toString();
    m_tick.setInterval(150);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        if (m_current >= 0 && m_process && m_transfers[m_current].upload && m_file)
            m_transfers[m_current].done = m_file->pos() - m_process->bytesToWrite();
        if (m_dirty || m_current >= 0) {
            m_dirty = false;
            emit transfersChanged();
        }
    });
}

RemoteFiles::~RemoteFiles()
{
    s_instance = nullptr;
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
        delete m_process;
    }
}

RemoteFiles *RemoteFiles::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void RemoteFiles::setDownloadFolder(const QString &folder)
{
    if (folder == m_downloadFolder || folder.isEmpty())
        return;
    m_downloadFolder = folder;
    QSettings().setValue(QStringLiteral("downloadFolder"), folder);
    emit downloadFolderChanged();
}

void RemoteFiles::setError(const QString &error)
{
    m_error = error;
    emit errorChanged();
}

QString RemoteFiles::child(const QString &name) const { return m_path == QLatin1String("/") ? u'/' + name : m_path + u'/' + name; }

bool RemoteFiles::exists(const QString &name) const
{
    return std::any_of(m_entries.begin(), m_entries.end(),
                       [&name](const QVariant &e) { return e.toMap().value(QStringLiteral("name")).toString() == name; });
}

void RemoteFiles::reset()
{
    cancelTransfers();
    m_path.clear();
    m_entries.clear();
    setError({});
    emit listed();
}

// ---- Browsing

void RemoteFiles::open(const QString &path)
{
    if (!path.startsWith(u'/'))
        return;
    const QString clean = QDir::cleanPath(path);
    const int listing = ++m_listing;
    m_loading = true;
    emit loadingChanged();
    const QString command = QStringLiteral("p=%1; [ -d \"$p\" ] || { echo \"$p isn't a folder there\" >&2; exit 2; }; "
                                           "find \"$p\" -mindepth 1 -maxdepth 1 -printf '%y\\0%Y\\0%s\\0%T@\\0%f\\0'")
                                .arg(Shell::quote(clean));
    m_shell.run(command, [this, clean, listing](const Shell::Result &r) {
        if (listing != m_listing)
            return;
        m_loading = false;
        emit loadingChanged();
        if (!r.ok() && r.out.isEmpty()) {
            setError(r.error.isEmpty() ? QStringLiteral("Couldn't open %1.").arg(clean) : r.error.section(u'\n', -1));
            return;
        }
        // Unreadable entries (find's complaints) still list what could be read;
        // what went wrong before a refresh stays said.
        if (!r.ok())
            setError(r.error.section(u'\n', -1));
        else if (clean != m_path)
            setError({});
        const QList<QByteArray> fields = r.out.split('\0');
        struct Entry
        {
            QString name;
            bool dir, link;
            qint64 size;
            double modified;
        };
        QList<Entry> list;
        for (qsizetype i = 0; i + 4 < fields.size(); i += 5)
            list.append({QString::fromUtf8(fields[i + 4]), fields[i + 1] == "d", fields[i] == "l", fields[i + 2].toLongLong(),
                         fields[i + 3].toDouble()});
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(list.begin(), list.end(), [&collator](const Entry &a, const Entry &b) {
            if (a.dir != b.dir)
                return a.dir;
            return collator.compare(a.name, b.name) < 0;
        });
        m_entries.clear();
        for (const Entry &e : std::as_const(list))
            m_entries << QVariantMap{
                {QStringLiteral("name"), e.name},
                {QStringLiteral("dir"), e.dir},
                {QStringLiteral("link"), e.link},
                {QStringLiteral("size"), e.size},
                {QStringLiteral("modified"), qint64(e.modified * 1000)},
            };
        m_path = clean;
        emit listed();
    });
}

void RemoteFiles::up()
{
    if (!m_path.isEmpty() && m_path != QLatin1String("/"))
        open(parentOf(m_path));
}

void RemoteFiles::makeFolder(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(u'/') || n == QLatin1String(".") || n == QLatin1String(".."))
        return;
    m_shell.run(QStringLiteral("mkdir -- %1").arg(Shell::quote(child(n))), [this](const Shell::Result &r) {
        setError(r.ok() ? QString() : r.error);
        refresh();
    });
}

void RemoteFiles::rename(const QString &from, const QString &to)
{
    const QString n = to.trimmed();
    if (n.isEmpty() || n == from || n.contains(u'/') || n == QLatin1String(".") || n == QLatin1String(".."))
        return;
    const QString command = QStringLiteral("a=%1; b=%2; if [ -e \"$b\" ] || [ -L \"$b\" ]; then echo \"There's a %3 already.\" >&2; exit 1; fi; "
                                           "mv -- \"$a\" \"$b\"")
                                .arg(Shell::quote(child(from)), Shell::quote(child(n)), n);
    m_shell.run(command, [this](const Shell::Result &r) {
        setError(r.ok() ? QString() : r.error);
        refresh();
    });
}

void RemoteFiles::remove(const QStringList &names)
{
    QStringList paths;
    for (const QString &n : names)
        if (!n.isEmpty() && !n.contains(u'/') && n != QLatin1String(".") && n != QLatin1String(".."))
            paths << Shell::quote(child(n));
    if (paths.isEmpty())
        return;
    m_shell.run(QStringLiteral("rm -rf -- ") + paths.join(u' '), [this](const Shell::Result &r) {
        setError(r.ok() ? QString() : r.error);
        refresh();
    });
}

// ---- Transfers

QVariantList RemoteFiles::transfers() const
{
    QVariantList list;
    for (const Transfer &t : m_transfers)
        list << QVariantMap{
            {QStringLiteral("name"), t.name},   {QStringLiteral("upload"), t.upload}, {QStringLiteral("done"), t.done},
            {QStringLiteral("total"), t.total}, {QStringLiteral("state"), t.state},   {QStringLiteral("error"), t.error},
        };
    return list;
}

bool RemoteFiles::transferring() const { return m_current >= 0; }

void RemoteFiles::queue(Transfer t)
{
    m_transfers.push_back(std::move(t));
    m_dirty = true;
    if (!m_tick.isActive())
        m_tick.start();
    if (m_current < 0)
        next();
}

void RemoteFiles::upload(const QList<QUrl> &urls)
{
    if (m_path.isEmpty())
        return;
    for (const QUrl &url : urls) {
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) { // all it holds, kept in its folders
            const QDir top(info.absoluteFilePath());
            QDirIterator it(top.path(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo f(it.next());
                const QString rel = info.fileName() + u'/' + top.relativeFilePath(f.absoluteFilePath());
                queue({true, f.absoluteFilePath(), child(rel), rel, f.size()});
            }
        } else if (info.isFile()) {
            queue({true, info.absoluteFilePath(), child(info.fileName()), info.fileName(), info.size()});
        }
    }
}

void RemoteFiles::download(const QStringList &names)
{
    for (const QString &name : names) {
        bool dir = false;
        qint64 size = 0;
        for (const QVariant &e : std::as_const(m_entries)) {
            const QVariantMap m = e.toMap();
            if (m.value(QStringLiteral("name")).toString() == name) {
                dir = m.value(QStringLiteral("dir")).toBool();
                size = m.value(QStringLiteral("size")).toLongLong();
            }
        }
        const QString local = freeLocal(QDir(m_downloadFolder).filePath(name));
        if (!dir) {
            queue({false, local, child(name), name, size});
            continue;
        }
        // A folder: what it holds, listed first.
        const QString from = child(name);
        m_shell.run(QStringLiteral("find %1 -type f -printf '%s\\0%P\\0'").arg(Shell::quote(from)), [this, from, local, name](const Shell::Result &r) {
            const QList<QByteArray> fields = r.out.split('\0');
            for (qsizetype i = 0; i + 1 < fields.size(); i += 2) {
                const QString rel = QString::fromUtf8(fields[i + 1]);
                queue({false, local + u'/' + rel, from + u'/' + rel, name + u'/' + rel, fields[i].toLongLong()});
            }
            if (!r.ok())
                setError(r.error.section(u'\n', -1));
        });
    }
}

void RemoteFiles::next()
{
    m_current = -1;
    for (size_t i = 0; i < m_transfers.size(); ++i)
        if (m_transfers[i].state == QLatin1String("waiting")) {
            m_current = int(i);
            break;
        }
    if (m_current < 0) {
        m_dirty = true;
        emit transfersChanged();
        if (m_uploadedHere) {
            m_uploadedHere = false;
            refresh();
        }
        emit transfersDone();
        return;
    }
    Transfer &t = m_transfers[m_current];
    t.state = QStringLiteral("sending");
    m_dirty = true;
    if (t.upload)
        startUpload(t);
    else
        startDownload(t);
}

void RemoteFiles::startUpload(Transfer &t)
{
    m_file = std::make_unique<QFile>(t.local);
    if (!m_file->open(QIODevice::ReadOnly)) {
        finish(QStringLiteral("Can't read ") + t.local);
        return;
    }
    t.total = m_file->size();
    // Renamed into place only whole: a cut-off ssh still ends cat's input
    // (as if the file ended there), so the size is checked first.
    const QString command = QStringLiteral("f=%1; n=%2; p=\"$f%3\"; mkdir -p -- \"$(dirname -- \"$f\")\" && cat > \"$p\" && "
                                           "[ \"$(wc -c < \"$p\")\" -eq \"$n\" ] && mv -f -- \"$p\" \"$f\" || "
                                           "{ rm -f -- \"$p\"; echo \"$f: not sent whole\" >&2; exit 1; }")
                                .arg(Shell::quote(t.remote), QString::number(t.total), kPart); // one pass: a name may hold "%2"
    m_process = m_shell.start(command);
    connect(m_process, &QProcess::bytesWritten, this, &RemoteFiles::feed);
    connect(m_process, &QProcess::started, this, &RemoteFiles::feed);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString err = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        finish(status == QProcess::NormalExit && code == 0 ? QString() : (err.isEmpty() ? QStringLiteral("It stopped (%1).").arg(code) : err.section(u'\n', -1)));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(QStringLiteral("ssh isn't installed (the openssh-client package)"));
    });
    if (parentOf(t.remote) == m_path || parentOf(t.remote).startsWith(m_path + u'/'))
        m_uploadedHere = true;
}

void RemoteFiles::feed()
{
    if (!m_process || !m_file || !m_file->isOpen())
        return;
    const qint64 total = m_transfers[m_current].total; // what the size check expects
    while (m_file->pos() < total && m_process->bytesToWrite() < kAhead) {
        const QByteArray chunk = m_file->read(std::min(kChunk, total - m_file->pos()));
        if (chunk.isEmpty())
            break;
        m_process->write(chunk);
    }
    if (m_file->pos() >= total && m_process->state() == QProcess::Running) {
        m_file->close();
        m_process->closeWriteChannel(); // once what's written has gone
    }
}

void RemoteFiles::startDownload(Transfer &t)
{
    QDir().mkpath(QFileInfo(t.local).path());
    m_file = std::make_unique<QFile>(t.local + kPart);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finish(QStringLiteral("Can't write ") + m_file->fileName());
        return;
    }
    m_process = m_shell.start(QStringLiteral("cat -- %1").arg(Shell::quote(t.remote)));
    m_process->closeWriteChannel();
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        const QByteArray data = m_process->readAllStandardOutput();
        m_file->write(data);
        m_transfers[m_current].done += data.size();
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QByteArray rest = m_process->readAllStandardOutput();
        m_file->write(rest);
        m_transfers[m_current].done += rest.size();
        const QString err = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (status != QProcess::NormalExit || code != 0) {
            finish(err.isEmpty() ? QStringLiteral("It stopped (%1).").arg(code) : err.section(u'\n', -1));
            return;
        }
        if (!m_file->flush()) {
            finish(QStringLiteral("Couldn't write ") + m_file->fileName() + QStringLiteral(" (is the disk full?)"));
            return;
        }
        m_file->close();
        const Transfer &t = m_transfers[m_current];
        QFile::remove(t.local);
        if (!QFile::rename(t.local + kPart, t.local)) {
            finish(QStringLiteral("Couldn't name it ") + t.local);
            return;
        }
        finish({});
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(QStringLiteral("ssh isn't installed (the openssh-client package)"));
    });
}

void RemoteFiles::finish(const QString &error)
{
    if (m_current < 0)
        return;
    Transfer &t = m_transfers[m_current];
    if (t.state == QLatin1String("sending")) {
        t.state = error.isEmpty() ? QStringLiteral("done") : QStringLiteral("failed");
        t.error = error;
        if (error.isEmpty())
            t.done = t.total = std::max(t.total, t.done);
    }
    if (!t.upload && t.state != QLatin1String("done") && m_file) {
        m_file->close();
        m_file->remove(); // the part
    }
    m_file.reset();
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_dirty = true;
    next();
}

void RemoteFiles::cancelTransfers()
{
    bool any = false;
    for (Transfer &t : m_transfers)
        if (t.state == QLatin1String("waiting")) {
            t.state = QStringLiteral("cancelled");
            any = true;
        }
    if (m_current >= 0) {
        Transfer &t = m_transfers[m_current];
        t.state = QStringLiteral("cancelled");
        if (t.upload) // what got there, never under its name
            m_shell.run(QStringLiteral("rm -f -- %1").arg(Shell::quote(t.remote + kPart)), {});
        if (m_process) {
            m_process->disconnect(this);
            m_process->kill();
        }
        finish({});
        any = true;
    }
    if (any) {
        m_dirty = true;
        emit transfersChanged();
    }
}

void RemoteFiles::clearFinished()
{
    std::deque<Transfer> kept;
    for (Transfer &t : m_transfers)
        if (t.state == QLatin1String("waiting") || t.state == QLatin1String("sending"))
            kept.push_back(std::move(t));
    m_transfers = std::move(kept);
    m_current = -1;
    for (size_t i = 0; i < m_transfers.size(); ++i)
        if (m_transfers[i].state == QLatin1String("sending"))
            m_current = int(i);
    if (m_transfers.empty())
        m_tick.stop();
    emit transfersChanged();
}
