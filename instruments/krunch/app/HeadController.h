#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "dsp/Head.h"

namespace krunch {
class Krunch;
}

// QML type "Heads" (one per instrument, given to its panel): amp heads
// (docs/HEADS.md), the .json files in Krunch's heads folder
// (~/.local/share/krunch/heads: from krunch-fit, put there by hand, by Krate
// Remote, or IMPORT), and which one each channel's tubes are built as (""
// = the channel's VOICE). A head replaced is freed once the audio thread has
// moved on. Choices are kept in heads.json (by file name) and in songs'
// sounds and presets.
class HeadController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Heads)
    QML_UNCREATABLE("one per instrument, given to its panel")

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QStringList names READ names NOTIFY changed) // the folder's heads, by file name
    Q_PROPERTY(QString folder READ folder CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    static constexpr int kChannels = 4;

    HeadController(krunch::Krunch &engine, const QString &folder, const QString &filePath, QObject *parent);
    ~HeadController() override;

    int revision() const { return m_revision; }
    QStringList names() const { return m_names; }
    QString folder() const { return m_folder; }
    QString message() const { return m_message; }

    Q_INVOKABLE QString head(int channel) const; // "" = the VOICE's built-in head
    Q_INVOKABLE void choose(int channel, const QString &name);
    Q_INVOKABLE QString title(const QString &name) const; // the head's own name
    Q_INVOKABLE QString about(const QString &name) const; // how it was made: "fitted to …"
    Q_INVOKABLE bool ownPowerTubes(int channel) const;    // its power amp isn't the TUBES stepper's
    // .json heads (file:// URLs or paths) copied into the folder.
    Q_INVOKABLE void importUrls(const QVariantList &urls);

    QJsonArray save() const;
    void load(const QJsonArray &channels);

signals:
    void changed();

private:
    struct Entry
    {
        QString title, about;
    };
    void rescan();
    void apply(int channel); // reads the chosen file and hands the head to the engine
    void bump();
    void saveFile();
    void loadFile();

    krunch::Krunch &m_engine;
    QString m_folder, m_filePath;
    QStringList m_names;
    std::vector<Entry> m_entries; // by m_names
    QString m_message;
    int m_revision = 0;
    std::array<QString, kChannels> m_chosen;
    std::array<std::unique_ptr<krunch::HeadDesign>, kChannels> m_live;
    struct Retired
    {
        std::unique_ptr<krunch::HeadDesign> head;
        quint64 after;
    };
    std::vector<Retired> m_retired;
    QFileSystemWatcher m_watcher;
    QTimer m_poll;
};
