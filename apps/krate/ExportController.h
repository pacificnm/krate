#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class ArrangeController;
class AudioTracksController;
class RackController;
class SequencerController;
namespace krate {
class AutomationPlayer;
class JackProcessor;
class Rack;
class Transport;
namespace seq {
class Sequencer;
}
}

// QML singleton "Export": bounce and export (docs/DAW.md, D6). The song as
// the arranger has it (from its start to its last clip, or the loop range)
// rendered offline, faster than real time, to a 24-bit WAV in ~/Music/Krate,
// through the very instruments, mixer and automation playing live; with
// stems, each track too (soloed: through its strip, EQ and effects, but not
// the master stage's soft clip, so they add up to the mix before it), a file
// each. The metronome is left out unless asked for.
//
// While it runs the audio host hands the rack over (live sound stops) and a
// worker thread runs it; then everything is put back: mutes, solos, SONG,
// the transport's start and loop, REC and AUTO.
class ExportController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Export)
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged) // 0..1
    Q_PROPERTY(QString message READ message NOTIFY changed)         // the last export's outcome
    Q_PROPERTY(QStringList files READ files NOTIFY changed)         // what it wrote
    Q_PROPERTY(QString folder READ folder CONSTANT)

public:
    struct Host
    {
        std::function<void()> suspend, resume; // the audio host lets go of the rack, and takes it back
        std::function<double()> sampleRate;     // 0: none (48 kHz then)
    };

    ExportController(krate::Rack &rack, krate::JackProcessor &processor, krate::Transport &transport,
                     krate::seq::Sequencer &sequencer, krate::AutomationPlayer *automation, RackController &rackController,
                     SequencerController &sequencerController, ArrangeController &arranger, AudioTracksController *tracks,
                     QString folder, QObject *parent = nullptr);
    ~ExportController() override;
    static ExportController *create(QQmlEngine *, QJSEngine *);
    void setHost(Host host) { m_host = std::move(host); }

    bool busy() const { return m_busy; }
    qreal progress() const { return m_shownProgress; }
    QString message() const { return m_message; }
    QStringList files() const { return m_files; }
    QString folder() const { return m_folder; }

    // Starts an export: the whole song or the loop range; with stems; with
    // the metronome. false (and a message) if there's nothing to export.
    Q_INVOKABLE bool start(bool loopRange, bool stems, bool metronome);
    Q_INVOKABLE void cancel() { m_cancel.store(true); }
    Q_INVOKABLE void openFolder() const;
    Q_INVOKABLE qreal songSeconds() const; // the whole song's length, for the screen

signals:
    void changed();
    void progressChanged();

private:
    struct Job
    {
        QString path;
        int solo = -1; // the device soloed (a stem), -1: the mix
    };
    // The worker thread; `solos`: the mixer's, as they were (the mix keeps them).
    void run(std::vector<Job> jobs, std::vector<bool> solos, double fromBeat, double toBeat, double rate);
    void poll();
    void finish();

    static inline ExportController *s_instance = nullptr;
    krate::Rack &m_rack;
    krate::JackProcessor &m_processor;
    krate::Transport &m_transport;
    krate::seq::Sequencer &m_sequencer;
    krate::AutomationPlayer *m_automation;
    RackController &m_rackController;
    SequencerController &m_sequencerController;
    ArrangeController &m_arranger;
    AudioTracksController *m_tracks;
    QString m_folder;
    Host m_host;

    bool m_busy = false;
    QString m_message;
    QStringList m_files;
    qreal m_shownProgress = 0.0;
    // Put back afterwards.
    bool m_wasRecording = false, m_wasAutoWrite = false;
    int m_klick = -1;

    std::thread m_worker;
    std::atomic<double> m_progress{0.0};
    std::atomic<bool> m_cancel{false}, m_done{false};
    std::mutex m_mutex; // the worker's results
    QStringList m_written;
    QString m_failed;
    QTimer m_pollTimer;
};
