#include "app/MidiLearnController.h"

#include <QJSEngine>

namespace {
constexpr int kPollIntervalMs = 16;
} // namespace

MidiLearnController::MidiLearnController(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &MidiLearnController::drainEvents);
    m_pollTimer.start();
}

MidiLearnController::~MidiLearnController()
{
    s_instance = nullptr;
}

MidiLearnController *MidiLearnController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void MidiLearnController::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    if (!active && !m_armed.isEmpty()) {
        armTarget({});
        m_armed.clear();
        emit armedParamChanged();
    }
    emit activeChanged();
}

void MidiLearnController::setFollowFocus(bool on)
{
    if (on == m_followFocus || !canFollowFocus())
        return;
    m_followFocus = on;
    emit followFocusChanged();
}

void MidiLearnController::arm(const QString &key)
{
    if (!m_active)
        return;
    QString next = key == m_armed ? QString() : key;
    if (!armTarget(next))
        next.clear();
    if (next == m_armed)
        return;
    m_armed = next;
    emit armedParamChanged();
}

QString MidiLearnController::badgeFor(const QString &key) const
{
    const QString sources = mappingFor(key);
    return sources.isEmpty() ? QString() : QStringLiteral("CC ") + sources;
}

void MidiLearnController::learned(const QString &key)
{
    if (!key.isEmpty() && m_armed == key) {
        m_armed.clear();
        emit armedParamChanged();
    }
    mappingsEdited();
}

void MidiLearnController::mappingsEdited()
{
    ++m_revision;
    emit mappingsChanged();
}
