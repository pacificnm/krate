#pragma once

#include <optional>

#include <QList>
#include <QString>
#include <QStringList>

// Krunch's impulse-response library (docs/DSP.md#ir-library): an SQLite
// database next to copies of every IR, each file named by its contents'
// SHA-256, which is also its id (so an IR is never added twice, and songs and
// presets refer to IRs by id). Imports WAV files, folders and zip packs.
// GUI thread.
class IrLibrary
{
public:
    static constexpr int kSchemaVersion = 1;

    struct Ir
    {
        QString id;     // SHA-256 of the file, hex
        QString pack;   // the zip or folder it came in
        QString folder; // inside the pack ("" at its top)
        QString name;   // the file's name without .wav
        int channels = 1;
        int rate = 0;
        int frames = 0;
        bool favourite = false;
    };

    struct ImportResult
    {
        int added = 0;
        int duplicates = 0; // already in the library
        int failed = 0;     // not a WAV we can read
        QStringList errors; // a few, for the message
    };

    explicit IrLibrary(const QString &directory);
    ~IrLibrary();

    IrLibrary(const IrLibrary &) = delete;
    IrLibrary &operator=(const IrLibrary &) = delete;

    bool isOpen() const { return m_open; }
    QString directory() const { return m_directory; }

    // Files (.wav, .zip) and folders (searched for both), by local path.
    ImportResult import(const QStringList &paths);

    // Favourites first, then by pack, folder and name. `filter`: words that
    // must all appear in the pack, folder or name (case-insensitive).
    QList<Ir> list(const QString &filter = {}) const;
    std::optional<Ir> find(const QString &id) const;
    // The library's copy of the IR's file ("" if there is none).
    QString filePath(const QString &id) const;
    void setFavourite(const QString &id, bool favourite);
    bool remove(const QString &id);

private:
    bool createSchema();
    void importFile(const QString &path, const QString &pack, const QString &folder, ImportResult &result);
    void importZip(const QString &path, ImportResult &result);
    void importBytes(const QByteArray &bytes, const QString &pack, const QString &folder, const QString &name,
                     ImportResult &result);

    QString m_directory;
    QString m_connection;
    bool m_open = false;
};
