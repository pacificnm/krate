#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "SoundLibrary.h"

class QQmlEngine;
class QJSEngine;
class RackController;
namespace krate {
class InstrumentInstance;
class Previewer;
struct PreviewSound;
}

// QML singleton "Sounds": the sound library (docs/SOUNDS.md). Folders you add
// are indexed in place (scanned off the GUI thread); sounds are browsed by
// category, favourites and name, auditioned through Krate's output, and
// assigned to an instrument in the rack that takes sounds (Klang's pads,
// Kessel's drums), which copies the file in.
class SoundsController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sounds)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList folders READ folders NOTIFY changed)
    Q_PROPERTY(QStringList categoryNames READ categoryNames CONSTANT)
    Q_PROPERTY(QVariantList categoryCounts READ categoryCounts NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(int favouriteCount READ favouriteCount NOTIFY changed)
    Q_PROPERTY(int previewing READ previewing NOTIFY previewChanged) // the sound playing, -1: none

public:
    struct Device
    {
        int device; // rack device
        QString name;
        krate::InstrumentInstance *instance;
    };

    SoundsController(const QString &databaseFile, krate::Previewer &previewer, RackController &rack,
                     std::vector<Device> devices, QObject *parent = nullptr);
    ~SoundsController() override;
    static SoundsController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    bool scanning() const { return m_scanning.load(); }
    QString message() const { return m_message; }
    QStringList folders() const { return m_library.folders(); }
    QStringList categoryNames() const { return sounds::categoryNames(); }
    QVariantList categoryCounts() const;
    int total() const { return m_library.total(); }
    int favouriteCount() const { return m_library.favouriteCount(); }
    int previewing() const { return m_previewing; }

    // category: -1 all; favourites only; a search in names and folders.
    Q_INVOKABLE QVariantList list(int category, bool favourites, const QString &search) const;
    Q_INVOKABLE QVariantMap sound(int id) const { return m_library.sound(id); }
    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addFolderPath(const QString &folder);
    Q_INVOKABLE void removeFolder(const QString &folder);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void setCategory(int id, int category);
    Q_INVOKABLE void setFavourite(int id, bool favourite);
    Q_INVOKABLE void preview(int id);
    Q_INVOKABLE void stopPreview();
    Q_INVOKABLE QVariantList waveform(int id, int points); // 0..1 peaks
    // The instruments in the rack that take sounds: [{device, name, targets, contents}].
    Q_INVOKABLE QVariantList instruments() const;
    Q_INVOKABLE bool assign(int id, int device, int target);
    Q_INVOKABLE void clearTarget(int device, int target);

    void finish(); // before the audio host goes: no preview playing

signals:
    void changed();
    void previewChanged();

private:
    struct Decoded;
    std::shared_ptr<const Decoded> decode(int id);
    void scan(QStringList folders); // the worker thread's
    void freeRetired();
    void retirePlaying(); // the sound the previewer had, freed once the audio has moved on
    void bump();
    krate::InstrumentInstance *instanceOf(int device) const;

    static inline SoundsController *s_instance = nullptr;
    SoundLibrary m_library;
    krate::Previewer &m_previewer;
    RackController &m_rack;
    std::vector<Device> m_devices;
    int m_revision = 0;
    QString m_message;
    std::thread m_worker;
    std::atomic<bool> m_scanning{false}, m_quit{false};
    // The sound decoded last (its preview and waveform), and the one playing.
    std::shared_ptr<const Decoded> m_decoded;
    int m_previewing = -1;
    struct Retired
    {
        std::shared_ptr<const Decoded> sound;
        std::uint64_t block;
        QElapsedTimer since;
    };
    std::vector<Retired> m_retired;
    std::shared_ptr<const Decoded> m_playing; // handed to the previewer
    std::uint64_t m_playStart = 0;
    QTimer m_timer;
};
