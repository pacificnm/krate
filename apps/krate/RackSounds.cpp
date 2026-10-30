#include "RackSounds.h"

#include "RackController.h"
#include "app/InstrumentRegistry.h"

RackSounds::RackSounds(RackController &rack, QStringList ids, std::vector<krate::InstrumentInstance *> instances)
    : m_rack(rack)
    , m_ids(std::move(ids))
    , m_instances(std::move(instances))
{
}

RackSounds::Snapshot RackSounds::capture() const
{
    Snapshot s;
    for (std::size_t i = 0; i < m_instances.size() && i < std::size_t(m_ids.size()); ++i)
        if (m_instances[i])
            s.sounds[m_ids[qsizetype(i)]] = m_instances[i]->saveSound();
    s.mixer = m_rack.mixerState();
    return s;
}

void RackSounds::restore(const Snapshot &snapshot) const
{
    for (std::size_t i = 0; i < m_instances.size() && i < std::size_t(m_ids.size()); ++i)
        if (m_instances[i] && snapshot.sounds.contains(m_ids[qsizetype(i)]))
            m_instances[i]->loadSound(snapshot.sounds.value(m_ids[qsizetype(i)]).toObject());
    if (!snapshot.mixer.isEmpty())
        m_rack.setMixerState(snapshot.mixer);
}
