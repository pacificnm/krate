#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// QML singleton "MidiLearn": the MIDI Learn API every panel's LearnOverlay uses
// (docs/MIDI.md#midi-learn). Keys are parameter keys or action keys (learnable
// buttons). Two implementations: krate::InstrumentMidiLearn (a standalone app:
// one instrument's map, midimap.json) and the rack's RackMidiLearn (the rack-
// wide map: keys refer to the focused instrument; rack.json).
class MidiLearnController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MidiLearn)
    QML_SINGLETON

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString armedParam READ armedParam NOTIFY armedParamChanged) // key, "" = none
    // Bumped on every mapping change; bind to it to refresh badges.
    Q_PROPERTY(int revision READ revision NOTIFY mappingsChanged)
    // Rack only: learned knobs follow the focused instrument instead of staying fixed.
    Q_PROPERTY(bool followFocus READ followFocus WRITE setFollowFocus NOTIFY followFocusChanged)
    Q_PROPERTY(bool canFollowFocus READ canFollowFocus CONSTANT)

public:
    ~MidiLearnController() override;

    static MidiLearnController *create(QQmlEngine *, QJSEngine *);

    bool active() const { return m_active; }
    void setActive(bool active);
    QString armedParam() const { return m_armed; }
    int revision() const { return m_revision; }
    bool followFocus() const { return m_followFocus; }
    void setFollowFocus(bool on);
    virtual bool canFollowFocus() const { return false; }

    // Arm a control for learning (or disarm it if already armed). Learn mode only.
    Q_INVOKABLE void arm(const QString &key);
    // Mapped sources for a key, e.g. "21 74"; "" if none.
    Q_INVOKABLE virtual QString mappingFor(const QString &key) const = 0;
    // The badge a learn overlay shows, e.g. "CC 21" ("" if unmapped).
    Q_INVOKABLE virtual QString badgeFor(const QString &key) const;
    Q_INVOKABLE virtual void clear(const QString &key) = 0;
    Q_INVOKABLE virtual void clearAll() = 0;

public slots:
    // Called at 60 Hz: learned mappings and pressed buttons from the audio thread.
    virtual void drainEvents() = 0;

signals:
    void activeChanged();
    void armedParamChanged();
    void mappingsChanged();
    void followFocusChanged();
    // A learned button was pressed: the app performs the action.
    void actionTriggered(const QString &action);

protected:
    // Not default-constructible: see Korrode's SynthController.
    explicit MidiLearnController(QObject *parent);

    // Arm `key` in the backend ("" disarms); false if the key can't be learned.
    virtual bool armTarget(const QString &key) = 0;
    // A key got its mapping: disarm it if it was armed, refresh badges.
    void learned(const QString &key);
    void mappingsEdited(); // refresh badges

private:
    static inline MidiLearnController *s_instance = nullptr;

    bool m_active = false;
    QString m_armed;
    int m_revision = 0;
    bool m_followFocus = false;
    QTimer m_pollTimer;
};
