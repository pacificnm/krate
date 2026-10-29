#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace krate {
class Rack;
class Transport;
struct InstrumentEntry;
}
class QQmlEngine;
class QJSEngine;

// QML singleton "Rack": the krate window's view of the rack: the devices and
// their panels, focus, the shared transport (play, tempo, beat) and the mixer.
// Fader positions are 0..1 with unity gain at 0.8 (gain = (fader / 0.8)^2).
// The master meter is Host.outputLevel (the host reads the master peak).
class RackController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Rack)
    QML_SINGLETON

    Q_PROPERTY(int deviceCount READ deviceCount CONSTANT)
    Q_PROPERTY(QStringList deviceNames READ deviceNames CONSTANT)
    Q_PROPERTY(QStringList panelModules READ panelModules CONSTANT)
    Q_PROPERTY(QStringList panelComponents READ panelComponents CONSTANT)
    Q_PROPERTY(int focus READ focus WRITE setFocus NOTIFY focusChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(qreal bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    Q_PROPERTY(bool beat READ beat NOTIFY beatChanged)
    Q_PROPERTY(int mixerRevision READ mixerRevision NOTIFY mixerChanged)
    Q_PROPERTY(QVariantList meters READ meters NOTIFY metersChanged) // per device, 0..1 (-60..0 dB)

public:
    static constexpr qreal kMinBpm = 40.0;
    static constexpr qreal kMaxBpm = 240.0;
    static constexpr qreal kUnityFader = 0.8;

    RackController(krate::Rack &rack, krate::Transport &transport, const QList<krate::InstrumentEntry> &entries,
                   QObject *parent);
    ~RackController() override;

    static RackController *create(QQmlEngine *, QJSEngine *);

    int deviceCount() const { return int(m_names.size()); }
    QStringList deviceNames() const { return m_names; }
    QStringList panelModules() const { return m_modules; }
    QStringList panelComponents() const { return m_components; }

    int focus() const { return m_focus; }
    void setFocus(int device);
    bool playing() const;
    void setPlaying(bool playing);
    qreal bpm() const;
    void setBpm(qreal bpm);
    bool beat() const { return m_beat; }
    int mixerRevision() const { return m_mixerRevision; }
    QVariantList meters() const { return m_meters; }

    // Mixer (device -1 = master for fader and its text).
    Q_INVOKABLE qreal fader(int device) const;
    Q_INVOKABLE void setFader(int device, qreal position);
    Q_INVOKABLE QString faderText(qreal position) const;
    Q_INVOKABLE qreal pan(int device) const;
    Q_INVOKABLE void setPan(int device, qreal pan);
    Q_INVOKABLE bool mute(int device) const;
    Q_INVOKABLE void setMute(int device, bool mute);
    Q_INVOKABLE bool solo(int device) const;
    Q_INVOKABLE void setSolo(int device, bool solo);
    // MIDI channel routing: 1..16 = notes on that channel always go to this
    // device; 0 = none of its own (it gets keys when it has focus). Giving a
    // device a channel takes the channel from any other device.
    Q_INVOKABLE int midiChannel(int device) const;
    Q_INVOKABLE void setMidiChannel(int device, int channel);

signals:
    void focusChanged();
    void playingChanged();
    void bpmChanged();
    void beatChanged();
    void mixerChanged();
    void metersChanged();

private:
    struct Strip
    {
        qreal fader = kUnityFader;
        qreal pan = 0.0;
        bool mute = false;
        bool solo = false;
    };

    static qreal gainFor(qreal fader) { return (fader / kUnityFader) * (fader / kUnityFader); }
    void poll();

    static inline RackController *s_instance = nullptr;

    krate::Rack &m_rack;
    krate::Transport &m_transport;
    QStringList m_names, m_modules, m_components;
    QList<Strip> m_strips;
    qreal m_masterFader = kUnityFader;
    int m_focus = 0;
    bool m_beat = false;
    int m_mixerRevision = 0;
    QVariantList m_meters;
    QTimer m_pollTimer;
};
