#include "kobalt/app/KobaltController.h"

#include "app/RunControl.h"

#include <cmath>

#include <QJSEngine>

#include "core/Transport.h"
#include "kobalt/dsp/Engine.h"
#include "kobalt/dsp/Params.h"

KobaltController::KobaltController(kobalt::Engine &engine, const QString &panelFilePath,
                                       krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(16); // ~60 Hz
    connect(&m_pollTimer, &QTimer::timeout, this, &KobaltController::poll);
    m_pollTimer.start();

    syncTempo(); // the saved Tempo knob position
    syncArp();   // a saved Arpeggio on: ready to play as soon as keys are held
    connect(this, &ParamController::paramsChanged, this, &KobaltController::syncTempo);
    connect(this, &ParamController::paramsChanged, this, &KobaltController::syncArp);
}

KobaltController::~KobaltController()
{
    s_instance = nullptr;
}

KobaltController *KobaltController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool KobaltController::playing() const
{
    if (m_transport)
        return m_transport->playing();
    return m_run && m_run->running(m_runId);
}

void KobaltController::setPlaying(bool playing)
{
    if (!m_transport && m_run) {
        m_run->setRunning(m_runId, playing);
        return;
    }
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
    m_playing = playing;
    emit playingChanged();
}

void KobaltController::noteOn(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postNote(note, 100);
}

void KobaltController::noteOff(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postNote(note, 0);
}

bool KobaltController::noteHeld(int note) const
{
    return m_engine.noteHeld(note);
}

void KobaltController::poll()
{
    std::uint64_t held[2] = {0, 0};
    for (int n = 0; n < 128; ++n)
        if (m_engine.noteHeld(n))
            held[n / 64] |= std::uint64_t(1) << (n % 64);
    if (held[0] != m_heldSeen[0] || held[1] != m_heldSeen[1]) {
        m_heldSeen[0] = held[0];
        m_heldSeen[1] = held[1];
        ++m_notesRevision;
        emit notesChanged();
    }

    // The transport can also be started by a MIDI button or Space.
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void KobaltController::syncArp()
{
    // Play Mode Arpeggio (the middle position).
    const bool on = std::abs(value(QStringLiteral("play_mode")) - 0.5) < 0.01;
    if (on == m_arpOn)
        return;
    m_arpOn = on;
    if (!m_transport)
        return; // in the rack, the rack's transport bar runs the arpeggio
    if (on && !m_transport->playing()) {
        setPlaying(true);
        m_arpStartedTransport = true;
    } else if (!on && m_arpStartedTransport) {
        setPlaying(false);
        m_arpStartedTransport = false;
    }
}

void KobaltController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(kobalt::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}

void KobaltController::setRunControl(krate::RunControl *run, const QString &id)
{
    m_run = run;
    m_runId = id;
    if (!m_run)
        return;
    m_run->addRunner(id);
    connect(m_run, &krate::RunControl::changed, this, &KobaltController::playingChanged);
}
