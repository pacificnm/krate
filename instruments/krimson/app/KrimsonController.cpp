#include "krimson/app/KrimsonController.h"

#include <algorithm>
#include <cmath>

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
    m_arpOn = value(QStringLiteral("arp_on")) > 0.5;
    connect(this, &ParamController::paramsChanged, this, &KrimsonController::syncTempo);
    connect(this, &ParamController::paramsChanged, this, &KrimsonController::syncArp);
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
    ++m_morphRevision;            // and shows the new slot's morph targets
    emit editedSlotChanged();
    emit morphChanged();
}

void KrimsonController::noteOn(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postKeyboard(0x90, std::uint8_t(note), 100);
}

void KrimsonController::noteOff(int note)
{
    if (note >= 0 && note < 128)
        m_engine.postKeyboard(0x80, std::uint8_t(note), 0);
}

bool KrimsonController::noteHeld(int note) const
{
    return m_engine.noteHeld(note);
}

void KrimsonController::pitchBend(qreal amount)
{
    const int value = std::clamp(int(std::lround(8192.0 + amount * 8191.0)), 0, 16383);
    m_engine.postKeyboard(0xE0, std::uint8_t(value & 0x7F), std::uint8_t(value >> 7));
}

void KrimsonController::modWheel(qreal amount)
{
    m_engine.postKeyboard(0xB0, 1, std::uint8_t(std::clamp(int(std::lround(amount * 127.0)), 0, 127)));
}

namespace {
int paramIndex(const QString &key)
{
    for (int i = 0; i < krimson::ParamCount; ++i) {
        const auto k = krimson::kParams[std::size_t(i)].key;
        if (key == QLatin1StringView(k.data(), qsizetype(k.size())))
            return i;
    }
    return -1;
}
} // namespace

void KrimsonController::setMorphEdit(bool on)
{
    if (on == m_morphEdit)
        return;
    m_morphEdit = on;
    emit morphChanged();
}

bool KrimsonController::morphable(const QString &key) const
{
    const int i = paramIndex(key);
    return i >= 0 && krimson::Engine::morphable(krimson::ParamId(i));
}

qreal KrimsonController::morph(const QString &key) const
{
    const int i = paramIndex(key);
    return morphable(key) ? m_engine.morph(m_engine.editedSlot(), krimson::ParamId(i)) : -1.0;
}

void KrimsonController::setMorph(const QString &key, qreal target)
{
    if (!morphable(key))
        return;
    m_engine.setMorph(m_engine.editedSlot(), krimson::ParamId(paramIndex(key)), float(std::clamp(target, 0.0, 1.0)));
    ++m_morphRevision;
    emit morphChanged();
    emit paramsChanged(); // the slot's sound changed: slots.json saves it
}

void KrimsonController::clearMorph(const QString &key)
{
    if (!morphable(key))
        return;
    m_engine.setMorph(m_engine.editedSlot(), krimson::ParamId(paramIndex(key)), -1.0f);
    ++m_morphRevision;
    emit morphChanged();
    emit paramsChanged();
}

void KrimsonController::morphReloaded()
{
    ++m_morphRevision;
    emit morphChanged();
}

void KrimsonController::clearAllMorph()
{
    m_engine.clearMorph(m_engine.editedSlot());
    ++m_morphRevision;
    emit morphChanged();
    emit paramsChanged();
}

void KrimsonController::poll()
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
    if (playing() != m_playing) {
        m_playing = playing();
        emit playingChanged();
    }
}

void KrimsonController::syncArp()
{
    const bool on = value(QStringLiteral("arp_on")) > 0.5;
    const int slot = m_engine.editedSlot();
    const bool switchedSlot = slot != m_arpSlot; // another slot's switch, not a change
    m_arpSlot = slot;
    if (on == m_arpOn || switchedSlot) {
        m_arpOn = on;
        return;
    }
    m_arpOn = on;
    if (!m_transport)
        return; // in the rack, the rack's transport bar runs the arpeggios
    if (on && !m_transport->playing()) {
        setPlaying(true);
        m_arpStartedTransport = true;
    } else if (!on && m_arpStartedTransport) {
        setPlaying(false);
        m_arpStartedTransport = false;
    }
}

void KrimsonController::syncTempo()
{
    if (m_transport)
        m_transport->setBpm(krimson::units::tempoBpm(float(value(QStringLiteral("tempo")))));
}
