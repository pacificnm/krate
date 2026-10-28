#pragma once

#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "app/ParamController.h"
#include "dsp/Sequencer.h"

class QQmlEngine;
class QJSEngine;

namespace krate {
class Transport;
}
namespace k909 {
class Engine;
}

// QML singleton "Drums": K909's knobs (krate::ParamController by key), the
// sequencer's current pattern (steps by track, edited live), plus START/STOP and
// Tempo. Standalone, those drive the transport it is given; in the Krate rack
// it is given none, because the rack's transport bar owns them.
class DrumsController : public krate::ParamController
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Drums)
    QML_SINGLETON

    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // False in the rack: the panel hides START/STOP and Tempo then.
    Q_PROPERTY(bool ownsTransport READ ownsTransport CONSTANT)
    // Playing step 0..15, or -1 when stopped. Display only: polled at 60 Hz.
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    // The track the step buttons edit: 0 = bass drum .. 10 = ride.
    Q_PROPERTY(int selectedTrack READ selectedTrack WRITE setSelectedTrack NOTIFY selectedTrackChanged)
    // Bumped on every pattern edit; QML binds steps like
    //     Drums.patternRevision >= 0 ? Drums.stepState(track, step) : 0
    Q_PROPERTY(int patternRevision READ patternRevision NOTIFY patternChanged)
    Q_PROPERTY(int length READ length NOTIFY patternChanged)

public:
    // Not default-constructible on purpose (see k303's SynthController).
    DrumsController(k909::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                    QObject *parent);
    ~DrumsController() override;

    static DrumsController *create(QQmlEngine *, QJSEngine *);

    bool ownsTransport() const { return m_transport != nullptr; }
    bool playing() const;
    void setPlaying(bool playing);

    int currentStep() const { return m_currentStep; }
    int selectedTrack() const { return m_selectedTrack; }
    void setSelectedTrack(int track);
    int patternRevision() const { return m_patternRevision; }
    int length() const { return m_pattern.length; }
    const k909::Pattern &pattern() const { return m_pattern; }

    // Play one voice now (0 = bass drum .. 10 = ride, track order), unaccented.
    Q_INVOKABLE void audition(int track);

    // 0 = off, 1 = on, 2 = accent.
    Q_INVOKABLE int stepState(int track, int step) const;
    Q_INVOKABLE void setStepState(int track, int step, int state);
    // Step button: off -> on -> accent -> off, on the selected track.
    Q_INVOKABLE void cycleStep(int step);
    Q_INVOKABLE void clearTrack(int track);
    Q_INVOKABLE void clearPattern();

signals:
    void playingChanged();
    void currentStepChanged();
    void selectedTrackChanged();
    void patternChanged();

private:
    void syncTempo(); // the Tempo knob drives the transport, if we have one
    void poll();

    static inline DrumsController *s_instance = nullptr;

    k909::Engine &m_engine;
    krate::Transport *m_transport = nullptr;
    k909::Pattern m_pattern;  // the GUI's copy; every edit is also posted to the engine
    QTimer m_pollTimer;
    int m_currentStep = -1;
    bool m_playing = false;
    int m_selectedTrack = 0;
    int m_patternRevision = 0;
};
