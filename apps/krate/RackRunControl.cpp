#include "RackRunControl.h"

#include "core/Instrument.h"
#include "core/Transport.h"
#include "rack/Rack.h"

RackRunControl::RackRunControl(krate::Rack &rack, krate::Transport &transport, QStringList ids, QObject *parent)
    : krate::RunControl(parent)
    , m_rack(rack)
    , m_transport(transport)
    , m_ids(std::move(ids))
{
    m_enabled.fill(true, m_ids.size());
}

bool RackRunControl::running(const QString &id) const
{
    const qsizetype i = m_ids.indexOf(id);
    return i >= 0 && m_transport.playing() && m_enabled[i];
}

void RackRunControl::setRunning(const QString &id, bool running)
{
    const qsizetype i = m_ids.indexOf(id);
    if (i < 0)
        return;
    if (running) {
        if (!m_transport.playing()) { // from silence: just this one
            for (qsizetype j = 0; j < m_ids.size(); ++j) {
                m_enabled[j] = j == i;
                apply(int(j));
            }
            m_transport.setPlaying(true);
        } else { // joins the others on the next bar
            m_enabled[i] = true;
            apply(int(i));
        }
    } else {
        m_enabled[i] = false;
        apply(int(i));
        bool any = false;
        for (qsizetype j = 0; j < m_ids.size(); ++j)
            any = any || (m_enabled[j] && m_runners.contains(m_ids[j]));
        if (!any)
            m_transport.setPlaying(false);
    }
    emit changed();
}

void RackRunControl::addRunner(const QString &id) { m_runners.insert(id); }

void RackRunControl::playAll()
{
    for (qsizetype j = 0; j < m_ids.size(); ++j) {
        m_enabled[j] = true;
        apply(int(j));
    }
    emit changed();
}

void RackRunControl::apply(int device)
{
    if (device >= 0 && device < m_rack.deviceCount())
        m_rack.device(device).setSequencerEnabled(m_enabled[device]);
}
