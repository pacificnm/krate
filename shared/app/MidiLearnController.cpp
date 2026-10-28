#include "app/MidiLearnController.h"

#include <QJSEngine>
#include <QLoggingCategory>

#include "core/Instrument.h"
#include "core/Midi.h"

Q_LOGGING_CATEGORY(lcMidi, "k303.midi")

namespace {

constexpr int kPollIntervalMs = 16;

} // namespace

MidiLearnController::MidiLearnController(krate::IInstrument &instrument, const QString &filePath, QObject *parent,
                                         const QStringList &actions)
    : QObject(parent)
    , m_learnMap(*instrument.midiLearnMap())
    , m_params(instrument.parameters())
    , m_actions(actions)
    , m_filePath(filePath)
{
    m_learnMap.setActionCount(int(m_actions.size()));
    Q_ASSERT(!s_instance);
    s_instance = this;

    using krate::files::LoadResult;
    switch (krate::files::loadMidiMap(m_filePath, m_params, &m_map, m_actions)) {
    case LoadResult::Loaded:
        qCInfo(lcMidi) << "loaded MIDI mappings from" << m_filePath;
        break;
    case LoadResult::Missing:
        break;
    case LoadResult::Invalid:
        qCWarning(lcMidi) << "could not read" << m_filePath << "- starting with no mappings";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcMidi) << m_filePath << "was written by a newer k303; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    for (int cc = 0; cc < 128; ++cc)
        m_learnMap.setMapping(cc, m_map.ccToParam[std::size_t(cc)]);

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &MidiLearnController::drainEvents);
    m_pollTimer.start();
}

MidiLearnController::~MidiLearnController()
{
    m_learnMap.setLearnTarget(krate::MidiLearnMap::kUnmapped);
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
    if (!active)
        setArmed({});
    emit activeChanged();
}

int MidiLearnController::indexOf(const QString &key) const
{
    for (std::size_t i = 0; i < m_params.size(); ++i)
        if (key == QLatin1StringView(m_params[i].key.data(), qsizetype(m_params[i].key.size())))
            return int(i);
    if (const qsizetype action = m_actions.indexOf(key); action >= 0)
        return krate::MidiLearnMap::actionTarget(int(action));
    return krate::MidiLearnMap::kUnmapped;
}

QString MidiLearnController::keyOf(int target) const
{
    if (krate::MidiLearnMap::isAction(target)) {
        const int action = krate::MidiLearnMap::actionIndex(target);
        return action < m_actions.size() ? m_actions.at(action) : QString();
    }
    if (target < 0 || target >= int(m_params.size()))
        return {};
    const auto key = m_params[std::size_t(target)].key;
    return QString::fromLatin1(key.data(), qsizetype(key.size()));
}

void MidiLearnController::setArmed(const QString &paramKey)
{
    const int target = indexOf(paramKey);
    const bool known = target != krate::MidiLearnMap::kUnmapped;
    const QString armed = known ? paramKey : QString();
    m_learnMap.setLearnTarget(target);
    if (armed == m_armed)
        return;
    m_armed = armed;
    emit armedParamChanged();
}

void MidiLearnController::arm(const QString &paramKey)
{
    if (!m_active)
        return;
    setArmed(paramKey == m_armed ? QString() : paramKey);
}

QString MidiLearnController::mappingFor(const QString &paramKey) const
{
    const int target = indexOf(paramKey);
    if (target == krate::MidiLearnMap::kUnmapped)
        return {};
    QStringList ccs;
    for (int cc = 0; cc < 128; ++cc)
        if (m_map.ccToParam[std::size_t(cc)] == target)
            ccs << QString::number(cc);
    return ccs.join(u' ');
}

void MidiLearnController::clear(const QString &paramKey)
{
    const int target = indexOf(paramKey);
    if (target == krate::MidiLearnMap::kUnmapped)
        return;
    bool changed = false;
    for (int cc = 0; cc < 128; ++cc) {
        if (m_map.ccToParam[std::size_t(cc)] == target) {
            m_map.ccToParam[std::size_t(cc)] = -1;
            m_learnMap.setMapping(cc, krate::MidiLearnMap::kUnmapped);
            changed = true;
        }
    }
    if (changed)
        mappingsEdited();
}

void MidiLearnController::clearAll()
{
    m_map.ccToParam.fill(-1);
    m_learnMap.clearAll();
    setArmed({});
    mappingsEdited();
}

void MidiLearnController::drainEvents()
{
    while (const auto learned = m_learnMap.popLearned()) {
        // The audio thread already bound it; mirror it, save, and disarm.
        const QString key = keyOf(learned->target);
        m_map.ccToParam[learned->cc & 0x7F] = learned->target;
        qCInfo(lcMidi) << "CC" << learned->cc << "->" << key;
        if (m_armed == key) {
            m_armed.clear();
            emit armedParamChanged();
        }
        mappingsEdited();
    }
    while (const auto action = m_learnMap.popAction())
        if (*action >= 0 && *action < m_actions.size())
            emit actionTriggered(m_actions.at(*action));
}

void MidiLearnController::mappingsEdited()
{
    ++m_revision;
    emit mappingsChanged();
    if (!krate::files::saveMidiMap(m_filePath, m_params, m_map, m_actions))
        qCWarning(lcMidi) << "could not save MIDI mappings to" << m_filePath;
}
