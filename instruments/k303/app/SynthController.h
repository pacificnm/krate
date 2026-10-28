#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "core/Transport.h"
#include "dsp/Engine.h"

class QQmlEngine;
class QJSEngine;

// QML singleton "Synth": the GUI's view of k303's engine. Owns the Engine and
// saves its knob positions; polls the playhead and MIDI-changed knobs at 60 Hz.
// Knows nothing about the audio host (that's krate::HostController, "Host").
// RUN/STOP and the Tempo knob drive the transport it is given; in the Krate rack
// it is given none, because the rack's transport bar owns play and tempo.
class SynthController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Synth)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // False in the rack: the panel hides RUN/STOP and Tempo then.
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // Playing step 0..15, or -1 when stopped. Display only: polled at 60 Hz.
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)

    // Sound parameters, normalized 0..1 (docs/DSP.md#parameters). One notify
    // signal for all: changes are rare and there are only nine.
    Q_PROPERTY(qreal tuning READ tuning WRITE setTuning NOTIFY paramsChanged)
    Q_PROPERTY(qreal cutoff READ cutoff WRITE setCutoff NOTIFY paramsChanged)
    Q_PROPERTY(qreal resonance READ resonance WRITE setResonance NOTIFY paramsChanged)
    Q_PROPERTY(qreal envmod READ envmod WRITE setEnvmod NOTIFY paramsChanged)
    Q_PROPERTY(qreal decay READ decay WRITE setDecay NOTIFY paramsChanged)
    Q_PROPERTY(qreal accent READ accent WRITE setAccent NOTIFY paramsChanged)
    Q_PROPERTY(qreal waveform READ waveform WRITE setWaveform NOTIFY paramsChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY paramsChanged)
    Q_PROPERTY(qreal tempo READ tempo WRITE setTempo NOTIFY paramsChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY paramsChanged)
    Q_PROPERTY(qreal playMode READ playMode WRITE setPlayMode NOTIFY paramsChanged)
    // True while the playing step starts a beat (beat LED). Display only.
    Q_PROPERTY(bool beat READ beat NOTIFY currentStepChanged)

public:
    // Deliberately not default-constructible: Qt prefers a default constructor
    // over create(), which would give QML a second instance.
    // panelFilePath: where knob positions are saved (docs/ARCHITECTURE.md).
    // transport: driven by RUN/STOP and Tempo; nullptr in the rack.
    SynthController(const QString &panelFilePath, krate::Transport *transport, QObject *parent);
    ~SynthController() override;

    // The instance main() created; QML gets this one rather than a new object.
    static SynthController *create(QQmlEngine *, QJSEngine *);

    // C++ only: for the other controllers that send commands to the engine.
    k303::Engine &engine() { return m_engine; }

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const { return m_transport && m_transport->playing(); }
    int currentStep() const { return m_currentStep; }
    void setPlaying(bool playing);

    qreal tuning() const { return param(k303::ParamId::Tuning); }
    qreal cutoff() const { return param(k303::ParamId::Cutoff); }
    qreal resonance() const { return param(k303::ParamId::Resonance); }
    qreal envmod() const { return param(k303::ParamId::EnvMod); }
    qreal decay() const { return param(k303::ParamId::Decay); }
    qreal accent() const { return param(k303::ParamId::Accent); }
    qreal waveform() const { return param(k303::ParamId::Waveform); }
    qreal volume() const { return param(k303::ParamId::Volume); }
    qreal tempo() const { return param(k303::ParamId::Tempo); }
    qreal scale() const { return param(k303::ParamId::Scale); }
    qreal playMode() const { return param(k303::ParamId::PlayMode); }
    bool beat() const { return m_beat; }
    void setTuning(qreal v) { setParam(k303::ParamId::Tuning, v); }
    void setCutoff(qreal v) { setParam(k303::ParamId::Cutoff, v); }
    void setResonance(qreal v) { setParam(k303::ParamId::Resonance, v); }
    void setEnvmod(qreal v) { setParam(k303::ParamId::EnvMod, v); }
    void setDecay(qreal v) { setParam(k303::ParamId::Decay, v); }
    void setAccent(qreal v) { setParam(k303::ParamId::Accent, v); }
    void setWaveform(qreal v) { setParam(k303::ParamId::Waveform, v); }
    void setVolume(qreal v) { setParam(k303::ParamId::Volume, v); }
    void setTempo(qreal v) { setParam(k303::ParamId::Tempo, v); }
    void setScale(qreal v) { setParam(k303::ParamId::Scale, v); }
    void setPlayMode(qreal v) { setParam(k303::ParamId::PlayMode, v); }

    // Key-based access for generic controls (keys are the ids in docs/DSP.md).
    Q_INVOKABLE void setParamByKey(const QString &key, qreal value);
    Q_INVOKABLE qreal defaultValue(const QString &key) const;
    // Human-readable value, e.g. "1.2 kHz"; pass the value so bindings update with it.
    Q_INVOKABLE QString displayText(const QString &key, qreal value) const;

signals:
    void playingChanged();
    void paramsChanged();
    void currentStepChanged();
    // A MIDI key was pressed (for step record, later).
    void notePressed(int note, int velocity);

private:
    qreal param(k303::ParamId id) const { return m_engine.params().get(id); }
    void setParam(k303::ParamId id, qreal value);
    void syncTempo(); // the Tempo knob drives the transport, if we have one
    void poll();
    void savePanel();

    static inline SynthController *s_instance = nullptr;

    mutable k303::Engine m_engine; // mutable: ParamStore reads are const in spirit
    krate::Transport *m_transport = nullptr;

    QTimer m_pollTimer;
    int m_currentStep = -1;
    bool m_beat = false;
    QString m_panelFilePath;
    QTimer m_saveTimer;
};
