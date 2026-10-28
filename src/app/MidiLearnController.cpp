#include "app/MidiLearnController.h"

#include <QJSEngine>
#include <QLoggingCategory>

#include "dsp/Engine.h"
#include "dsp/Params.h"

Q_LOGGING_CATEGORY(lcMidi, "k303.midi")

namespace {

constexpr int kPollIntervalMs = 16;

QString keyOf(int param)
{
    const auto key = k303::paramInfo(k303::ParamId(param)).key;
    return QString::fromLatin1(key.data(), qsizetype(key.size()));
}

} // namespace

MidiLearnController::MidiLearnController(k303::Engine &engine, const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_filePath(filePath)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    using k303::persistence::LoadResult;
    switch (k303::persistence::loadMidiMap(m_filePath, &m_map)) {
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
        m_engine.setCcMapping(cc, m_map.ccToParam[std::size_t(cc)]);

    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &MidiLearnController::drainEvents);
    m_pollTimer.start();
}

MidiLearnController::~MidiLearnController()
{
    m_engine.setLearnTarget(-1);
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

void MidiLearnController::setArmed(const QString &paramKey)
{
    const auto id = k303::paramFromKey(paramKey.toStdString());
    const QString armed = id ? paramKey : QString();
    m_engine.setLearnTarget(id ? int(*id) : -1);
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
    const auto id = k303::paramFromKey(paramKey.toStdString());
    if (!id)
        return {};
    QStringList ccs;
    for (int cc = 0; cc < 128; ++cc)
        if (m_map.ccToParam[std::size_t(cc)] == int(*id))
            ccs << QString::number(cc);
    return ccs.join(u' ');
}

void MidiLearnController::clear(const QString &paramKey)
{
    const auto id = k303::paramFromKey(paramKey.toStdString());
    if (!id)
        return;
    bool changed = false;
    for (int cc = 0; cc < 128; ++cc) {
        if (m_map.ccToParam[std::size_t(cc)] == int(*id)) {
            m_map.ccToParam[std::size_t(cc)] = -1;
            m_engine.setCcMapping(cc, -1);
            changed = true;
        }
    }
    if (changed)
        mappingsEdited();
}

void MidiLearnController::clearAll()
{
    m_map.ccToParam.fill(-1);
    for (int cc = 0; cc < 128; ++cc)
        m_engine.setCcMapping(cc, -1);
    setArmed({});
    mappingsEdited();
}

void MidiLearnController::drainEvents()
{
    while (const auto ev = m_engine.popEvent()) {
        switch (ev->kind) {
        case k303::EngineEvent::Kind::CcLearned:
            // The engine already bound it; mirror it, save, and disarm.
            m_map.ccToParam[ev->a & 0x7F] = ev->b;
            qCInfo(lcMidi) << "CC" << ev->a << "->" << keyOf(ev->b);
            if (m_armed == keyOf(ev->b)) {
                m_armed.clear();
                emit armedParamChanged();
            }
            mappingsEdited();
            break;
        case k303::EngineEvent::Kind::NoteOn:
            emit notePressed(ev->a, ev->b);
            break;
        }
    }
}

void MidiLearnController::mappingsEdited()
{
    ++m_revision;
    emit mappingsChanged();
    if (!k303::persistence::saveMidiMap(m_filePath, m_map))
        qCWarning(lcMidi) << "could not save MIDI mappings to" << m_filePath;
}
