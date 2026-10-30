#pragma once

#include <array>
#include <map>
#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "klang/dsp/Sample.h"

class QQmlEngine;
class QJSEngine;
class KlangController;

namespace klang {
class Engine;
}

// QML singleton "Pads": the 16 pads' samples and settings
// (docs/BRIEF.md#scope-v1). Loaded files (WAV or MP3) are copied into Klang's
// sample library (~/.local/share/klang/samples) so the pads keep working if the
// originals move; each is decoded once, off the audio thread, and shared by the
// pads that play it (slices). A replaced sample is freed once the engine has
// moved two blocks past it. pads.json holds every pad's sample and settings
// (the panel's panel.json only the edited pad's), saved 2 s after a change.
class KlangPadController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Pads)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)        // bumped on every change
    Q_PROPERTY(QStringList library READ library NOTIFY changed)  // the library's files, sorted
    Q_PROPERTY(QString message READ message NOTIFY changed)      // the last load's or slice's result

public:
    static constexpr int kPads = 16;

    // Loads pads.json into the engine: construct it before processing starts.
    KlangPadController(KlangController &panel, klang::Engine &engine, const QString &dataDir, QObject *parent);
    ~KlangPadController() override;

    static KlangPadController *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }
    QStringList library() const { return m_library; }
    QString message() const { return m_message; }
    QString libraryDir() const { return m_libraryDir; }

    // Files onto pads: the first onto `pad`, the next onto the pads after it.
    Q_INVOKABLE void loadUrls(int pad, const QList<QUrl> &urls);
    Q_INVOKABLE bool loadFromLibrary(int pad, const QString &name);
    Q_INVOKABLE void step(int pad, int delta); // the next or previous file in the library
    Q_INVOKABLE void clear(int pad);
    Q_INVOKABLE QString name(int pad) const;    // its file, "" = none
    Q_INVOKABLE double seconds(int pad) const;  // the whole sample's length
    // The sample's outline, `points` peaks 0..1 (for the waveform view).
    Q_INVOKABLE QVariantList waveform(int pad, int points) const;

    // Slicing: the pad's region cut into `count` equal slices (4, 8, 16), or
    // at its hits, onto it and the pads after it (as far as pad 16).
    Q_INVOKABLE void slice(int pad, int count);
    Q_INVOKABLE void sliceAtHits(int pad);
    // Where the hits are in `pad`'s region (fractions of the sample), for tests and the view.
    std::vector<double> hits(int pad) const;

    void save();

signals:
    void changed();

private:
    using SamplePtr = std::shared_ptr<const klang::Sample>;

    SamplePtr decode(const QString &path, QString *error);
    QString import(const QString &path, QString *error); // into the library: its name there
    void use(int pad, const SamplePtr &sample, const QString &name);
    void sliceInto(int pad, const std::vector<double> &cuts); // cuts: fractions, ascending, first = start
    void rescan();
    void load();
    void edited(); // notify, save in 2 s
    void freeRetired();

    static inline KlangPadController *s_instance = nullptr;

    KlangController &m_panel;
    klang::Engine &m_engine;
    QString m_libraryDir;
    QString m_path;
    QStringList m_library;
    QString m_message;
    int m_revision = 0;
    std::array<SamplePtr, kPads> m_samples{};
    std::array<QString, kPads> m_names{};
    std::map<QString, std::weak_ptr<const klang::Sample>> m_decoded; // shared by the pads playing it
    struct Retired
    {
        SamplePtr sample;
        std::uint64_t block;
    };
    std::vector<Retired> m_retired;
    QTimer m_freeTimer;
    QTimer m_saveTimer;
};
