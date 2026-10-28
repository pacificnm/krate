#include "kalliope/app/KalliopeController.h"

#include <QJSEngine>

#include "core/Transport.h"
#include "kalliope/dsp/Engine.h"
#include "kalliope/dsp/Params.h"

KalliopeController::KalliopeController(kalliope::Engine &engine, const QString &panelFilePath,
                                       krate::Transport *transport, QObject *parent)
    : krate::ParamController(engine, panelFilePath, parent)
    , m_engine(engine)
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

void KalliopeController::noteOn(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postNote(note, 100);
}

void KalliopeController::noteOff(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postNote(note, 0);
}

bool KalliopeController::noteHeld(int note) const
{
    return m_engine.noteHeld(note);
}

void KalliopeController::poll()
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

void KalliopeController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(kalliope::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
