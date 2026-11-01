#include "app/CrashReports.h"

#include <array>
#include <csignal>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>

#include <cxxabi.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextStream>

namespace krate::crash {

namespace {

constexpr int kLogLines = 200;  // of the session's log, in a report
constexpr int kMaxFrames = 64;

// --- What the crash handler needs, made ready before any crash ----------------------------

char g_crashPath[4096] = {};
long g_mainThread = 0;
int g_quitPipe[2] = {-1, -1};
std::array<char, 1 << 16> g_altStack{}; // a stack overflow still gets its report

// Writing from the handler: only write(2), no allocation.
void put(int fd, const char *text) { (void)!write(fd, text, std::strlen(text)); }

void putNumber(int fd, unsigned long long value, int base = 10)
{
    char digits[32];
    int n = 0;
    do {
        const int d = int(value % unsigned(base));
        digits[n++] = char(d < 10 ? '0' + d : 'a' + d - 10);
        value /= unsigned(base);
    } while (value && n < 32);
    char out[34];
    int k = 0;
    while (n)
        out[k++] = digits[--n];
    out[k] = 0;
    put(fd, out);
}

const char *signalName(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGBUS: return "SIGBUS";
    default: return "?";
    }
}

void onCrash(int sig, siginfo_t *info, void *)
{
    // The handler was reset on the way in (SA_RESETHAND): a fault in here ends it.
    const int fd = ::open(g_crashPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd >= 0) {
        put(fd, "signal ");
        putNumber(fd, unsigned(sig));
        put(fd, " ");
        put(fd, signalName(sig));
        put(fd, "\ntime ");
        timespec now{}; // the precise clock (time() is the coarse one, a tick behind)
        clock_gettime(CLOCK_REALTIME, &now);
        putNumber(fd, static_cast<unsigned long long>(now.tv_sec) * 1000 + static_cast<unsigned long long>(now.tv_nsec / 1000000));
        const long thread = syscall(SYS_gettid);
        put(fd, "\nthread ");
        putNumber(fd, static_cast<unsigned long long>(thread));
        put(fd, thread == g_mainThread ? " main" : " other");
        char name[17] = {};
        if (prctl(PR_GET_NAME, name) == 0) {
            put(fd, " ");
            put(fd, name);
        }
        if (sig == SIGSEGV || sig == SIGBUS || sig == SIGFPE || sig == SIGILL) {
            put(fd, "\naddress 0x");
            putNumber(fd, reinterpret_cast<unsigned long long>(info ? info->si_addr : nullptr), 16);
        }
        put(fd, "\nstack\n");
        void *frames[kMaxFrames];
        const int n = backtrace(frames, kMaxFrames);
        // Past this handler and the kernel's way into it: where it crashed.
        constexpr int kSkip = 2;
        if (n > kSkip)
            backtrace_symbols_fd(frames + kSkip, n - kSkip, fd);
        ::close(fd);
    }
    raise(sig); // as it would have ended: the default action (a core, systemd's "dumped")
}

void onQuit(int)
{
    const char byte = 1;
    (void)!write(g_quitPipe[1], &byte, 1);
}

// --- The session's log ----------------------------------------------------------------------

std::mutex g_logMutex;
std::unique_ptr<QFile> g_log;
QtMessageHandler g_previousHandler = nullptr;

void onMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        const std::lock_guard lock(g_logMutex);
        if (g_log) {
            static const char *const kinds[] = {"debug", "warning", "critical", "fatal", "info"};
            const QByteArray line = QTime::currentTime().toString(u"HH:mm:ss.zzz").toUtf8() + ' '
                                    + kinds[std::min<int>(type, 4)] + ' '
                                    + (context.category ? QByteArray(context.category) + ": " : QByteArray())
                                    + message.toUtf8() + '\n';
            g_log->write(line);
            g_log->flush(); // a crash next must find it written
        }
    }
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

QString markerPath(const QString &appName, const QString &dir) { return dir + u'/' + appName + QStringLiteral(".running"); }
QString logPath(const QString &appName, const QString &dir) { return dir + u'/' + appName + QStringLiteral(".log"); }
QString crashesDir(const QString &dir) { return dir + QStringLiteral("/crashes"); }
QString pendingPath(const QString &dir) { return crashesDir(dir) + QStringLiteral("/pending.txt"); }

QString s_newReport;
QString s_marker;

QStringList lastLines(const QString &path, int count)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList lines = QString::fromUtf8(f.readAll()).split(u'\n');
    if (!lines.isEmpty() && lines.back().isEmpty())
        lines.removeLast();
    return lines.mid(std::max<qsizetype>(0, lines.size() - count));
}

QString describeSignal(const QString &name)
{
    if (name == u"SIGSEGV")
        return QStringLiteral("a segmentation fault (memory it shouldn't touch)");
    if (name == u"SIGABRT")
        return QStringLiteral("an abort (a fatal error or a failed check: see the log)");
    if (name == u"SIGFPE")
        return QStringLiteral("an arithmetic error (division by zero)");
    if (name == u"SIGILL")
        return QStringLiteral("an illegal instruction");
    if (name == u"SIGBUS")
        return QStringLiteral("a bus error (a bad memory access, or a file gone)");
    return name;
}

void prune(const QString &dir)
{
    QDir crashes(crashesDir(dir));
    const QStringList files = crashes.entryList({QStringLiteral("2*.txt")}, QDir::Files, QDir::Name | QDir::Reversed);
    for (qsizetype i = kKeep; i < files.size(); ++i)
        crashes.remove(files[i]);
}

} // namespace

QString readableFrame(const QString &line)
{
    static const QRegularExpression frame(QStringLiteral(R"(^(.*?)\((.*?)(\+0x[0-9a-fA-F]+)?\)\s*\[(0x[0-9a-fA-F]+)\]\s*$)"));
    const QRegularExpressionMatch m = frame.match(line.trimmed());
    if (!m.hasMatch())
        return line.trimmed();
    const QString module = QFileInfo(m.captured(1)).fileName();
    QString symbol = m.captured(2);
    if (!symbol.isEmpty()) {
        int status = -1;
        char *demangled = abi::__cxa_demangle(symbol.toLatin1().constData(), nullptr, nullptr, &status);
        if (status == 0 && demangled)
            symbol = QString::fromLatin1(demangled);
        std::free(demangled);
    }
    const QString offset = m.captured(3);
    if (symbol.isEmpty())
        return module + u' ' + (offset.isEmpty() ? m.captured(4) : offset);
    return symbol + (offset.isEmpty() ? QString() : u' ' + offset) + QStringLiteral("  (") + module + u')';
}

QString collect(const QString &appName, const QString &dir)
{
    const QString pending = pendingPath(dir), marker = markerPath(appName, dir);
    const bool crashed = QFileInfo::exists(pending), unfinished = QFileInfo::exists(marker);
    if (!crashed && !unfinished)
        return {};
    QDir().mkpath(crashesDir(dir));

    // The session: when it began, which version.
    QDateTime started;
    QString version;
    for (const QString &line : lastLines(marker, 10)) {
        if (line.startsWith(u"started "))
            started = QDateTime::fromString(line.mid(8), Qt::ISODate);
        else if (line.startsWith(u"version "))
            version = line.mid(8);
    }
    // The crash: signal, time, thread, address, stack.
    QString signal, thread, address;
    QDateTime when;
    QStringList stack;
    bool inStack = false;
    for (const QString &line : lastLines(pending, 1000)) {
        if (inStack)
            stack << readableFrame(line);
        else if (line.startsWith(u"signal "))
            signal = line.section(u' ', 2, 2);
        else if (line.startsWith(u"time "))
            when = QDateTime::fromMSecsSinceEpoch(line.mid(5).toLongLong());
        else if (line.startsWith(u"thread "))
            thread = line.mid(7);
        else if (line.startsWith(u"address "))
            address = line.mid(8);
        else if (line == u"stack")
            inStack = true;
    }

    const QString threadName = thread.section(u' ', 2);
    const bool mainThread = thread.section(u' ', 1, 1) == u"main";
    const QString where = thread.isEmpty() ? QString()
                          : mainThread     ? QStringLiteral("in the main (screen) thread")
                                           : QStringLiteral("in thread \"%1\" (not the screen's: the audio thread, or a helper)").arg(threadName);
    const QString summary = crashed ? QStringLiteral("Crashed: %1 %2").arg(describeSignal(signal), where)
                                    : QStringLiteral("Ended without finishing: no crash was caught (killed, frozen, out of memory, or the power went)");
    if (!when.isValid())
        when = started.isValid() ? started : QFileInfo(crashed ? pending : marker).lastModified();

    QString text;
    QTextStream out(&text);
    out << appName << " crash report\n\n";
    out << "What happened: " << summary << "\n";
    out << "When: " << when.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    if (started.isValid()) {
        out << "Session started: " << started.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (crashed) {
            const qint64 ran = std::max<qint64>(0, started.secsTo(when));
            out << " (it ran " << ran / 60 << " min " << ran % 60 << " s)";
        }
        out << "\n";
    }
    if (!version.isEmpty())
        out << "Version: " << version << "\n";
    if (crashed) {
        out << "Signal: " << signal << "\n";
        if (!thread.isEmpty())
            out << "Thread: " << thread.section(u' ', 0, 0) << ' ' << (mainThread ? "(main)" : "") << ' ' << threadName << "\n";
        if (!address.isEmpty())
            out << "Address: " << address << "\n";
        out << "\nCall stack (where it was, innermost first):\n";
        for (qsizetype i = 0; i < stack.size(); ++i)
            out << "  #" << i << "  " << stack[i] << "\n";
    }
    const QStringList log = lastLines(logPath(appName, dir), kLogLines);
    out << "\nThe session's last log lines (" << log.size() << "):\n";
    for (const QString &line : log)
        out << "  " << line << "\n";
    out.flush();

    QString path = crashesDir(dir) + u'/' + when.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")) + QStringLiteral(".txt");
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = crashesDir(dir) + u'/' + when.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")) + u'-' + QString::number(n)
               + QStringLiteral(".txt");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    f.write(text.toUtf8());
    f.close();
    QFile::remove(pending);
    QFile::remove(marker);
    prune(dir);
    return path;
}

void install(const QString &appName, const QString &dir)
{
    QDir().mkpath(crashesDir(dir));
    s_newReport = collect(appName, dir);

    // This session's log, the last one's kept beside it.
    const QString log = logPath(appName, dir);
    QFile::remove(log + QStringLiteral(".1"));
    QFile::rename(log, log + QStringLiteral(".1"));
    {
        const std::lock_guard lock(g_logMutex);
        g_log = std::make_unique<QFile>(log);
        if (!g_log->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            g_log.reset();
    }
    g_previousHandler = qInstallMessageHandler(onMessage);

    // A session running: gone again only when it ends as it should.
    s_marker = markerPath(appName, dir);
    QFile marker(s_marker);
    if (marker.open(QIODevice::WriteOnly | QIODevice::Text))
        marker.write((QStringLiteral("started ") + QDateTime::currentDateTime().toString(Qt::ISODateWithMs) + QStringLiteral("\npid ")
                      + QString::number(QCoreApplication::applicationPid()) + QStringLiteral("\nversion ")
                      + QCoreApplication::applicationVersion() + u'\n')
                         .toUtf8());

    // The crash handler, everything it needs made now.
    const QByteArray pending = QFile::encodeName(pendingPath(dir));
    std::strncpy(g_crashPath, pending.constData(), sizeof(g_crashPath) - 1);
    g_mainThread = syscall(SYS_gettid);
    void *warm[2];
    backtrace(warm, 2); // loads what backtrace() needs, so the handler doesn't allocate
    stack_t alt{};
    alt.ss_sp = g_altStack.data();
    alt.ss_size = g_altStack.size();
    sigaltstack(&alt, nullptr);
    struct sigaction crash{};
    crash.sa_sigaction = onCrash;
    crash.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&crash.sa_mask);
    for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS})
        sigaction(sig, &crash, nullptr);

    // SIGTERM / SIGINT: quit through the event loop (saving as on any quit).
    if (QCoreApplication *app = QCoreApplication::instance(); app && pipe2(g_quitPipe, O_CLOEXEC | O_NONBLOCK) == 0) {
        auto *notifier = new QSocketNotifier(g_quitPipe[0], QSocketNotifier::Read, app);
        QObject::connect(notifier, &QSocketNotifier::activated, app, [] {
            char byte;
            while (read(g_quitPipe[0], &byte, 1) > 0) {
            }
            qInfo("asked to quit (a shutdown, the service stopping, or Ctrl+C)");
            // Ending as asked: if the screen goes first (cage stopping with us)
            // and Qt exits before main() returns, that's no crash either.
            if (!s_marker.isEmpty())
                QFile::remove(s_marker);
            QCoreApplication::quit();
        });
        struct sigaction quit{};
        quit.sa_handler = onQuit;
        sigemptyset(&quit.sa_mask);
        quit.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &quit, nullptr);
        sigaction(SIGINT, &quit, nullptr);
    }
    if (!s_newReport.isEmpty())
        qWarning("the last session didn't end cleanly: %s", qPrintable(s_newReport));
}

void endedCleanly()
{
    qInfo("ended cleanly");
    if (!s_marker.isEmpty())
        QFile::remove(s_marker);
}

QString newReport() { return s_newReport; }

QList<Report> reports(const QString &dir)
{
    QList<Report> list;
    QDir crashes(crashesDir(dir));
    for (const QString &name : crashes.entryList({QStringLiteral("2*.txt")}, QDir::Files, QDir::Name | QDir::Reversed)) {
        Report r;
        r.path = crashes.filePath(name);
        for (const QString &line : lastLines(r.path, 100000).mid(0, 8)) {
            if (line.startsWith(u"What happened: "))
                r.summary = line.mid(15);
            else if (line.startsWith(u"When: "))
                r.when = QDateTime::fromString(line.mid(6), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
        list << r;
    }
    return list;
}

} // namespace krate::crash
