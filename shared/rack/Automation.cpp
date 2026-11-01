#include "rack/Automation.h"

#include <limits>

namespace krate {

AutomationPlayer::AutomationPlayer()
{
    for (auto &v : m_last)
        v.store(std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);
}

AutomationPlayer::~AutomationPlayer()
{
    // The audio thread has stopped calling us by now.
    while (const auto a = m_incoming.pop())
        delete *a;
    collectGarbage();
    delete m_pending;
    delete m_backlog;
    delete m_current;
}

void AutomationPlayer::setAutomation(std::unique_ptr<Automation> automation)
{
    collectGarbage();
    Automation *raw = automation.release();
    if (!m_incoming.push(raw)) { // full: the newest wins, on the next collectGarbage()
        delete m_pending;
        m_pending = raw;
    }
}

void AutomationPlayer::collectGarbage()
{
    while (const auto a = m_retired.pop())
        delete *a;
    if (m_pending && m_incoming.push(m_pending))
        m_pending = nullptr;
}

void AutomationPlayer::setHeld(int lane, bool held) noexcept
{
    if (lane >= 0 && lane < kMaxLanes)
        m_held[std::size_t(lane)].store(held, std::memory_order_relaxed);
}

void AutomationPlayer::releaseAll() noexcept
{
    for (auto &h : m_held)
        h.store(false, std::memory_order_relaxed);
}

float AutomationPlayer::lastValue(int lane) const noexcept
{
    return lane >= 0 && lane < kMaxLanes ? m_last[std::size_t(lane)].load(std::memory_order_acquire)
                                         : std::numeric_limits<float>::quiet_NaN();
}

void AutomationPlayer::apply(const TransportState &transport) noexcept
{
    if (m_backlog && m_retired.push(m_backlog))
        m_backlog = nullptr;
    while (!m_backlog) {
        const auto a = m_incoming.pop();
        if (!a)
            break;
        if (m_current && !m_retired.push(m_current))
            m_backlog = m_current;
        m_current = *a;
        for (auto &v : m_last) // another set of lanes: what was set before means nothing now
            v.store(std::numeric_limits<float>::quiet_NaN(), std::memory_order_relaxed);
    }
    if (!m_current || !transport.playing || !m_active.load(std::memory_order_relaxed))
        return;
    const std::size_t count = std::min<std::size_t>(m_current->lanes.size(), kMaxLanes);
    for (std::size_t i = 0; i < count; ++i) {
        const AutomationLane &lane = m_current->lanes[i];
        if (lane.points.empty() || lane.device < 0 || lane.device >= int(m_instruments.size())
            || !m_instruments[std::size_t(lane.device)] || m_held[i].load(std::memory_order_relaxed))
            continue;
        const float value = lane.valueAt(transport.beatAtBlockStart);
        if (value == m_last[i].load(std::memory_order_relaxed))
            continue;
        // Known before the knob moves, so a GUI seeing it move can tell it was us.
        m_last[i].store(value, std::memory_order_release);
        m_instruments[std::size_t(lane.device)]->setParamFromHost(lane.param, value);
    }
}

} // namespace krate
