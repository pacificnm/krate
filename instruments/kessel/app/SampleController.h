#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include "kessel/dsp/Sample.h"

class QQmlEngine;
class QJSEngine;

namespace kessel {
class Engine;
}

// QML singleton "Samples": the SAMPLE voice's sample library (docs/KITS.md#samples).
// Imported WAV files are copied into Kessel's samples folder (so kits keep
// working if the originals move); the arrows step through them; the choice is
// saved in samples.json. Decoding happens here, off the audio thread; a
// replaced sample is freed once the engine has moved two blocks past it.
class SampleController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Samples)
    QML_SINGLETON

    Q_PROPERTY(QString name READ name NOTIFY changed)         // the file playing, "" = none
    Q_PROPERTY(QStringList names READ names NOTIFY changed)   // the library, sorted
    Q_PROPERTY(double seconds READ seconds NOTIFY changed)    // its length as played
    Q_PROPERTY(QString message READ message NOTIFY changed)   // the last import's result

public:
    // Not default-constructible on purpose (see Korrode's SynthController).
    SampleController(kessel::Engine &engine, const QString &dataDir, QObject *parent);
    ~SampleController() override;

    static SampleController *create(QQmlEngine *, QJSEngine *);

    QString name() const { return m_name; }
    QStringList names() const { return m_names; }
    double seconds() const;
    QString message() const { return m_message; }
    QString libraryDir() const { return m_libraryDir; }

    Q_INVOKABLE void importUrls(const QList<QUrl> &urls); // WAV files; the last one imported plays
    Q_INVOKABLE bool select(const QString &name);         // a file in the library; false if it won't load
    Q_INVOKABLE void step(int delta);                     // the arrows
    Q_INVOKABLE void clear();                             // no sample

signals:
    void changed();

private:
    void rescan();
    void use(std::unique_ptr<kessel::Sample> sample, const QString &name);
    void freeRetired();
    void saveState() const;

    static inline SampleController *s_instance = nullptr;

    kessel::Engine &m_engine;
    QString m_libraryDir;
    QString m_statePath;
    QString m_name;
    QStringList m_names;
    QString m_message;
    std::unique_ptr<kessel::Sample> m_current;
    struct Retired
    {
        std::unique_ptr<kessel::Sample> sample;
        std::uint64_t block; // the engine's block count when it was replaced
    };
    std::vector<Retired> m_retired;
    QTimer m_freeTimer;
};
