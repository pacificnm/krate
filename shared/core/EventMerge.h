#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "core/EventSource.h"

namespace krate {

// Several event sources playing into one rack (the song player and the
// sequencer): each renders into its own share of the output, then the shares
// are merged by frame (a source's events keep their order; at the same frame,
// earlier sources first). beforeTransport() goes to each in order, so a source
// that sets the tempo (the song player) still does. Audio thread.
template<std::size_t N>
class EventMerge final : public IEventSource
{
public:
    explicit EventMerge(std::array<IEventSource *, N> sources)
        : m_sources(sources)
    {
    }

    void beforeTransport(Transport &transport) noexcept override
    {
        for (IEventSource *s : m_sources)
            if (s)
                s->beforeTransport(transport);
    }

    std::uint32_t render(const TransportState &transport, std::uint32_t frames, double sampleRate, DeviceEvent *out,
                         std::uint32_t capacity) noexcept override
    {
        // Each source renders into the scratch buffer at its own offset.
        const std::uint32_t share = std::min<std::uint32_t>(capacity, kScratch) / std::uint32_t(N);
        std::array<std::uint32_t, N> count{}, next{};
        for (std::size_t i = 0; i < N; ++i)
            if (m_sources[i])
                count[i] = m_sources[i]->render(transport, frames, sampleRate, m_scratch.data() + i * share, share);
        std::uint32_t written = 0;
        while (written < capacity) {
            std::size_t best = N;
            for (std::size_t i = 0; i < N; ++i)
                if (next[i] < count[i]
                    && (best == N || m_scratch[i * share + next[i]].frame() < m_scratch[best * share + next[best]].frame()))
                    best = i;
            if (best == N)
                break;
            out[written++] = m_scratch[best * share + next[best]++];
        }
        return written;
    }

private:
    static constexpr std::uint32_t kScratch = 4096;
    std::array<IEventSource *, N> m_sources;
    std::array<DeviceEvent, kScratch> m_scratch{};
};

} // namespace krate
