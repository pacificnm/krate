#include "app/InstrumentMidiLearn.h"

#include <QLoggingCategory>

#include "core/Instrument.h"
#include "core/Midi.h"

Q_LOGGING_CATEGORY(lcMidi, "krate.midi")

namespace krate {

InstrumentMidiLearn::InstrumentMidiLearn(IInstrument &instrument, const QString &filePath, QObject *parent,
                                         const QStringList &actions)
    : MidiLearnController(parent)
    , m_learnMap(*instrument.midiLearnMap())
    , m_params(instrument.parameters())
    , m_actions(actions)
    , m_filePath(filePath)
{
    m_learnMap.setActionCount(int(m_actions.size()));

    using files::LoadResult;
    switch (files::loadMidiMap(m_filePath, m_params, &m_map, m_actions)) {
    case LoadResult::Loaded:
        qCInfo(lcMidi) << "loaded MIDI mappings from" << m_filePath;
        break;
    case LoadResult::Missing:
        break;
    case LoadResult::Invalid:
        qCWarning(lcMidi) << "could not read" << m_filePath << "- starting with no mappings";
        break;
    case LoadResult::NewerVersion:
        qCWarning(lcMidi) << m_filePath << "was written by a newer version; saving to .v1 instead";
        m_filePath += QStringLiteral(".v1");
        break;
    }
    for (int cc = 0; cc < 128; ++cc)
        m_learnMap.setMapping(cc, m_map.ccToParam[std::size_t(cc)]);
}

InstrumentMidiLearn::~InstrumentMidiLearn()
{
    m_learnMap.setLearnTarget(MidiLearnMap::kUnmapped);
}

int InstrumentMidiLearn::indexOf(const QString &key) const
{
    for (std::size_t i = 0; i < m_params.size(); ++i)
        if (key == QLatin1StringView(m_params[i].key.data(), qsizetype(m_params[i].key.size())))
            return int(i);
    if (const qsizetype action = m_actions.indexOf(key); action >= 0)
        return MidiLearnMap::actionTarget(int(action));
    return MidiLearnMap::kUnmapped;
}

QString InstrumentMidiLearn::keyOf(int target) const
{
    if (MidiLearnMap::isAction(target)) {
        const int action = MidiLearnMap::actionIndex(target);
        return action < m_actions.size() ? m_actions.at(action) : QString();
    }
    if (target < 0 || target >= int(m_params.size()))
        return {};
    const auto key = m_params[std::size_t(target)].key;
    return QString::fromLatin1(key.data(), qsizetype(key.size()));
}

bool InstrumentMidiLearn::armTarget(const QString &key)
{
    const int target = key.isEmpty() ? MidiLearnMap::kUnmapped : indexOf(key);
    m_learnMap.setLearnTarget(target);
    return key.isEmpty() || target != MidiLearnMap::kUnmapped;
}

QString InstrumentMidiLearn::mappingFor(const QString &paramKey) const
{
    const int target = indexOf(paramKey);
    if (target == MidiLearnMap::kUnmapped)
        return {};
    QStringList ccs;
    for (int cc = 0; cc < 128; ++cc)
        if (m_map.ccToParam[std::size_t(cc)] == target)
            ccs << QString::number(cc);
    return ccs.join(u' ');
}

void InstrumentMidiLearn::clear(const QString &paramKey)
{
    const int target = indexOf(paramKey);
    if (target == MidiLearnMap::kUnmapped)
        return;
    bool changed = false;
    for (int cc = 0; cc < 128; ++cc) {
        if (m_map.ccToParam[std::size_t(cc)] == target) {
            m_map.ccToParam[std::size_t(cc)] = -1;
            m_learnMap.setMapping(cc, MidiLearnMap::kUnmapped);
            changed = true;
        }
    }
    if (changed) {
        save();
        mappingsEdited();
    }
}

void InstrumentMidiLearn::clearAll()
{
    m_map.ccToParam.fill(-1);
    m_learnMap.clearAll();
    arm(armedParam()); // disarm
    save();
    mappingsEdited();
}

void InstrumentMidiLearn::drainEvents()
{
    while (const auto bound = m_learnMap.popLearned()) {
        // The audio thread already bound it; mirror it, save, and disarm.
        const QString key = keyOf(bound->target);
        m_map.ccToParam[bound->cc & 0x7F] = bound->target;
        qCInfo(lcMidi) << "CC" << bound->cc << "->" << key;
        save();
        learned(key);
    }
    while (const auto action = m_learnMap.popAction())
        if (*action >= 0 && *action < m_actions.size())
            emit actionTriggered(m_actions.at(*action));
}

void InstrumentMidiLearn::save()
{
    if (!files::saveMidiMap(m_filePath, m_params, m_map, m_actions))
        qCWarning(lcMidi) << "could not save MIDI mappings to" << m_filePath;
}

} // namespace krate
