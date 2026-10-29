#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <zlib.h>

#include "IrLibrary.h"

namespace {

// A mono 16-bit WAV of n samples; `seed` makes its contents (and id) differ.
QByteArray wav(int n, int seed)
{
    QByteArray b;
    const auto put = [&b](quint32 v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            b.append(char(v >> (8 * i)));
    };
    b.append("RIFF");
    put(quint32(36 + 2 * n), 4);
    b.append("WAVEfmt ");
    put(16, 4);
    put(1, 2);
    put(1, 2);
    put(44100, 4);
    put(88200, 4);
    put(2, 2);
    put(16, 2);
    b.append("data");
    put(quint32(2 * n), 4);
    for (int i = 0; i < n; ++i)
        put(quint32(qint16((i * 37 + seed * 1000) % 20000 - 10000)) & 0xFFFF, 2);
    return b;
}

// A zip with these (name, contents) entries, stored (not compressed).
QByteArray zip(const QList<std::pair<QByteArray, QByteArray>> &files)
{
    QByteArray out, central;
    const auto put = [](QByteArray &b, quint32 v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            b.append(char(v >> (8 * i)));
    };
    for (const auto &[name, data] : files) {
        const quint32 offset = quint32(out.size());
        const quint32 crc = quint32(crc32(0, reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size())));
        put(out, 0x04034b50, 4);
        put(out, 20, 2);
        put(out, 0, 2);
        put(out, 0, 2); // stored
        put(out, 0, 4); // time, date
        put(out, crc, 4);
        put(out, quint32(data.size()), 4);
        put(out, quint32(data.size()), 4);
        put(out, quint32(name.size()), 2);
        put(out, 0, 2);
        out.append(name);
        out.append(data);

        put(central, 0x02014b50, 4);
        put(central, 20, 2);
        put(central, 20, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 4);
        put(central, crc, 4);
        put(central, quint32(data.size()), 4);
        put(central, quint32(data.size()), 4);
        put(central, quint32(name.size()), 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 2);
        put(central, 0, 4);
        put(central, offset, 4);
        central.append(name);
    }
    const quint32 centralOffset = quint32(out.size());
    out.append(central);
    put(out, 0x06054b50, 4);
    put(out, 0, 4);
    put(out, quint32(files.size()), 2);
    put(out, quint32(files.size()), 2);
    put(out, quint32(central.size()), 4);
    put(out, centralOffset, 4);
    put(out, 0, 2);
    return out;
}

void write(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestIrLibrary : public QObject
{
    Q_OBJECT

private slots:
    void importsAZipPack()
    {
        QTemporaryDir dir;
        const QString pack = dir.filePath(QStringLiteral("Some 4x12 Pack.zip"));
        write(pack, zip({{"Room Stereo.wav", wav(100, 1)},
                         {"Mic A/Bright.wav", wav(100, 2)},
                         {"Mic A/Dark.wav", wav(100, 3)},
                         {"Mic A/readme.txt", "not an IR"},
                         {"__MACOSX/Mic A/._Bright.wav", "junk"},
                         {"Mic A/._Dark.wav", "junk"}}));
        IrLibrary lib(dir.filePath(QStringLiteral("library")));
        QVERIFY(lib.isOpen());
        const auto result = lib.import({pack});
        QCOMPARE(result.added, 3);
        QCOMPARE(result.failed, 0);
        const auto irs = lib.list();
        QCOMPARE(irs.size(), 3);
        QCOMPARE(irs[0].pack, QStringLiteral("Some 4x12 Pack"));
        QCOMPARE(irs[0].folder, QString()); // the pack's top first, then its folders
        QCOMPARE(irs[0].name, QStringLiteral("Room Stereo"));
        QCOMPARE(irs[1].folder, QStringLiteral("Mic A"));
        QCOMPARE(irs[1].name, QStringLiteral("Bright"));
        QCOMPARE(irs[2].name, QStringLiteral("Dark"));
        QCOMPARE(irs[1].rate, 44100);
        QCOMPARE(irs[1].frames, 100);
        // The library's copy is the same file, named by its id.
        QFile copy(lib.filePath(irs[1].id));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), wav(100, 2));
    }

    void importsFoldersAndFiles()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("My IRs"));
        write(root + QStringLiteral("/Cab One/close.wav"), wav(50, 4));
        write(root + QStringLiteral("/Cab One/Far/far.WAV"), wav(50, 5));
        write(root + QStringLiteral("/loose.wav"), wav(50, 6));
        write(root + QStringLiteral("/inner pack.zip"), zip({{"z.wav", wav(50, 7)}}));
        write(dir.filePath(QStringLiteral("single/one.wav")), wav(50, 8));
        IrLibrary lib(dir.filePath(QStringLiteral("library")));
        const auto result = lib.import({root, dir.filePath(QStringLiteral("single/one.wav"))});
        QCOMPARE(result.added, 5);
        QStringList seen;
        for (const auto &ir : lib.list())
            seen << ir.pack + u'|' + ir.folder + u'|' + ir.name;
        seen.sort();
        QCOMPARE(seen, (QStringList{QStringLiteral("My IRs|Cab One/Far|far"), QStringLiteral("My IRs|Cab One|close"),
                                    QStringLiteral("My IRs||loose"), QStringLiteral("inner pack||z"),
                                    QStringLiteral("single||one")}));
    }

    void neverTwice()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("a.wav")), wav(80, 9));
        write(dir.filePath(QStringLiteral("same contents.wav")), wav(80, 9));
        IrLibrary lib(dir.filePath(QStringLiteral("library")));
        auto r = lib.import({dir.filePath(QStringLiteral("a.wav")), dir.filePath(QStringLiteral("same contents.wav"))});
        QCOMPARE(r.added, 1);
        QCOMPARE(r.duplicates, 1);
        r = lib.import({dir.filePath(QStringLiteral("a.wav"))});
        QCOMPARE(r.added, 0);
        QCOMPARE(r.duplicates, 1);
    }

    void reportsWhatItCantRead()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("broken.wav")), "RIFF....WAVEjunk");
        write(dir.filePath(QStringLiteral("notes.txt")), "hello");
        IrLibrary lib(dir.filePath(QStringLiteral("library")));
        const auto r = lib.import({dir.filePath(QStringLiteral("broken.wav")), dir.filePath(QStringLiteral("notes.txt"))});
        QCOMPARE(r.added, 0);
        QCOMPARE(r.failed, 2);
        QCOMPARE(r.errors.size(), 2);
        QVERIFY(lib.list().isEmpty());
    }

    void filterFavouritesRemove()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("p.zip")), zip({{"Close Bright.wav", wav(40, 10)},
                                                          {"Close Dark.wav", wav(40, 11)},
                                                          {"Room.wav", wav(40, 12)}}));
        IrLibrary lib(dir.filePath(QStringLiteral("library")));
        lib.import({dir.filePath(QStringLiteral("p.zip"))});
        QCOMPARE(lib.list(QStringLiteral("close")).size(), 2);
        QCOMPARE(lib.list(QStringLiteral("CLOSE dark")).size(), 1);
        QCOMPARE(lib.list(QStringLiteral("p room")).size(), 1); // the pack's name counts

        const QString room = lib.list(QStringLiteral("room"))[0].id;
        lib.setFavourite(room, true);
        QCOMPARE(lib.list()[0].id, room); // favourites first
        QVERIFY(lib.find(room)->favourite);

        const QString file = lib.filePath(room);
        QVERIFY(QFile::exists(file));
        QVERIFY(lib.remove(room));
        QVERIFY(!QFile::exists(file));
        QVERIFY(!lib.find(room));
        QCOMPARE(lib.filePath(room), QString());
        QVERIFY(!lib.remove(room));
    }

    void keptAcrossRuns()
    {
        QTemporaryDir dir;
        write(dir.filePath(QStringLiteral("x.wav")), wav(30, 13));
        {
            IrLibrary lib(dir.filePath(QStringLiteral("library")));
            lib.import({dir.filePath(QStringLiteral("x.wav"))});
        }
        IrLibrary again(dir.filePath(QStringLiteral("library")));
        QCOMPARE(again.list().size(), 1);
        QCOMPARE(again.list()[0].name, QStringLiteral("x"));
    }
};

QTEST_GUILESS_MAIN(TestIrLibrary)
#include "tst_irlibrary.moc"
