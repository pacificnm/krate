#pragma once

#include <array>

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

// QML type "Samples" (one per instrument, given to its panel): the SAMPLE voice's sample library (docs/KITS.md#samples).
// Imported WAV files are copied into Kessel's samples folder (so kits keep
// working if the originals move); the arrows step through them; the choice is
// saved in samples.json. Decoding happens here, off the audio thread; a
// replaced sample is freed once the engine has moved two blocks past it.
//
// Any other drum can play a sample of the library too, instead of its
// synthesized sound (the sound library, docs/SOUNDS.md): kept in
// samples.json by drum and carried by the kits.
class SampleController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Samples)
    QML_UNCREATABLE("one per instrument, given to its panel")

    Q_PROPERTY(QString name READ name NOTIFY changed)         // the file playing, "" = none
    Q_PROPERTY(QStringList names READ names NOTIFY changed)   // the library, sorted
    Q_PROPERTY(double seconds READ seconds NOTIFY changed)    // its length as played
    Q_PROPERTY(QString message READ message NOTIFY changed)   // the last import's result
    Q_PROPERTY(QStringList drumSamples READ drumSamples NOTIFY changed) // per drum (DrumId order): its file, "" = its synth

public:
    // Not default-constructible on purpose (see Korrode's SynthController).
    SampleController(kessel::Engine &engine, const QString &dataDir, QObject *parent);
    ~SampleController() override;


    QString name() const { return m_name; }
    QStringList names() const { return m_names; }
    double seconds() const;
    QString message() const { return m_message; }
    QString libraryDir() const { return m_libraryDir; }

    Q_INVOKABLE void importUrls(const QList<QUrl> &urls); // WAV files; the last one imported plays
    Q_INVOKABLE bool select(const QString &name);         // a file in the library; false if it won't load
    Q_INVOKABLE void step(int delta);                     // the arrows
    Q_INVOKABLE void clear();                             // no sample
    // A drum's own sample: a file in the library ("": its synth again; the
    // SAMPLE drum: select / clear). false if it won't load.
    Q_INVOKABLE bool setDrumSample(int drum, const QString &name);
    Q_INVOKABLE QString drumSample(int drum) const;
    // A WAV file from anywhere, copied into the library and given to `drum`.
    Q_INVOKABLE bool assignFile(int drum, const QString &path);
    QStringList drumSamples() const;
    static QString drumKey(int drum); // "bd", "sd", ... (files)

signals:
    void changed();

private:
    void rescan();
    QString copyIn(const QString &path, QString *error); // into the library: its name there, "" if not
    void use(std::unique_ptr<kessel::Sample> sample, const QString &name);
    void freeRetired();
    void saveState() const;


    kessel::Engine &m_engine;
    QString m_libraryDir;
    QString m_statePath;
    QString m_name;
    QStringList m_names;
    QString m_message;
    std::unique_ptr<kessel::Sample> m_current;
    std::array<std::unique_ptr<kessel::Sample>, 16> m_drum;
    std::array<QString, 16> m_drumNames;
    struct Retired
    {
        std::unique_ptr<kessel::Sample> sample;
        std::uint64_t block; // the engine's block count when it was replaced
    };
    std::vector<Retired> m_retired;
    QTimer m_freeTimer;
};
