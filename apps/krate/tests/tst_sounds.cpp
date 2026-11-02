#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "SoundIndex.h"
#include "SoundLibrary.h"
#include "rec/WavWriter.h"

using sounds::Category;

// The sound library (docs/SOUNDS.md): categories guessed from names, WAV
// lengths from their headers, folders indexed in place, your categories and
// favourites kept through a rescan, files gone dropped.
class TestSounds : public QObject
{
    Q_OBJECT

    static void writeWav(const QString &path, double seconds, int rate = 44100, int channels = 2)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        krate::WavWriter w;
        QVERIFY(w.open(path.toStdString(), rate, channels));
        std::vector<float> l(std::size_t(seconds * rate), 0.25f);
        QVERIFY(w.write(l.data(), l.data(), l.size()));
        QVERIFY(w.close());
    }

private slots:
    void guessesCategories()
    {
        const struct
        {
            const char *path;
            Category category;
        } cases[] = {
            {"Kick 01.wav", Category::Kick},
            {"BD_02.wav", Category::Kick},
            {"Drum Kits/Dusty/Bass Drum Long.wav", Category::Kick}, // not Bass
            {"SD-tight.wav", Category::Snare},
            {"OpenHat03.wav", Category::HiHat},
            {"hh_closed_01.wav", Category::HiHat},
            {"Claps/Clap Big.wav", Category::Clap},
            {"Crash 2.wav", Category::Cymbal},
            {"Floor Tom.wav", Category::Tom},
            {"Percussion/Shaker 1.wav", Category::Percussion},
            {"rimshot.wav", Category::Percussion},
            {"Sub Bass C.wav", Category::Bass},
            {"Synth Leads/Saw Lead A.wav", Category::Synth},
            {"Piano Chord Em.wav", Category::Keys},
            {"Vox/Chant hey.wav", Category::Vocal},
            {"Riser 8 bars.wav", Category::FX},
            {"Drum Loops/Funky 120 BPM.wav", Category::Loop},
            {"kick loop 128bpm.wav", Category::Loop}, // a loop of kicks is a loop
            {"Snares/Hit 4.wav", Category::Snare},    // from its folder
            {"Kicks/Snare 4.wav", Category::Snare},   // its own name first
            {"untitled 7.wav", Category::Other},
        };
        for (const auto &c : cases)
            QVERIFY2(sounds::guessCategory(QString::fromUtf8(c.path)) == c.category, c.path);
        QCOMPARE(sounds::categoryNames().size(), int(Category::Count));
    }

    void readsWavHeaders()
    {
        QTemporaryDir dir;
        writeWav(dir.filePath("a/b/tone.wav"), 1.5, 44100, 2);
        writeWav(dir.filePath("mono.WAV"), 0.25, 48000, 1);
        QFile notes(dir.filePath("notes.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.close();
        const QStringList found = sounds::findSoundFiles(dir.path());
        QCOMPARE(found.size(), 2);
        const sounds::FileInfo tone = sounds::soundInfo(dir.filePath("a/b/tone.wav"));
        QVERIFY(std::fabs(tone.seconds - 1.5) < 1e-4);
        QCOMPARE(tone.rate, 44100);
        QCOMPARE(tone.channels, 2);
        QCOMPARE(sounds::soundInfo(dir.filePath("mono.WAV")).channels, 1);
        QCOMPARE(sounds::soundInfo(dir.filePath("notes.txt")).seconds, 0.0);
    }

    // A sound right in the folder (no subfolder): indexed too (its dir "").
    void indexesTheFoldersOwnFiles()
    {
        QTemporaryDir dir;
        const QString sounds = dir.filePath("sounds");
        writeWav(sounds + "/Big Clap.wav", 0.3);
        writeWav(sounds + "/pack/Kick 1.wav", 0.5);
        SoundLibrary lib(dir.filePath("sounds.db"));
        QVERIFY(lib.addFolder(sounds));
        std::vector<sounds::FileInfo> files;
        for (const QString &f : sounds::findSoundFiles(sounds))
            files.push_back(sounds::soundInfo(f));
        QCOMPARE(lib.update(QDir(sounds).absolutePath(), files), 2);
        SoundLibrary::Query search;
        search.search = QStringLiteral("clap");
        const QVariantList found = lib.sounds(search);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].toMap().value("dir").toString(), QString());
    }

    void indexesFolders()
    {
        QTemporaryDir dir;
        const QString pack = dir.filePath("pack");
        writeWav(pack + "/Kicks/Kick 1.wav", 0.5);
        writeWav(pack + "/Kicks/Kick 2.wav", 0.5);
        writeWav(pack + "/Snares/Snare A.wav", 0.4);
        writeWav(pack + "/Loops/Groove 90.wav", 2.0);
        const auto scan = [&] {
            std::vector<sounds::FileInfo> files;
            for (const QString &f : sounds::findSoundFiles(pack))
                files.push_back(sounds::soundInfo(f));
            return files;
        };
        int snareId = -1;
        {
            SoundLibrary lib(dir.filePath("sounds.db"));
            QVERIFY(lib.isOpen());
            QVERIFY(lib.addFolder(pack));
            QVERIFY(!lib.addFolder(pack + "/Kicks")); // inside one already
            QCOMPARE(lib.update(pack, scan()), 4);
            QCOMPARE(lib.total(), 4);
            const auto counts = lib.categoryCounts();
            QCOMPARE(counts[std::size_t(Category::Kick)], 2);
            QCOMPARE(counts[std::size_t(Category::Loop)], 1);
            SoundLibrary::Query kicks;
            kicks.category = int(Category::Kick);
            const QVariantList list = lib.sounds(kicks);
            QCOMPARE(list.size(), 2);
            QCOMPARE(list[0].toMap().value("name").toString(), QStringLiteral("Kick 1"));
            QCOMPARE(list[0].toMap().value("dir").toString(), QStringLiteral("Kicks"));
            QVERIFY(std::fabs(list[0].toMap().value("seconds").toDouble() - 0.5) < 1e-3);
            SoundLibrary::Query search;
            search.search = QStringLiteral("snare");
            snareId = lib.sounds(search).value(0).toMap().value("id").toInt();
            lib.setCategory(snareId, int(Category::Clap)); // yours
            lib.setFavourite(snareId, true);
            QCOMPARE(lib.favouriteCount(), 1);
        }
        // Rescanned (a file gone, one new): yours kept, the gone one dropped.
        QFile::remove(pack + "/Kicks/Kick 2.wav");
        writeWav(pack + "/Hats/Hat 1.wav", 0.2);
        SoundLibrary lib(dir.filePath("sounds.db"));
        QCOMPARE(lib.folders(), QStringList{QDir(pack).absolutePath()});
        QCOMPARE(lib.update(QDir(pack).absolutePath(), scan()), 4);
        const QVariantMap snare = lib.sound(snareId);
        QCOMPARE(snare.value("category").toInt(), int(Category::Clap));
        QVERIFY(snare.value("favorite").toBool());
        QCOMPARE(lib.categoryCounts()[std::size_t(Category::Kick)], 1);
        QCOMPARE(lib.categoryCounts()[std::size_t(Category::HiHat)], 1);
        SoundLibrary::Query favourites;
        favourites.favourites = true;
        QCOMPARE(lib.sounds(favourites).size(), 1);
        lib.removeFolder(QDir(pack).absolutePath());
        QCOMPARE(lib.total(), 0);
        QVERIFY(lib.folders().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSounds)
#include "tst_sounds.moc"
