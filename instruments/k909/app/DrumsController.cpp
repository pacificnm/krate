#include "app/DrumsController.h"

#include <QJSEngine>

#include "core/Transport.h"
#include "dsp/Params.h"

DrumsController::DrumsController(krate::IInstrument &instrument, const QString &panelFilePath,
                                 krate::Transport *transport, QObject *parent)
    : krate::ParamController(instrument, panelFilePath, parent)
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

void DrumsController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(k909::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
