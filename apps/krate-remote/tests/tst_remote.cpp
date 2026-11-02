// Krate Remote (docs/REMOTE.md) against a stand-in ssh that runs each
// command here, with HOME a scratch "player": names quoted through the shell
// whatever they hold, the player's folders worked out, settings changed as
// Krate's screen changes them (and never written over when unreadable),
// files listed, sent both ways, renamed and deleted, and the askpass helper.

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "RemoteController.h"
#include "RemoteFiles.h"
#include "RemoteSettings.h"
#include "Shell.h"

namespace loc = krate::locations;

namespace {

void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
}

// Names that break careless quoting.
const QString kAwkward = QStringLiteral("it's a \"take\" $HOME `id` 50% %2.wav");

} // namespace

class TestRemote : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_player; // the "player's" home

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_player = m_dir.filePath("player");
        QDir().mkpath(m_player);
        const QString ssh = m_dir.filePath("fake-ssh");
        write(ssh, "#!/bin/sh\n"
                   "# ssh's options and destination ignored; the command run here, in the scratch player.\n"
                   "for a; do last=$a; done\n"
                   "HOME=\"$KRATE_REMOTE_TEST_HOME\"; export HOME\n"
                   "unset XDG_CONFIG_HOME XDG_DATA_HOME\n"
                   "exec sh -c \"$last\"\n");
        QFile::setPermissions(ssh, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        qputenv("KRATE_REMOTE_SSH", ssh.toLocal8Bit());
        qputenv("KRATE_REMOTE_TEST_HOME", m_player.toLocal8Bit());
    }

    void quoting()
    {
        Shell shell;
        shell.setTarget(QStringLiteral("player"), QStringLiteral("me"), 22);
        QString out;
        shell.run(QStringLiteral("printf %s ") + Shell::quote(kAwkward), [&out](const Shell::Result &r) { out = QString::fromUtf8(r.out); });
        QTRY_COMPARE(out, kAwkward);
        QByteArray echoed;
        shell.run(QStringLiteral("cat"), [&echoed](const Shell::Result &r) { echoed = r.out; }, "stdin\nthrough\n");
        QTRY_COMPARE(echoed, QByteArray("stdin\nthrough\n"));
    }

    // The folders as the player has them, and changes as Krate's screen makes them.
    void settingsFile()
    {
        const loc::Roots roots{QStringLiteral("/p/.local/share"), QStringLiteral("/p/Music")};
        QJsonObject s;
        QVariantList rows = remote::folderRows(s, roots);
        QCOMPARE(rows.size(), 5);
        QCOMPARE(rows[0].toMap().value("key").toString(), QStringLiteral("recordings"));
        QCOMPARE(rows[0].toMap().value("path").toString(), QStringLiteral("/p/.local/share/krate/recordings"));
        QCOMPARE(rows[3].toMap().value("path").toString(), QStringLiteral("/p/Music/Krate"));

        s = remote::setFolder(s, QStringLiteral("exports"), QStringLiteral("/usb/out/"), true, roots); // at once
        QCOMPARE(s.value("folders").toObject().value("exports").toString(), QStringLiteral("/usb/out"));
        QVERIFY(!s.contains("pending"));
        s = remote::setFolder(s, QStringLiteral("library"), QStringLiteral("/big/songs"), true, roots);
        QCOMPARE(s.value("pending").toObject().value("library").toObject().value("move").toBool(), true);
        rows = remote::folderRows(s, roots);
        QCOMPARE(rows[1].toMap().value("pending").toString(), QStringLiteral("/big/songs"));
        QCOMPARE(rows[1].toMap().value("path").toString(), QStringLiteral("/p/.local/share/krate/library")); // until Krate starts
        // Back where it is: nothing waits.
        s = remote::setFolder(s, QStringLiteral("library"), QString(), false, roots);
        QVERIFY(!s.contains("pending"));
        s = remote::setPluginFolders(s, {QStringLiteral(" /a/clap/ "), QStringLiteral("/a/clap"), QString()});
        QCOMPARE(s.value("pluginFolders").toArray(), QJsonArray{QStringLiteral("/a/clap")});
    }

    void controller()
    {
        Shell shell;
        RemoteController remote(shell);
        QSignalSpy connected(&remote, &RemoteController::connected);
        remote.connectTo(QStringLiteral("player"), QStringLiteral("me"), 22);
        QTRY_COMPARE(connected.count(), 1);
        QCOMPARE(remote.state(), QStringLiteral("connected"));
        QCOMPARE(remote.home(), m_player);
        QCOMPARE(remote.settingsPath(), m_player + QStringLiteral("/.config/krate/settings.json"));
        QCOMPARE(remote.folder(loc::Folder::Recordings), m_player + QStringLiteral("/.local/share/krate/recordings"));
        QCOMPARE(remote.folder(loc::Folder::Exports), m_player + QStringLiteral("/Music/Krate"));
        QCOMPARE(remote.krateState(), QStringLiteral("stopped"));

        // Each change read, made and written whole; Krate's other keys kept.
        write(remote.settingsPath(), R"({"version": 1, "future": 7, "midiInputs": "Old"})");
        QSignalSpy changed(&remote, &RemoteController::settingsChanged);
        remote.setMidiInputs(QStringLiteral(" Pedal "));
        remote.setProgramChangeFocus(false);
        remote.addPluginFolder(QStringLiteral("/opt/clap"));
        QTRY_VERIFY(!remote.programChangeFocus());
        QTRY_COMPARE(remote.pluginFolders(), QStringList{QStringLiteral("/opt/clap")});
        QJsonObject file = QJsonDocument::fromJson(read(remote.settingsPath())).object();
        QCOMPARE(file.value("midiInputs").toString(), QStringLiteral("Pedal"));
        QCOMPARE(file.value("programChangeFocus").toBool(true), false);
        QCOMPARE(file.value("future").toInt(), 7);
        QVERIFY(!QFile::exists(remote.settingsPath() + QStringLiteral(".remote")));

        // A move into a folder with files: refused before Krate would.
        write(m_dir.filePath("full/x"), "x");
        remote.setFolder(QStringLiteral("recordings"), m_dir.filePath("full"), true);
        QTRY_VERIFY(remote.message().contains(QStringLiteral("has files")));
        QVERIFY(!remote.restartNeeded());
        remote.setFolder(QStringLiteral("recordings"), m_dir.filePath("empty"), true);
        QTRY_VERIFY(remote.restartNeeded());
        file = QJsonDocument::fromJson(read(remote.settingsPath())).object();
        QCOMPARE(file.value("pending").toObject().value("recordings").toObject().value("path").toString(), m_dir.filePath("empty"));
        remote.cancel(QStringLiteral("recordings"));
        QTRY_VERIFY(!remote.restartNeeded());

        // Unreadable: left alone.
        write(remote.settingsPath(), "{\"midiInputs\": ");
        remote.setMidiInputs(QStringLiteral("New"));
        QTRY_VERIFY(remote.message().contains(QStringLiteral("isn't readable")));
        QCOMPARE(read(remote.settingsPath()), QByteArray("{\"midiInputs\": "));
        QFile::remove(remote.settingsPath());

        // Crash reports, newest first; a text file read.
        const QString crashes = remote.folder(loc::Folder::Data) + QStringLiteral("/crashes/");
        write(crashes + QStringLiteral("old.txt"), "old");
        write(crashes + QStringLiteral("new.txt"), "Crashed: here");
        QFile f(crashes + QStringLiteral("old.txt"));
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(QDateTime::currentDateTime().addDays(-1), QFileDevice::FileModificationTime));
        f.close();
        remote.refresh();
        QTRY_COMPARE(remote.crashReports().size(), 2);
        QCOMPARE(remote.crashReports()[0].toMap().value("name").toString(), QStringLiteral("new.txt"));
        QSignalSpy text(&remote, &RemoteController::textRead);
        remote.readText(crashes + QStringLiteral("new.txt"));
        QTRY_COMPARE(text.count(), 1);
        QCOMPARE(text[0][1].toString(), QStringLiteral("Crashed: here"));
    }

    void files()
    {
        Shell shell;
        RemoteFiles files(shell);
        const QString local = m_dir.filePath("here");
        files.setDownloadFolder(m_dir.filePath("downloads"));
        QSignalSpy listed(&files, &RemoteFiles::listed);
        QSignalSpy done(&files, &RemoteFiles::transfersDone);

        files.open(m_player);
        QTRY_COMPARE(listed.count(), 1);
        files.makeFolder(QStringLiteral("songs"));
        QTRY_VERIFY(files.exists(QStringLiteral("songs")));
        files.openEntry(QStringLiteral("songs"));
        QTRY_COMPARE(files.path(), m_player + QStringLiteral("/songs"));

        // Up: a file with an awkward name, a big one, and a folder with what it holds.
        QByteArray big(3 * 1024 * 1024 + 17, '\0');
        for (int i = 0; i < big.size(); ++i)
            big[i] = char(i * 7);
        write(local + u'/' + kAwkward, "take one");
        write(local + QStringLiteral("/big.bin"), big);
        write(local + QStringLiteral("/kit/drums/kick.wav"), "kick");
        write(local + QStringLiteral("/kit/snare.wav"), "snare");
        files.upload({QUrl::fromLocalFile(local + u'/' + kAwkward), QUrl::fromLocalFile(local + QStringLiteral("/big.bin")),
                      QUrl::fromLocalFile(local + QStringLiteral("/kit"))});
        QTRY_VERIFY_WITH_TIMEOUT(done.count() >= 1, 20000);
        for (const QVariant &t : files.transfers())
            QCOMPARE(t.toMap().value("state").toString(), QStringLiteral("done"));
        const QString there = m_player + QStringLiteral("/songs/");
        QCOMPARE(read(there + kAwkward), QByteArray("take one"));
        QCOMPARE(read(there + QStringLiteral("big.bin")), big);
        QCOMPARE(read(there + QStringLiteral("kit/drums/kick.wav")), QByteArray("kick"));
        QTRY_VERIFY(files.exists(QStringLiteral("kit"))); // listed again
        QVERIFY(files.entries()[0].toMap().value("dir").toBool()); // folders first
        QVERIFY(QDir(there).entryList({QStringLiteral("*part*")}, QDir::Files | QDir::Hidden).isEmpty());

        // Down: a file, and a folder (a name taken here gets " (2)").
        done.clear();
        write(m_dir.filePath("downloads/kit"), "in the way");
        files.download({QStringLiteral("big.bin"), QStringLiteral("kit")});
        QTRY_VERIFY_WITH_TIMEOUT(done.count() >= 1 && !files.transferring(), 20000);
        QTRY_COMPARE(read(m_dir.filePath("downloads/big.bin")), big);
        QCOMPARE(read(m_dir.filePath("downloads/kit (2)/drums/kick.wav")), QByteArray("kick"));
        QCOMPARE(read(m_dir.filePath("downloads/kit (2)/snare.wav")), QByteArray("snare"));

        // Renamed (never over another), deleted.
        files.rename(kAwkward, QStringLiteral("take.wav"));
        QTRY_VERIFY(files.exists(QStringLiteral("take.wav")));
        files.rename(QStringLiteral("take.wav"), QStringLiteral("kit"));
        QTRY_VERIFY(files.error().contains(QStringLiteral("already")));
        QVERIFY(QFileInfo(there + QStringLiteral("take.wav")).isFile());
        files.remove({QStringLiteral("kit"), QStringLiteral("take.wav"), QStringLiteral("..")});
        QTRY_VERIFY(!files.exists(QStringLiteral("kit")));
        QVERIFY(!QFile::exists(there + QStringLiteral("take.wav")));
        QVERIFY(QFile::exists(there + QStringLiteral("big.bin")));
    }

    // A file sent short (the ssh cut off): never under its name.
    void cutOff()
    {
        Shell shell;
        const QString f = m_player + QStringLiteral("/short.wav");
        // The upload's own command, as RemoteFiles builds it, told to expect more than comes.
        QProcess *p = shell.start(QStringLiteral("f=%1; n=100; p=\"$f.krate-remote-part\"; mkdir -p -- \"$(dirname -- \"$f\")\" && cat > \"$p\" && "
                                                 "[ \"$(wc -c < \"$p\")\" -eq \"$n\" ] && mv -f -- \"$p\" \"$f\" || "
                                                 "{ rm -f -- \"$p\"; echo \"$f: not sent whole\" >&2; exit 1; }")
                                      .arg(Shell::quote(f)));
        p->write("only some");
        p->closeWriteChannel();
        QVERIFY(p->waitForFinished(5000));
        QCOMPARE(p->exitCode(), 1);
        QVERIFY(!QFile::exists(f));
        QVERIFY(!QFile::exists(f + QStringLiteral(".krate-remote-part")));
        delete p;
    }

    void askpass()
    {
        QVERIFY(Shell::isAskpass() == false);
        qputenv("KRATE_REMOTE_ASKPASS", "1");
        qputenv("KRATE_REMOTE_PASSWORD", "s3cret");
        QVERIFY(Shell::isAskpass());
        char prog[] = "ssh-askpass";
        char pw[] = "me@player's password: ";
        char host[] = "Are you sure you want to continue connecting (yes/no)?";
        char *withPassword[] = {prog, pw};
        char *withHost[] = {prog, host};
        QCOMPARE(Shell::askpassMain(2, withPassword), 0);
        QCOMPARE(Shell::askpassMain(2, withHost), 1);
        qunsetenv("KRATE_REMOTE_ASKPASS");
        qunsetenv("KRATE_REMOTE_PASSWORD");
    }
};

QTEST_GUILESS_MAIN(TestRemote)
#include "tst_remote.moc"
