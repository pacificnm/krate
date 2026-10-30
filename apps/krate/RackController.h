#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <array>

#include "rack/ChannelEq.h"

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
class RackRunControl;

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
    // The mixer, the song screen or the sequencer instead of the focused instrument's panel
    // (one at a time); a focus change shows the instrument.
    Q_PROPERTY(bool mixerShown READ mixerShown WRITE setMixerShown NOTIFY mixerShownChanged)
    Q_PROPERTY(bool songShown READ songShown WRITE setSongShown NOTIFY songShownChanged)
    Q_PROPERTY(bool seqShown READ seqShown WRITE setSeqShown NOTIFY seqShownChanged)
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
    // The device `step` places on (1 next, -1 previous) that has a panel to show.
    Q_INVOKABLE int stepFocus(int step) const;

    int focus() const { return m_focus; }
    void setFocus(int device);
    bool mixerShown() const { return m_mixerShown; }
    void setMixerShown(bool shown);
    bool songShown() const { return m_songShown; }
    void setSongShown(bool shown);
    bool seqShown() const { return m_seqShown; }
    void setSeqShown(bool shown);
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
    // A strip's EQ (docs/SHARED_FOUNDATIONS.MD#mixer-eq): band as in
    // krate::ChannelEq::Band, knob position 0..1 (gains: 0.5 = 0 dB).
    Q_INVOKABLE qreal eq(int device, int band) const;
    Q_INVOKABLE void setEq(int device, int band, qreal position);
    Q_INVOKABLE void resetEq(int device);
    Q_INVOKABLE QString eqText(int band, qreal position) const;
    Q_INVOKABLE qreal eqDefault(int band) const;
    Q_INVOKABLE bool eqFlat(int device) const;
    // Knob position <-> its value (dB, Hz or Q).
    static double eqValue(int band, qreal position);
    static qreal eqPosition(int band, double value);
    // Effects (docs/SHARED_FOUNDATIONS.MD#mixer-effects): a strip's sends
    // (effect 0 reverb, 1 delay; 0..1), and the returns on the master strip:
    // 0 reverb level, 1 reverb size, 2 delay level, 3 delay time (7 positions:
    // 1/16 .. 1/2), 4 delay feedback; all knob positions 0..1.
    Q_INVOKABLE qreal send(int device, int effect) const;
    Q_INVOKABLE void setSend(int device, int effect, qreal position);
    Q_INVOKABLE QString sendText(qreal position) const;
    Q_INVOKABLE qreal effect(int control) const;
    Q_INVOKABLE void setEffect(int control, qreal position);
    Q_INVOKABLE QString effectText(int control, qreal position) const;
    Q_INVOKABLE qreal effectDefault(int control) const;
    static constexpr int kEffectControls = 5;
    static constexpr int kDelayTimes = 7;
    Q_INVOKABLE bool mute(int device) const;
    Q_INVOKABLE void setMute(int device, bool mute);
    Q_INVOKABLE bool solo(int device) const;
    Q_INVOKABLE void setSolo(int device, bool solo);
    // MIDI channel routing: 1..16 = notes on that channel always go to this
    // device; 0 = none of its own (it gets keys when it has focus). Giving a
    // device a channel takes the channel from any other device.
    Q_INVOKABLE int midiChannel(int device) const;    // the first of its channels, 0 = none
    Q_INVOKABLE int midiChannelCount(int device) const; // how many it has (Krimson: 4)
    Q_INVOKABLE void setMidiChannel(int device, int channel); // just this one (or none)
    Q_INVOKABLE void restoreMidiChannels(int device);  // its registry defaults

    // The instruments' own RUN / STOP (set before the panels load); PLAY switches them all on.
    void setRunControl(RackRunControl *run) { m_run = run; }

    // Mixer moves from learned controller faders, knobs and buttons (polled; public for tests).
    void applyMixerMoves();

    // The whole mixer (every strip by instrument id, and the master), for a
    // song in the library to keep and put back.
    QJsonObject mixerState() const;
    // A strip's EQ knobs as saved ([7 positions]); missing ones are flat.
    QJsonArray eqJson(int device) const;
    void setEqJson(int device, const QJsonArray &values);
    // The effects' returns as saved ([5 positions]); missing: the defaults.
    QJsonArray effectsJson() const;
    void setEffectsJson(const QJsonArray &values);
    void setMixerState(const QJsonObject &state);

signals:
    void focusChanged();
    void mixerShownChanged();
    void songShownChanged();
    void seqShownChanged();
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
        std::array<qreal, krate::ChannelEq::Count> eq = defaultEq();
        std::array<qreal, 2> sends{};
        bool mute = false;
        bool solo = false;
    };

    static std::array<qreal, krate::ChannelEq::Count> defaultEq();
    static qreal gainFor(qreal fader) { return (fader / kUnityFader) * (fader / kUnityFader); }
    void poll();

    static inline RackController *s_instance = nullptr;

    krate::Rack &m_rack;
    krate::Transport &m_transport;
    QList<krate::InstrumentEntry> m_entries; // their default MIDI channels
    QStringList m_names, m_modules, m_components;
    QList<Strip> m_strips;
    qreal m_masterFader = kUnityFader;
    std::array<qreal, kEffectControls> m_effects{0.7, 0.5, 0.7, 0.5, 0.35 / 0.9};
    void applyEffects();
    int m_focus = 0;
    RackRunControl *m_run = nullptr;
    bool m_shownPlaying = false; // the play state last announced
    bool m_mixerShown = false;
    bool m_songShown = false;
    bool m_seqShown = false;
    double m_shownBpm = 0.0; // the tempo last announced (a song's tempo map moves it from the audio thread)
    bool m_beat = false;
    int m_mixerRevision = 0;
    QVariantList m_meters;
    QTimer m_pollTimer;
};
