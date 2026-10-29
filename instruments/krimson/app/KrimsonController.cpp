#include "krimson/app/KrimsonController.h"

#include <QJSEngine>

#include "core/Transport.h"
#include "krimson/dsp/Engine.h"
#include "krimson/dsp/Params.h"

KrimsonController::KrimsonController(krimson::Engine &engine, const QString &panelFilePath,
                                     krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &KrimsonController::poll);
    m_pollTimer.start();

    syncTempo();
    connect(this, &ParamController::paramsChanged, this, &KrimsonController::syncTempo);
}

KrimsonController::~KrimsonController()
{
    s_instance = nullptr;
}

KrimsonController *KrimsonController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool KrimsonController::playing() const
{
    return m_transport && m_transport->playing();
}

void KrimsonController::setPlaying(bool playing)
{
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
    m_playing = playing;
    emit playingChanged();
}

int KrimsonController::editedSlot() const
{
    return m_engine.editedSlot();
}

void KrimsonController::setEditedSlot(int slot)
{
    if (slot == m_engine.editedSlot() || slot < 0 || slot >= krimson::SlotCount)
        return;
    m_engine.setEditedSlot(slot); // the panel follows through the changed bits
    emit editedSlotChanged();
}

void KrimsonController::poll()
{
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void KrimsonController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(krimson::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
