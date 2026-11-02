#pragma once

#include <array>
#include <future>
#include <memory>
#include <vector>

#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace krunch {
class CaptureModel;
class Krunch;
} // namespace krunch

// QML type "Captures" (one per instrument, given to its panel): amp captures
// (docs/CAPTURES.md), the .nam files in Krunch's captures folder
// (~/.local/share/krunch/captures: put there by hand, by Krate Remote, or
// IMPORT), and which one each channel plays ("" = Krunch's own preamp and
// power amp). A capture is loaded off the GUI thread and handed to the audio
// thread; one replaced is freed once the audio thread has moved on. Choices
// are kept in captures.json (by file name) and in songs' sounds and presets.
class CaptureController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Captures)
    QML_UNCREATABLE("one per instrument, given to its panel")

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QStringList names READ names NOTIFY changed) // the folder's captures, by name
    Q_PROPERTY(QString folder READ folder CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY changed) // the last thing that went wrong (or was imported)

public:
    static constexpr int kChannels = 4;

    CaptureController(krunch::Krunch &engine, const QString &folder, const QString &filePath, QObject *parent);
    ~CaptureController() override;

    int revision() const { return m_revision; }
    QStringList names() const { return m_names; }
    QString folder() const { return m_folder; }
    QString message() const { return m_message; }

    // A channel's capture ("" = none) and whether it's playing yet (loaded).
    Q_INVOKABLE QString capture(int channel) const;
    Q_INVOKABLE bool loading(int channel) const;
    Q_INVOKABLE void choose(int channel, const QString &name); // "" = Krunch's own amp
    // .nam files (file:// URLs or paths) copied into the folder.
    Q_INVOKABLE void importUrls(const QVariantList &urls);

    QJsonArray save() const;
    void load(const QJsonArray &channels);

signals:
    void changed();

private:
    struct Loading
    {
        QString name;
        double rate = 0.0;
        std::future<std::shared_ptr<krunch::CaptureModel>> model;
        std::shared_ptr<QString> error;
    };

    void rescan();
    void start(int channel); // loads the channel's capture (in the background)
    void poll();             // loads finished, the rate changed, retired models freed
    void bump();
    void saveFile();
    void loadFile();

    krunch::Krunch &m_engine;
    QString m_folder, m_filePath;
    QStringList m_names;
    QString m_message;
    int m_revision = 0;
    std::array<QString, kChannels> m_chosen;
    std::array<std::shared_ptr<krunch::CaptureModel>, kChannels> m_live;
    std::array<Loading, kChannels> m_loading;
    struct Retired
    {
        std::shared_ptr<krunch::CaptureModel> model;
        quint64 after;
    };
    std::vector<Retired> m_retired;
    double m_rate = 0.0;
    QFileSystemWatcher m_watcher;
    QTimer m_poll;
};
