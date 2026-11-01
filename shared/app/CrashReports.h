#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace krate {

// Crash reports (docs/CRASHES.md). install(), first thing after the
// QGuiApplication exists:
// - every log message also goes to <dir>/<app>.log (this session's; the one
//   before is kept as <app>.log.1);
// - a crash (SIGSEGV, SIGABRT from qFatal or an assert, SIGFPE, SIGILL,
//   SIGBUS) writes its signal, thread and call stack to a file at once, then
//   ends the app as it would have (so systemd still sees the crash);
// - SIGTERM and SIGINT (a shutdown, the service stopping, Ctrl+C) quit the
//   app properly instead of killing it, so it saves and ends cleanly;
// - at the next start, whatever the last session left (a crash, or a
//   session that never ended: killed, frozen, out of memory, power cut) is
//   turned into a report, <dir>/crashes/<when>.txt, with the end of its log.
// endedCleanly() at the end of main(): the session ended as it should.
namespace crash {

struct Report
{
    QString path;
    QDateTime when;  // when it happened (or when the session began, if unknown)
    QString summary; // "Crashed: segmentation fault in the audio thread"
};

// `dir`: where the log and the reports go (~/.local/share/<app>).
void install(const QString &appName, const QString &dir);
void endedCleanly();

// The reports, newest first; the one made at this start, if any.
QList<Report> reports(const QString &dir);
QString newReport(); // "" when the last session ended cleanly
// The most kept: older ones go.
inline constexpr int kKeep = 30;

// For tests: build the report the last session left, as install() does at
// start, without touching the signal handlers. Returns its path ("" if none).
QString collect(const QString &appName, const QString &dir);

// A backtrace_symbols line made readable: "krate(_ZN5krate4Rack7processERKNS_9RackBlockE+0x1c) [0x55...]"
// -> "krate::Rack::process(krate::RackBlock const&) +0x1c  (krate)".
QString readableFrame(const QString &line);

} // namespace crash
} // namespace krate
