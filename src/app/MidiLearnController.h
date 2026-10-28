#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/Persistence.h"

namespace k303 {
class Engine;
}
class QQmlEngine;
class QJSEngine;

// QML singleton "MidiLearn" (docs/MIDI.md#midi-learn). In learn mode a click
// arms a control; the next CC the engine sees binds to it (the binding itself
// happens on the audio thread). Keeps the GUI copy of the CC map, which is the
// one saved to midimap.json, and drains the engine's audio->GUI events.
class MidiLearnController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MidiLearn)
    QML_SINGLETON

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString armedParam READ armedParam NOTIFY armedParamChanged) // param key, "" = none
    // Bumped on every mapping change; bind to it to refresh mappingFor() badges.
    Q_PROPERTY(int revision READ revision NOTIFY mappingsChanged)

public:
    // Not default-constructible on purpose: see SynthController.
    MidiLearnController(k303::Engine &engine, const QString &filePath, QObject *parent);
    ~MidiLearnController() override;

    static MidiLearnController *create(QQmlEngine *, QJSEngine *);

    bool active() const { return m_active; }
    void setActive(bool active);
    QString armedParam() const { return m_armed; }
    int revision() const { return m_revision; }
    QString inputPortMatch() const { return m_map.inputPortMatch; }

    // Arm a control for learning (or disarm it if already armed).
    Q_INVOKABLE void arm(const QString &paramKey);
    // CC numbers mapped to a parameter, e.g. "21" or "21 74"; "" if none.
    Q_INVOKABLE QString mappingFor(const QString &paramKey) const;
    Q_INVOKABLE void clear(const QString &paramKey);
    Q_INVOKABLE void clearAll();

public slots:
    // Called at 60 Hz; public for tests.
    void drainEvents();

signals:
    void activeChanged();
    void armedParamChanged();
    void mappingsChanged();
    // A MIDI key was pressed (for step record, later).
    void notePressed(int note, int velocity);

private:
    void setArmed(const QString &paramKey);
    void mappingsEdited();

    static inline MidiLearnController *s_instance = nullptr;

    k303::Engine &m_engine;
    QString m_filePath;
    k303::persistence::MidiMap m_map;
    bool m_active = false;
    QString m_armed;
    int m_revision = 0;
    QTimer m_pollTimer;
};
