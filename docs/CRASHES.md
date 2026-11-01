# Crash reports

When Krate crashes, the kiosk on the player starts it again two seconds later, so a crash looks like a restart. Krate now writes down what happened, so it can be found and fixed.

## Reading them

- **On the screen**: after a crash, Krate starts with a red banner: *Krate started again after the last session went wrong*, with what happened. **VIEW** opens the report, **DISMISS** hides the banner (the report stays). Any time later: **AUDIO** (top right), then **CRASH REPORTS**: the list, newest first, and the one you pick. **DELETE** / **DELETE ALL** when done.
- **As files**: `~/.local/share/krate/crashes/<date>_<time>.txt` (the last 30). From another computer: `scp 'jaimie@krate.local:.local/share/krate/crashes/*.txt' .`
- **The log**: `~/.local/share/krate/krate.log` is this session's, `krate.log.1` the one before. On the player, systemd's view too: `journalctl -u krate-kiosk@jaimie -b` (a crash shows as `code=dumped, status=11/SEGV` or `6/ABRT`).

## What a report says

- **What happened**: a crash and its kind (a segmentation fault, an abort from a fatal error or a failed check, an arithmetic error...), and in which thread: the main (screen) thread, or another one (usually the audio thread). Or *Ended without finishing*: Krate didn't get to say goodbye and caught no crash, so it was killed, froze and was stopped, ran out of memory, or the power went.
- **When**, when that session started and how long it ran, Krate's version.
- **The call stack**: where in the code it was, innermost first, function names readable (Krate is built with its functions' names exported for this).
- **The session's last 200 log lines**: what Krate was doing just before.

Send the file (or a photo of the screen) along with what you were doing when it happened.

## How it works

`shared/app/CrashReports` (`krate::crash`):

- `install()`, first thing in `main()`: every log message also goes to the session's log file (flushed each line, so a crash finds it written); a file marks the session as running; a handler for SIGSEGV, SIGABRT, SIGFPE, SIGILL and SIGBUS writes the signal, the time, the thread and the call stack (`backtrace_symbols_fd`) to `crashes/pending.txt` using only calls that are safe in a signal handler, on a stack of its own (so a stack overflow is caught too), then lets the signal end the app as it would have (systemd still sees the crash). SIGTERM and SIGINT (a shutdown, the service stopping, Ctrl+C) quit through the event loop instead, so Krate saves and ends cleanly.
- `endedCleanly()`, last thing in `main()`: removes the running mark.
- At the next start, `collect()` turns what's left (the crash file, the running mark, the old log) into a report, readable: symbols demangled. The `Crashes` QML singleton (`apps/krate/CrashReportsController`) and `CrashReportsPopup.qml` show them.

Tested by `tst_crash`: a helper app that crashes on a bad pointer, aborts, vanishes, is asked to quit, or ends well, and the report (or none) each leaves.
