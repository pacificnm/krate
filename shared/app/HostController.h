#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace krate {
class AudioHost;
}
class QQmlEngine;
class QJSEngine;

namespace krate {

// QML singleton "Host": the GUI's view of the audio host, whichever it is (a
// standalone JackHost or the rack's host). Opens the client, retries every 2 s
// while offline, auto-connects outputs and the MIDI controller, and publishes
// the status line and master level meter. Instruments' own controllers know
// nothing about the audio host.
class HostController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Host)
    QML_SINGLETON

    Q_PROPERTY(bool audioConnected READ audioConnected NOTIFY audioStatusChanged)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY audioStatusChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY audioStatusChanged)
    Q_PROPERTY(int bufferSize READ bufferSize NOTIFY audioStatusChanged)
    Q_PROPERTY(int xrunCount READ xrunCount NOTIFY audioStatusChanged)
    Q_PROPERTY(bool midiConnected READ midiConnected NOTIFY audioStatusChanged)
    Q_PROPERTY(qreal outputLevel READ outputLevel NOTIFY outputLevelChanged) // 0..1, -60..0 dB

    // Audio inputs (in_1, in_2: a guitar through the audio interface).
    Q_PROPERTY(int inputCount READ inputCount CONSTANT)
    // The interface's capture ports, to choose from (refreshed as devices come and go).
    Q_PROPERTY(QStringList capturePorts READ capturePorts NOTIFY capturePortsChanged)
    // The capture port chosen for each input ("" = none); kept and reconnected when it reappears.
    Q_PROPERTY(QStringList inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(QVariantList inputsConnected READ inputsConnected NOTIFY audioStatusChanged)
    Q_PROPERTY(QVariantList inputLevels READ inputLevels NOTIFY inputLevelsChanged) // 0..1 each, -60..0 dB
    // Frames per block asked of the server (0 = the server's own); smaller is less delay.
    Q_PROPERTY(int requestedBufferSize READ requestedBufferSize WRITE setRequestedBufferSize NOTIFY requestedBufferSizeChanged)
    // Where the rack's master goes: a playback device ("" = the first hardware outputs).
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY capturePortsChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY outputDeviceChanged)
    // From an input to the speakers, as the server reports it.
    Q_PROPERTY(qreal latencyMs READ latencyMs NOTIFY audioStatusChanged)

public:
    // Not default-constructible on purpose: see SynthController. Opens the
    // audio client right away (and keeps retrying while offline).
    HostController(AudioHost &host, const QString &clientName, QObject *parent);
    ~HostController() override;

    static HostController *create(QQmlEngine *, QJSEngine *);

    bool audioConnected() const;
    QString audioStatus() const { return m_audioStatus; }
    int sampleRate() const;
    int bufferSize() const;
    int xrunCount() const;
    bool midiConnected() const { return m_midiConnected; }
    qreal outputLevel() const { return m_outputLevel; }

    // Regular expression (case-insensitive) naming the MIDI controller; every
    // matching source is connected, again whenever ports appear.
    void setMidiInputMatch(const QString &pattern);

    int inputCount() const;
    QStringList capturePorts() const { return m_capturePorts; }
    QStringList inputPorts() const { return m_inputPorts; }
    // Before the first poll (the saved choice) or from the GUI.
    void setInputPorts(const QStringList &ports);
    Q_INVOKABLE void setInputPort(int input, const QString &port);
    // A capture port's name for people, e.g. "USB Audio Interface · in 1".
    Q_INVOKABLE static QString portLabel(const QString &port);
    QVariantList inputsConnected() const;
    QVariantList inputLevels() const;
    int requestedBufferSize() const { return m_requestedBufferSize; }
    void setRequestedBufferSize(int frames);
    qreal latencyMs() const;
    QStringList outputDevices() const { return m_outputDevices; }
    QString outputDevice() const { return m_outputDevice; }
    void setOutputDevice(const QString &device);

public slots:
    void connectAudio();
    void disconnectAudio();

signals:
    void audioStatusChanged();
    void outputLevelChanged();
    void capturePortsChanged();
    void inputPortsChanged();
    void inputLevelsChanged();
    void requestedBufferSizeChanged();
    void outputDeviceChanged();

private:
    void poll();
    void connectMidiInput();
    void connectInputs();
    void refreshCapturePorts();
    void connectOutputs();
    void setAudioStatus(const QString &status);

    static inline HostController *s_instance = nullptr;

    AudioHost &m_host;
    QByteArray m_clientName;
    QTimer m_pollTimer;
    QTimer m_retryTimer;
    QString m_audioStatus;
    qreal m_outputLevel = 0.0;
    QString m_midiMatch;
    bool m_midiConnected = false;
    int m_lastXruns = 0;
    int m_lastSampleRate = 0;
    int m_lastBufferSize = 0;
    QStringList m_capturePorts;
    QStringList m_outputDevices;
    QString m_outputDevice;
    QString m_connectedOutput; // the device the outputs went to ("" = the first hardware outputs)
    QStringList m_inputPorts;
    QList<qreal> m_inputLevels;
    int m_requestedBufferSize = 0;
    std::uint32_t m_lastLatency = 0;
};

} // namespace krate
