#pragma once

#include <QObject>
#include <QString>
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

public slots:
    void connectAudio();
    void disconnectAudio();

signals:
    void audioStatusChanged();
    void outputLevelChanged();

private:
    void poll();
    void connectMidiInput();
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
};

} // namespace krate
