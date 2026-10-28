#include "app/DrumsController.h"

#include <QJSEngine>

#include "core/Transport.h"
#include "dsp/Engine.h"
#include "dsp/Params.h"

DrumsController::DrumsController(k909::Engine &engine, const QString &panelFilePath,
                                 krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

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
    emit playingChanged();
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
