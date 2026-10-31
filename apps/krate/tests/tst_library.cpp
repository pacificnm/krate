#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "SongLibrary.h"

namespace {

QString write(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(content);
    return path;
}

SongLibrary::Details details(const char *artist, const char *album, const char *title, int track = 0)
{
    return {QString::fromUtf8(artist), QString::fromUtf8(album), QString::fromUtf8(title), track};
}

} // namespace

class TestLibrary : public QObject
{
    Q_OBJECT

private slots:
    void addsCopiesAndFindsAgain()
    {
        QTemporaryDir downloads, home;
        const QString tab = write(downloads, "a.gp", "tab bytes");
        const QString midi = write(downloads, "a.mid", "midi bytes");
        SongLibrary lib(home.filePath("library"));
        QVERIFY(lib.isOpen());
        const auto song = lib.add(tab, midi, details("The Band", "First Album", "Opening Song", 1));
        QVERIFY(song.has_value());
        QVERIFY(song->id > 0);
        QCOMPARE(song->tabFile, home.filePath("library/The Band/First Album/Opening Song.gp"));
        QCOMPARE(song->midiFile, home.filePath("library/The Band/First Album/Opening Song.mid"));
        QVERIFY(QFile::exists(song->tabFile));
        // The originals can go: the library has its own copies.
        QFile::remove(tab);
        QFile::remove(midi);
        QVERIFY(QFile::exists(song->midiFile));
        // The same files again (from anywhere): the same song, nothing copied twice.
        const QString again = write(downloads, "renamed.gp", "tab bytes");
        const QString againMidi = write(downloads, "renamed.mid", "midi bytes");
        const auto same = lib.add(again, againMidi, details("x", "y", "z"));
        QCOMPARE(same->id, song->id);
        QCOMPARE(lib.songs().size(), 1);
        // Different content with the same title: a second file beside it.
        const auto other = lib.add(write(downloads, "b.gp", "other"), {}, details("The Band", "First Album", "Opening Song"));
        QVERIFY(other.has_value());
        QVERIFY(other->id != song->id);
        QCOMPARE(other->tabFile, home.filePath("library/The Band/First Album/Opening Song (2).gp"));
        QVERIFY(other->midiFile.isEmpty());
        QVERIFY(!lib.add({}, {}, details("a", "b", "c")).has_value());
    }

    void safeNames()
    {
        QTemporaryDir downloads, home;
        SongLibrary lib(home.filePath("library"));
        const auto song = lib.add({}, write(downloads, "s.mid", "1"), details("../AC/DC?", "", "  "));
        QVERIFY(song.has_value());
        QVERIFY(song->midiFile.startsWith(home.filePath("library/")));
        QVERIFY(QFile::exists(song->midiFile));
        QVERIFY(song->midiFile.contains("Unknown album"));
        QVERIFY(song->midiFile.endsWith("/Song.mid"));
        QVERIFY(!song->midiFile.contains(".."));
    }

    void namesFromFiles()
    {
        auto d = SongLibrary::detailsFromFileName("/x/Some_Band-Some_Song_Title-10-24-2026.mid");
        QCOMPARE(d.artist, QStringLiteral("Some Band"));
        QCOMPARE(d.title, QStringLiteral("Some Song Title"));
        d = SongLibrary::detailsFromFileName("/x/justatitle.gp");
        QCOMPARE(d.artist, QString());
        QCOMPARE(d.title, QStringLiteral("justatitle"));
        d = SongLibrary::detailsFromFileName("/x/Band-Song-With-Dashes.mid");
        QCOMPARE(d.artist, QStringLiteral("Band"));
        QCOMPARE(d.title, QStringLiteral("Song-With-Dashes"));
    }

    void listSearchEditRemove()
    {
        QTemporaryDir downloads, home;
        SongLibrary lib(home.filePath("library"));
        lib.add({}, write(downloads, "1.mid", "1"), details("Zed", "B side", "Last", 2));
        lib.add({}, write(downloads, "2.mid", "2"), details("alpha", "Album", "Two", 2));
        lib.add({}, write(downloads, "3.mid", "3"), details("Alpha", "Album", "One", 1));
        const auto all = lib.songs();
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].details.title, QStringLiteral("One")); // artist (any case), album, track number
        QCOMPARE(all[1].details.title, QStringLiteral("Two"));
        QCOMPARE(all[2].details.artist, QStringLiteral("Zed"));
        QCOMPARE(lib.songs("side").size(), 1);   // album
        QCOMPARE(lib.songs("ALPHA").size(), 2);  // artist, any case
        QCOMPARE(lib.songs("o").size(), 2);      // "One", "Two" (and nothing else has an o)
        QCOMPARE(lib.songs("%").size(), 0);      // no wildcards
        // Edit: the details change, the files stay.
        const qint64 id = all[2].id;
        QVERIFY(lib.setDetails(id, details(" Zed ", "Singles", "Renamed", 7)));
        const auto edited = lib.song(id);
        QCOMPARE(edited->details.artist, QStringLiteral("Zed"));
        QCOMPARE(edited->details.trackNumber, 7);
        QVERIFY(QFile::exists(edited->midiFile));
        // Remove: its files and empty folders go.
        lib.setLastSong(id);
        QVERIFY(lib.remove(id));
        QVERIFY(!QFile::exists(edited->midiFile));
        QVERIFY(!QDir(home.filePath("library/Zed")).exists());
        QCOMPARE(lib.lastSong(), qint64(0));
        QCOMPARE(lib.songs().size(), 2);
        QVERIFY(!lib.remove(id));
    }

    void setupSoundsAndLast()
    {
        QTemporaryDir downloads, home;
        qint64 id = 0;
        {
            SongLibrary lib(home.filePath("library"));
            id = lib.add({}, write(downloads, "1.mid", "1"), details("A", "B", "C"))->id;
            QCOMPARE(lib.myTrack(id), -1);
            QVERIFY(lib.setup(id).isEmpty());
            lib.saveSetup(id, 2, {{"korrode", 0.5, true, false, true}, {}, {"krimson:B", 1.0, false, true, false}});
            QVERIFY(!lib.song(id)->soundsSaved);
            lib.saveSounds(id, QJsonObject{{"korrode", QJsonObject{{"cutoff", 0.3}}}, {"krimson", QJsonObject{{"slots", QJsonArray{1, 2}}}}},
                           QJsonObject{{"master", 0.7}});
            lib.setLastSong(id);
        }
        SongLibrary lib(home.filePath("library")); // opened again, as on the next start
        QCOMPARE(lib.lastSong(), id);
        QCOMPARE(lib.myTrack(id), 2);
        const auto setup = lib.setup(id);
        QCOMPARE(setup.size(), 3);
        QCOMPARE(setup[0].instrument, QStringLiteral("korrode"));
        QCOMPARE(setup[0].level, 0.5);
        QVERIFY(setup[0].mute && setup[0].fromMidi);
        QCOMPARE(setup[1].instrument, QString());
        QVERIFY(setup[2].solo);
        QVERIFY(lib.song(id)->soundsSaved);
        QCOMPARE(lib.sounds(id).value("korrode").toObject().value("cutoff").toDouble(), 0.3);
        QCOMPARE(lib.sounds(id).value("krimson").toObject().value("slots").toArray().size(), 2);
        QCOMPARE(lib.mixer(id).value("master").toDouble(), 0.7);
        // Saving again replaces.
        lib.saveSounds(id, QJsonObject{{"kessel", QJsonObject{}}}, {});
        QCOMPARE(lib.sounds(id).keys(), QStringList{"kessel"});
        // Removing a song removes its setup and sounds.
        lib.remove(id);
        QVERIFY(lib.setup(id).isEmpty());
        QVERIFY(lib.sounds(id).isEmpty());
    }

    void twoConnections()
    {
        QTemporaryDir downloads, home;
        SongLibrary a(home.filePath("library"));
        SongLibrary b(home.filePath("library"));
        a.add({}, write(downloads, "1.mid", "1"), details("A", "B", "C"));
        QCOMPARE(b.songs().size(), 1);
    }

    // A song's backing track: copied in beside the song, replaced, its offset kept, gone with the song.
    void backingTracks()
    {
        QTemporaryDir downloads, home;
        SongLibrary lib(home.filePath("library"));
        const auto song = lib.add(write(downloads, "a.gp", "tab"), {}, details("The Band", "Album", "Tune"));
        QVERIFY(song.has_value());
        QVERIFY(lib.song(song->id)->backingFile.isEmpty());

        const QString first = lib.setBacking(song->id, write(downloads, "Take One.MP3", "mp3 one"));
        QCOMPARE(first, home.filePath("library/The Band/Album/Tune backing.mp3"));
        QCOMPARE(lib.song(song->id)->backingFile, first);
        lib.setBackingOffset(song->id, 3.25);
        QCOMPARE(lib.song(song->id)->backingOffset, 3.25);

        // Another file replaces it (the old copy goes).
        const QString second = lib.setBacking(song->id, write(downloads, "take2.mp3", "mp3 two"));
        QVERIFY(!second.isEmpty());
        QCOMPARE(lib.song(song->id)->backingFile, second);
        QVERIFY(QFile::exists(second));
        QVERIFY(first == second || !QFile::exists(first));
        QFile f(second);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("mp3 two"));
        f.close();

        lib.clearBacking(song->id);
        QVERIFY(lib.song(song->id)->backingFile.isEmpty());
        QCOMPARE(lib.song(song->id)->backingOffset, 0.0);
        QVERIFY(!QFile::exists(second));

        // Removing the song takes its backing track with it.
        const QString third = lib.setBacking(song->id, write(downloads, "t3.mp3", "mp3 three"));
        QVERIFY(lib.remove(song->id));
        QVERIFY(!QFile::exists(third));
    }

    // A library from before backing tracks (schema 1) opens and gains them, its songs kept.
    void upgradesAnOlderLibrary()
    {
        QTemporaryDir downloads, home;
        qint64 id = 0;
        {
            SongLibrary lib(home.filePath("library"));
            id = lib.add(write(downloads, "a.gp", "tab"), {}, details("A", "B", "C"))->id;
        }
        {   // Turn it back into a version 1 library: no backing columns.
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade"));
            db.setDatabaseName(home.filePath("library/library.db"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("ALTER TABLE songs DROP COLUMN backing_file")));
            QVERIFY(q.exec(QStringLiteral("ALTER TABLE songs DROP COLUMN backing_offset")));
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 1")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("downgrade"));
        SongLibrary lib(home.filePath("library"));
        QVERIFY(lib.isOpen());
        QCOMPARE(lib.song(id)->details.title, QStringLiteral("C"));
        QVERIFY(!lib.setBacking(id, write(downloads, "b.mp3", "mp3")).isEmpty());
        lib.setBackingOffset(id, -1.5);
        QCOMPARE(lib.song(id)->backingOffset, -1.5);
    }

    // Saved sequences: save, list by name, read back, save over, rename, remove.
    void sequences()
    {
        QTemporaryDir home;
        SongLibrary lib(home.filePath("library"));
        const QJsonObject data{{"pattern", 3}}, sounds{{"korrode", QJsonObject{{"tone", 0.25}}}}, mixer{{"master", 0.7}};
        const qint64 b = lib.saveSequence(0, "bravo", data, sounds, mixer);
        const qint64 a = lib.saveSequence(0, "Alpha", {}, {}, {});
        QVERIFY(a > 0 && b > 0 && a != b);
        const auto list = lib.sequences();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].name, QStringLiteral("Alpha")); // by name, any case
        QVERIFY(list[1].data.isEmpty());                // the list carries no contents
        const auto got = lib.sequence(b);
        QVERIFY(got);
        QCOMPARE(got->data, data);
        QCOMPARE(got->sounds, sounds);
        QCOMPARE(got->mixer, mixer);
        QVERIFY(got->saved.isValid());
        QCOMPARE(lib.saveSequence(b, "bravo", QJsonObject{{"pattern", 4}}, sounds, mixer), b);
        QCOMPARE(lib.sequence(b)->data.value("pattern").toInt(), 4);
        QVERIFY(lib.renameSequence(b, "Charlie"));
        QCOMPARE(lib.sequence(b)->name, QStringLiteral("Charlie"));
        QVERIFY(lib.removeSequence(a));
        QVERIFY(!lib.sequence(a));
        QVERIFY(!lib.removeSequence(a));
        QCOMPARE(lib.saveSequence(9999, "x", {}, {}, {}), qint64(0)); // no such one to save over
    }

    // A library from before saved sequences (schema 2) opens and gains them, its songs kept.
    void upgradesAVersion2Library()
    {
        QTemporaryDir downloads, home;
        qint64 id = 0;
        {
            SongLibrary lib(home.filePath("library"));
            id = lib.add(write(downloads, "a.gp", "tab"), {}, details("A", "B", "C"))->id;
        }
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade2"));
            db.setDatabaseName(home.filePath("library/library.db"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("DROP TABLE sequences")));
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 2")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("downgrade2"));
        SongLibrary lib(home.filePath("library"));
        QVERIFY(lib.isOpen());
        QCOMPARE(lib.song(id)->details.title, QStringLiteral("C"));
        QVERIFY(lib.saveSequence(0, "first", {}, {}, {}) > 0);
        QCOMPARE(lib.sequences().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
