#pragma once

#include <span>

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/MidiMapFile.h"
#include "core/ParamDescriptor.h"

namespace krate {
class IInstrument;
class MidiLearnMap;
}
class QQmlEngine;
class QJSEngine;

// QML singleton "MidiLearn" (docs/MIDI.md#midi-learn). Instrument-neutral:
// works with any krate::IInstrument through its parameter descriptors and its
// krate::MidiLearnMap, using keys throughout (the keys QML passes). A key is a
// parameter key or one of the app's action keys (learnable buttons such as
// "pattern1"); a mapped action press is announced with actionTriggered().
// In learn mode a click arms a control; the next CC binds to it on the audio
// thread. Keeps the GUI copy of the CC map, which is saved to midimap.json.
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
    // Not default-constructible on purpose: see SynthController. The instrument
    // must provide a MidiLearnMap.
    MidiLearnController(krate::IInstrument &instrument, const QString &filePath, QObject *parent,
                        const QStringList &actions = {});
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
    // A learned button was pressed: the app performs the action.
    void actionTriggered(const QString &action);

private:
    int indexOf(const QString &key) const; // parameter index or action target; kUnmapped if unknown
    QString keyOf(int target) const;
    void setArmed(const QString &paramKey);
    void mappingsEdited();

    static inline MidiLearnController *s_instance = nullptr;

    krate::MidiLearnMap &m_learnMap;
    std::span<const krate::ParamDescriptor> m_params;
    QStringList m_actions;
    QString m_filePath;
    krate::files::MidiMap m_map;
    bool m_active = false;
    QString m_armed;
    int m_revision = 0;
    QTimer m_pollTimer;
};
