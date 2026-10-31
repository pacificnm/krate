#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "IrLibrary.h"

class QQmlEngine;
class QJSEngine;

namespace krunch {
class IrKernel;
class Krunch;
} // namespace krunch

// QML singleton "Cabinets": the IR library and which IRs each channel plays
// (A and B). Loads an IR when it's chosen (resampled to the engine's rate) and
// hands it to the audio thread; rebuilds them all when the rate changes.
// Choices are kept in cabinets.json (by IR id) and in songs' sounds.
class CabinetController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Cabinets)
    QML_SINGLETON

    // Bumped when a channel's IRs or the library change.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int libraryCount READ libraryCount NOTIFY changed)
    // What the last import did, for the panel ("" before any).
    Q_PROPERTY(QString importMessage READ importMessage NOTIFY changed)

public:
    static constexpr int kChannels = 4;

    // Not default-constructible on purpose (see Korrode's SynthController).
    CabinetController(krunch::Krunch &engine, const QString &libraryDirectory, const QString &filePath, QObject *parent);
    ~CabinetController() override;

    static CabinetController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    int libraryCount() const;
    QString importMessage() const { return m_importMessage; }

    // The IR library, filtered (see IrLibrary::list): [{id, pack, folder, name, channels, rate, seconds, favourite}].
    Q_INVOKABLE QVariantList irs(const QString &filter = {}) const;
    // Files and folders (file:// URLs or paths): WAVs, zips, folders of either.
    Q_INVOKABLE void importUrls(const QVariantList &urls);
    Q_INVOKABLE void setFavourite(const QString &id, bool favourite);
    Q_INVOKABLE void removeIr(const QString &id);

    // A channel's IR A (b false) or B: its id ("" = none) and its name.
    Q_INVOKABLE QString irId(int channel, bool b) const;
    Q_INVOKABLE QString irName(int channel, bool b) const;
    Q_INVOKABLE void choose(int channel, bool b, const QString &id);

    // For songs' sounds: every channel's [a, b] ids.
    QJsonArray save() const;
    void load(const QJsonArray &channels);

signals:
    void changed();

private:
    static int slot(int channel, bool b) { return 2 * channel + (b ? 1 : 0); }
    void build(int slot); // (re)loads the slot's IR into the engine
    void poll();          // the engine's rate; frees retired kernels
    void bump();
    void saveFile();
    void loadFile();

    static inline CabinetController *s_instance = nullptr;

    krunch::Krunch &m_engine;
    IrLibrary m_library;
    QString m_filePath;
    std::array<QString, 2 * kChannels> m_ids;
    std::array<std::unique_ptr<krunch::IrKernel>, 2 * kChannels> m_live;
    struct Retired
    {
        std::unique_ptr<krunch::IrKernel> kernel;
        quint64 after; // the engine's block count when it was replaced
    };
    std::vector<Retired> m_retired;
    double m_builtRate = 0.0;
    int m_revision = 0;
    QString m_importMessage;
    QTimer m_pollTimer;
    QTimer m_saveTimer;
};
