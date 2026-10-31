#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"
#include "kessel/app/PatternFile.h"
#include "kessel/dsp/Sequencer.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class RunControl;
class Transport;
}
namespace kessel {
class Engine;
struct Command;
}

// QML singleton "Drums": Kessel's knobs (krate::ParamController by key), the
// pattern bank A..P (edited live, saved to patterns.json 2 s after an edit and
// on destruction), plus START/STOP and Tempo. Standalone, those drive the
// transport it is given; in the Krate rack it is given none, because the
// rack's transport bar owns them.
class DrumsController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Drums)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // False in the rack: the panel hides START/STOP and Tempo then.
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // RUN / STOP works: its own transport (standalone) or the rack's run control.
    Q_PROPERTY(bool canRun READ canRun NOTIFY playingChanged)
    // Playing step 0..15, or -1 when stopped. Display only: polled at 60 Hz.
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    // The track the step buttons edit: 0 = bass drum .. 14 = boom.
    Q_PROPERTY(int selectedTrack READ selectedTrack WRITE setSelectedTrack NOTIFY selectedTrackChanged)

    // The pattern shown and edited, 0..15 = A..P. While playing, a new choice
    // starts at the end of the current pass: playingPattern says which one is
    // sounding, queuedPattern which is waiting (-1 = none).
    Q_PROPERTY(int patternIndex READ patternIndex WRITE setPatternIndex NOTIFY patternChanged)
    Q_PROPERTY(int playingPattern READ playingPattern NOTIFY playingPatternChanged)
    Q_PROPERTY(int queuedPattern READ queuedPattern NOTIFY playingPatternChanged)
    // Bumped on every pattern edit; QML binds steps like
    //     Drums.patternRevision >= 0 ? Drums.stepState(track, step) : 0
    Q_PROPERTY(int patternRevision READ patternRevision NOTIFY patternChanged)
    Q_PROPERTY(int length READ length WRITE setLength NOTIFY patternChanged)              // 1..16
    Q_PROPERTY(int scaleIndex READ scaleIndex WRITE setScaleIndex NOTIFY patternChanged)  // see scaleNames
    Q_PROPERTY(int orderIndex READ orderIndex WRITE setOrderIndex NOTIFY patternChanged)  // see orderNames
    Q_PROPERTY(QStringList scaleNames READ scaleNames CONSTANT)
    Q_PROPERTY(QStringList orderNames READ orderNames CONSTANT)

public:
    // Not default-constructible on purpose (see Korrode's SynthController).
    // Loads the bank and hands the current pattern to the engine, so call it
    // before processing starts.
    DrumsController(kessel::Engine &engine, const QString &panelFilePath, const QString &patternsFilePath,
                    krate::Transport *transport, QObject *parent);
    ~DrumsController() override;

    static DrumsController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool canRun() const { return m_transport != nullptr || m_run != nullptr; }
    // In the rack: RUN / STOP runs just this instrument's sequencer (before QML loads).
    void setRunControl(krate::RunControl *run, const QString &id);
    bool playing() const;
    void setPlaying(bool playing);

    int currentStep() const { return m_currentStep; }
    int selectedTrack() const { return m_selectedTrack; }
    void setSelectedTrack(int track);

    int patternIndex() const { return m_bank.current; }
    void setPatternIndex(int index);
    int playingPattern() const { return m_playingPattern; }
    int queuedPattern() const { return m_queuedPattern; }
    int patternRevision() const { return m_patternRevision; }
    int length() const { return current().length; }
    void setLength(int length);
    int scaleIndex() const;
    void setScaleIndex(int index);
    int orderIndex() const { return current().order; }
    void setOrderIndex(int index);
    QStringList scaleNames() const;
    QStringList orderNames() const;

    const kessel::persistence::PatternBank &bank() const { return m_bank; }

    // Play one voice now (0 = bass drum .. 10 = ride, track order), unaccented.
    Q_INVOKABLE void audition(int track);

    // The current pattern's steps: 0 = off, 1 = on, 2 = accent, 3 = flam.
    Q_INVOKABLE int stepState(int track, int step) const;
    Q_INVOKABLE void setStepState(int track, int step, int state);
    // Step button: off -> on -> accent -> flam -> off, on the selected track.
    Q_INVOKABLE void cycleStep(int step);

    // A step's details (docs/SEQUENCER.md#step-details). Velocity 1..127, or 0:
    // the pattern's (0.8, accented 1.0). Tune and decay locks 0..1, or -1: the knob.
    Q_INVOKABLE int stepVelocity(int track, int step) const;
    Q_INVOKABLE void setStepVelocity(int track, int step, int velocity);
    Q_INVOKABLE qreal stepTune(int track, int step) const;
    Q_INVOKABLE void setStepTune(int track, int step, qreal value);
    Q_INVOKABLE qreal stepDecay(int track, int step) const;
    Q_INVOKABLE void setStepDecay(int track, int step, qreal value);
    Q_INVOKABLE bool stepHasDetails(int track, int step) const;
    Q_INVOKABLE void clearStepDetails(int track, int step);
    // The parameter keys a track's TUNE and DECAY locks stand for ("" if the voice has none).
    Q_INVOKABLE QString tuneKey(int track) const;
    Q_INVOKABLE QString decayKey(int track) const;
    Q_INVOKABLE void clearTrack(int track);
    Q_INVOKABLE void clearPattern();

    // Writes patterns.json now (also done 2 s after an edit, and on destruction).
    void save();

signals:
    void playingChanged();
    void currentStepChanged();
    void selectedTrackChanged();
    void patternChanged();
    void playingPatternChanged();

private:
    kessel::Pattern &current() { return m_bank.patterns[std::size_t(m_bank.current)]; }
    const kessel::Pattern &current() const { return m_bank.patterns[std::size_t(m_bank.current)]; }
    void post(kessel::Command &command); // to the engine, for the current pattern
    void postDetail(int track, int step); // the step's details, to the engine
    void edited();                    // bump the revision and schedule a save
    void syncTempo();                 // the Tempo knob drives the transport, if we have one
    void poll();

    static inline DrumsController *s_instance = nullptr;

    kessel::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    krate::RunControl *m_run = nullptr;
    QString m_runId;
    QString m_patternsFilePath;
    kessel::persistence::PatternBank m_bank; // the GUI's copy; every edit is also posted to the engine
    QTimer m_pollTimer;
    QTimer m_saveTimer;
    int m_currentStep = -1;
    bool m_playing = false;
    int m_selectedTrack = 0;
    int m_playingPattern = 0;
    int m_queuedPattern = -1;
    int m_patternRevision = 0;
};
