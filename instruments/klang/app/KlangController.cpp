#include "klang/app/KlangController.h"

#include <QJSEngine>

#include "app/RunControl.h"
#include "core/Transport.h"
#include "klang/dsp/Engine.h"
#include "klang/dsp/Params.h"

KlangController::KlangController(klang::Engine &engine, const QString &panelFilePath, krate::Transport *transport,
                                 QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
    , m_transport(transport)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &KlangController::poll);
    m_pollTimer.start();
    syncTempo();
    connect(this, &ParamController::paramsChanged, this, &KlangController::syncTempo);
}

KlangController::~KlangController()
{
    s_instance = nullptr;
}

KlangController *KlangController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int KlangController::editedPad() const
{
    return m_engine.editedPad();
}

void KlangController::setEditedPad(int pad)
{
    if (pad < 0 || pad >= klang::Engine::kPads || pad == editedPad())
        return;
    m_engine.setEditedPad(pad); // reports the pad parameters changed: the panel follows
    emit editedPadChanged();
}

void KlangController::pressPad(int pad)
{
    if (pad < 0 || pad >= klang::Engine::kPads)
        return;
    setEditedPad(pad);
    m_engine.postHit(pad, 110);
}

void KlangController::releasePad(int pad)
{
    if (pad >= 0 && pad < klang::Engine::kPads)
        m_engine.postRelease(pad);
}

bool KlangController::sounding(int pad) const
{
    return (m_lightsSeen >> pad) & 1u;
}

bool KlangController::playing() const
{
    if (m_transport)
        return m_transport->playing();
    return m_run && m_run->running(m_runId);
}

void KlangController::setPlaying(bool playing)
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

void KlangController::setRunControl(krate::RunControl *run, const QString &id)
{
    m_run = run;
    m_runId = id;
    if (!m_run)
        return;
    m_run->addRunner(id);
    connect(m_run, &krate::RunControl::changed, this, &KlangController::playingChanged);
}

void KlangController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(klang::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}

void KlangController::poll()
{
    std::uint32_t lights = 0;
    for (int pad = 0; pad < klang::Engine::kPads; ++pad)
        if (m_engine.padSounding(pad))
            lights |= 1u << pad;
    if (lights != m_lightsSeen) {
        m_lightsSeen = lights;
        ++m_lights;
        emit lightsChanged();
    }
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}
