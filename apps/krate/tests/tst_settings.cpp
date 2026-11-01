// Settings (docs/SETTINGS.md): where folders are (and their defaults), moving
// one at a start (renamed, or refused onto files), the instruments' folders
// moved all or nothing, and the settings file kept.

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "SettingsController.h"
#include "app/Locations.h"

using krate::locations::Folder;
namespace loc = krate::locations;

namespace {

void write(const QString &path, const QByteArray &bytes = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

QJsonObject withFolders(const QJsonObject &folders, const QJsonObject &pending = {})
{
    QJsonObject o{{QStringLiteral("folders"), folders}};
    if (!pending.isEmpty())
        o[QStringLiteral("pending")] = pending;
    return o;
}

QJsonObject move(const QString &path, bool files = true)
{
    return QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("move"), files}};
}

} // namespace

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // Defaults; recordings and the library follow the data folder unless set.
    void folders()
    {
        const QJsonObject none;
        QVERIFY(loc::folder(Folder::Data, none).endsWith(u"/krate"));
        QCOMPARE(loc::folder(Folder::Recordings, none), loc::folder(Folder::Data, none) + QStringLiteral("/recordings"));
        const QJsonObject moved = withFolders({{QStringLiteral("data"), QStringLiteral("/mnt/big/krate")}});
        QCOMPARE(loc::folder(Folder::Recordings, moved), QStringLiteral("/mnt/big/krate/recordings"));
        QCOMPARE(loc::folder(Folder::Library, moved), QStringLiteral("/mnt/big/krate/library"));
        const QJsonObject own = withFolders({{QStringLiteral("recordings"), QStringLiteral("/mnt/takes/")}});
        QCOMPARE(loc::folder(Folder::Recordings, own), QStringLiteral("/mnt/takes"));
    }

    // A move: renamed into a new (or empty) folder; refused onto files, and
    // then the folder stays as it was.
    void moving()
    {
        QTemporaryDir dir;
        write(dir.filePath("old/take-1.wav"), "RIFF1");
        write(dir.filePath("old/sub/take-2.wav"), "RIFF2");
        QCOMPARE(loc::moveFolder(dir.filePath("old"), dir.filePath("new/takes")), QString());
        QVERIFY(!QFileInfo::exists(dir.filePath("old")));
        QVERIFY(QFileInfo::exists(dir.filePath("new/takes/sub/take-2.wav")));

        write(dir.filePath("busy/other.wav"));
        QVERIFY(!loc::moveFolder(dir.filePath("new/takes"), dir.filePath("busy")).isEmpty());
        QVERIFY(QFileInfo::exists(dir.filePath("new/takes/take-1.wav"))); // untouched
        QVERIFY(!loc::moveFolder(dir.filePath("new"), dir.filePath("new/inside")).isEmpty());
        QCOMPARE(loc::moveFolder(dir.filePath("nothing-here"), dir.filePath("x")), QString()); // nothing to move
        QVERIFY(QDir().mkpath(dir.filePath("empty")));
        QCOMPARE(loc::moveFolder(dir.filePath("new/takes"), dir.filePath("empty")), QString()); // an empty one: fine
        QVERIFY(QFileInfo::exists(dir.filePath("empty/take-1.wav")));
    }

    // At the start: what waits is done; the recordings moved out of the data
    // folder before the data folder itself moves.
    void applyAtTheStart()
    {
        QTemporaryDir dir;
        const QString data = dir.filePath("data"), newData = dir.filePath("bigdrive/krate"), takes = dir.filePath("takes");
        write(data + "/sequencer.json", "{}");
        write(data + "/recordings/audio1/take.wav", "RIFF");
        QJsonObject settings = withFolders({{QStringLiteral("data"), data}},
                                           {{QStringLiteral("data"), move(newData)}, {QStringLiteral("recordings"), move(takes)}});
        QStringList notes;
        settings = SettingsController::applyPending(settings, {}, &notes);
        QVERIFY(!settings.contains(u"pending"));
        QCOMPARE(loc::folder(Folder::Data, settings), newData);
        QCOMPARE(loc::folder(Folder::Recordings, settings), takes);
        QVERIFY(QFileInfo::exists(takes + "/audio1/take.wav"));
        QVERIFY(QFileInfo::exists(newData + "/sequencer.json"));
        QVERIFY(!QFileInfo::exists(newData + "/recordings"));
        QCOMPARE(notes.size(), 2);
    }

    // The instruments' folder: only the instruments' own folders go, all or none.
    void instrumentsAllOrNothing()
    {
        QTemporaryDir dir;
        const QString from = dir.filePath("share"), to = dir.filePath("music");
        write(from + "/korrode/patterns.json");
        write(from + "/kessel/kits.json");
        write(from + "/someoneelse/keep.txt");
        write(to + "/kessel/kits.json", "theirs"); // in the way
        QJsonObject settings = withFolders({{QStringLiteral("instruments"), from}}, {{QStringLiteral("instruments"), move(to)}});
        QStringList notes;
        settings = SettingsController::applyPending(settings, {QStringLiteral("korrode"), QStringLiteral("kessel")}, &notes);
        QCOMPARE(loc::folder(Folder::Instruments, settings), from); // not switched
        QVERIFY(QFileInfo::exists(from + "/korrode/patterns.json"));
        QVERIFY(notes.value(0).contains(u"not moved"));

        QDir(to).removeRecursively();
        settings = withFolders({{QStringLiteral("instruments"), from}}, {{QStringLiteral("instruments"), move(to)}});
        notes.clear();
        settings = SettingsController::applyPending(settings, {QStringLiteral("korrode"), QStringLiteral("kessel")}, &notes);
        QCOMPARE(loc::folder(Folder::Instruments, settings), to);
        QVERIFY(QFileInfo::exists(to + "/korrode/patterns.json"));
        QVERIFY(QFileInfo::exists(to + "/kessel/kits.json"));
        QVERIFY(QFileInfo::exists(from + "/someoneelse/keep.txt")); // not ours: stays
        QVERIFY(!QFileInfo::exists(to + "/someoneelse"));
    }

    // Through the controller: a folder waits for the start, exports apply at
    // once, the rest is kept in the file and read back.
    void controller()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        {
            SettingsController s(path, {});
            QVERIFY(!s.restartNeeded());
            QString exports;
            s.onExportsChanged = [&exports](const QString &f) { exports = f; };
            s.setFolder(QStringLiteral("exports"), dir.filePath("usb"), true);
            QCOMPARE(exports, dir.filePath("usb"));
            QVERIFY(!s.restartNeeded());
            s.setFolder(QStringLiteral("recordings"), dir.filePath("takes"), true);
            QVERIFY(s.restartNeeded());
            QCOMPARE(s.folder(Folder::Recordings), loc::folder(Folder::Recordings, QJsonObject{})); // not yet
            s.setMidiInputs(QStringLiteral("Keys"));
            s.setPluginFolders({dir.filePath("clap"), dir.filePath("clap/")});
            QCOMPARE(s.pluginFolders(), QStringList{dir.filePath("clap")});
            s.setProgramChangeFocus(false);
            s.setAutoCheckUpdates(false);
        }
        SettingsController again(path, {}); // the next start
        QVERIFY(!again.restartNeeded());
        QCOMPARE(again.folder(Folder::Recordings), dir.filePath("takes"));
        QCOMPARE(again.folder(Folder::Exports), dir.filePath("usb"));
        QCOMPARE(again.midiInputs(), QStringLiteral("Keys"));
        QCOMPARE(again.pluginFolders(), QStringList{dir.filePath("clap")});
        QVERIFY(!again.programChangeFocus());
        QVERIFY(!again.autoCheckUpdates());
        QCOMPARE(again.startNotes().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
