#include "app/DrumsController.h"

#include <algorithm>

#include <QJSEngine>

#include "core/Transport.h"
#include "dsp/Engine.h"
#include "dsp/Params.h"

DrumsController::DrumsController(k909::Engine &engine, const QString &panelFilePath,
                                 krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
    , m_pattern(engine.pattern()) // read before processing starts
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(16); // ~60 Hz
    connect(&m_pollTimer, &QTimer::timeout, this, &DrumsController::poll);
    m_pollTimer.start();

    syncTempo(); // the saved Tempo knob position
    connect(this, &ParamController::paramsChanged, this, &DrumsController::syncTempo);
}

DrumsController::~DrumsController()
{
    s_instance = nullptr;
}

DrumsController *DrumsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool DrumsController::playing() const
{
    return m_transport && m_transport->playing();
}

void DrumsController::setPlaying(bool playing)
{
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
    m_playing = playing;
    emit playingChanged();
}

void DrumsController::poll()
{
    const int step = m_engine.currentStep();
    if (step != m_currentStep) {
        m_currentStep = step;
        emit currentStepChanged();
    }
    // The transport can also be started by a MIDI button, Space, or the rack.
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void DrumsController::setSelectedTrack(int track)
{
    track = std::clamp(track, 0, k909::DrumCount - 1);
    if (track == m_selectedTrack)
        return;
    m_selectedTrack = track;
    emit selectedTrackChanged();
}

int DrumsController::stepState(int track, int step) const
{
    if (track < 0 || track >= k909::DrumCount || step < 0 || step >= 16)
        return 0;
    return int(m_pattern.tracks[std::size_t(track)][std::size_t(step)]);
}

void DrumsController::setStepState(int track, int step, int state)
{
    if (track < 0 || track >= k909::DrumCount || step < 0 || step >= 16 || state < 0 || state > 2)
        return;
    auto &slot = m_pattern.tracks[std::size_t(track)][std::size_t(step)];
    if (int(slot) == state)
        return;
    slot = k909::StepState(state);
    k909::Command command;
    command.kind = k909::Command::Kind::SetStep;
    command.drum = std::uint8_t(track);
    command.step = std::uint8_t(step);
    command.state = slot;
    m_engine.post(command);
    ++m_patternRevision;
    emit patternChanged();
}

void DrumsController::cycleStep(int step)
{
    setStepState(m_selectedTrack, step, (stepState(m_selectedTrack, step) + 1) % 3);
}

void DrumsController::clearTrack(int track)
{
    for (int step = 0; step < 16; ++step)
        setStepState(track, step, 0);
}

void DrumsController::clearPattern()
{
    for (int track = 0; track < k909::DrumCount; ++track)
        clearTrack(track);
}

void DrumsController::audition(int track)
{
    if (track < 0 || track >= k909::DrumCount)
        return;
    k909::Command command;
    command.kind = k909::Command::Kind::Trigger;
    command.drum = std::uint8_t(track);
    command.velocity = 0.85f; // a normal (unaccented) sequencer step
    m_engine.post(command);
}

void DrumsController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(k909::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
