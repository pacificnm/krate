#include "kalliope/app/KalliopeController.h"

#include <QJSEngine>

#include "core/Transport.h"
#include "kalliope/dsp/Params.h"

KalliopeController::KalliopeController(krate::IInstrument &instrument, const QString &panelFilePath,
                                       krate::Transport *transport, QObject *parent)
    : krate::ParamController(instrument, panelFilePath, parent)
    , m_transport(transport)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pollTimer.setInterval(16); // ~60 Hz
    connect(&m_pollTimer, &QTimer::timeout, this, &KalliopeController::poll);
    m_pollTimer.start();

    syncTempo(); // the saved Tempo knob position
    connect(this, &ParamController::paramsChanged, this, &KalliopeController::syncTempo);
}

KalliopeController::~KalliopeController()
{
    s_instance = nullptr;
}

KalliopeController *KalliopeController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool KalliopeController::playing() const
{
    return m_transport && m_transport->playing();
}

void KalliopeController::setPlaying(bool playing)
{
    if (!m_transport || playing == m_transport->playing())
        return;
    m_transport->setPlaying(playing);
    m_playing = playing;
    emit playingChanged();
}

void KalliopeController::poll()
{
    // The transport can also be started by a MIDI button or Space.
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void KalliopeController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(kalliope::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
