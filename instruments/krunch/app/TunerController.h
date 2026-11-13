#pragma once

#include <vector>

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "dsp/Tuner.h"

namespace krunch {
class Krunch;
}

// QML type "Tuner" (one per instrument, given to its panel): the tuner
// (docs/MANUAL.md#tuner). While open it reads the input's pitch about 30 times
// a second, against the chosen tuning's strings and the reference A4, and
// (with MUTE) silences the amp. The tuning, reference and MUTE are kept in
// tuner.json: they belong to the player, not to a preset or song.
class TunerController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Tuner)
    QML_UNCREATABLE("one per instrument, given to its panel")

    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    // Silence the amp while the tuner is open (as a tuner pedal does).
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY settingsChanged)
    // A4, kMinReferenceHz..kMaxReferenceHz (440 by default), to 0.1 Hz.
    Q_PROPERTY(qreal referenceHz READ referenceHz WRITE setReferenceHz NOTIFY settingsChanged)
    Q_PROPERTY(qreal minReferenceHz READ minReferenceHz CONSTANT)
    Q_PROPERTY(qreal maxReferenceHz READ maxReferenceHz CONSTANT)
    Q_PROPERTY(QString tuning READ tuning WRITE setTuning NOTIFY settingsChanged)
    // Every tuning: [{id, name, group}], Chromatic first.
    Q_PROPERTY(QVariantList tunings READ tunings CONSTANT)
    // The chosen tuning's open strings, lowest first: [{name, octave, hz}] (none in Chromatic).
    Q_PROPERTY(QVariantList strings READ strings NOTIFY settingsChanged)

    // The reading (steadied): a pitch is heard; its target note (the nearest
    // string's, or in Chromatic the nearest note), that string (-1 in
    // Chromatic), and how far off it is (cents, below is flat).
    Q_PROPERTY(bool hasPitch READ hasPitch NOTIFY readingChanged)
    Q_PROPERTY(qreal frequency READ frequency NOTIFY readingChanged)
    Q_PROPERTY(QString note READ note NOTIFY readingChanged)
    Q_PROPERTY(int octave READ octave NOTIFY readingChanged)
    Q_PROPERTY(int string READ string NOTIFY readingChanged)
    Q_PROPERTY(qreal cents READ cents NOTIFY readingChanged)

public:
    TunerController(krunch::Krunch &engine, const QString &filePath, QObject *parent);
    ~TunerController() override;

    bool isOpen() const { return m_open; }
    void setOpen(bool open);
    Q_INVOKABLE void toggle() { setOpen(!m_open); } // learnable as "tuner"
    bool mute() const { return m_mute; }
    void setMute(bool mute);
    qreal referenceHz() const { return m_referenceHz; }
    void setReferenceHz(qreal hz);
    static qreal minReferenceHz() { return krunch::tuner::kMinReferenceHz; }
    static qreal maxReferenceHz() { return krunch::tuner::kMaxReferenceHz; }
    QString tuning() const { return QString::fromUtf8(m_tuning->id); }
    void setTuning(const QString &id);
    QVariantList tunings() const;
    QVariantList strings() const;

    bool hasPitch() const { return m_hasPitch; }
    qreal frequency() const { return m_frequency; }
    QString note() const { return m_note; }
    int octave() const { return m_octave; }
    int string() const { return m_string; }
    qreal cents() const { return m_cents; }

signals:
    void openChanged();
    void settingsChanged();
    void readingChanged();

private:
    void poll();
    void settingsEdited();
    void saveFile();
    void updateMute();

    krunch::Krunch &m_engine;
    QString m_filePath;
    QTimer m_pollTimer, m_saveTimer;
    krunch::PitchDetector m_detector;
    std::vector<float> m_samples;

    bool m_open = false;
    bool m_mute = true;
    qreal m_referenceHz = krunch::tuner::kDefaultReferenceHz;
    const krunch::tuner::Tuning *m_tuning;

    double m_smoothed = -1.0; // the played note, steadied (fractional MIDI note), < 0: none
    int m_misses = 0;         // polls without a pitch since the last
    bool m_hasPitch = false;
    qreal m_frequency = 0.0;
    QString m_note;
    int m_octave = 0;
    int m_string = -1;
    qreal m_cents = 0.0;
};
