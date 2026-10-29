#pragma once

#include <array>
#include <cstdint>

namespace krunch {

// A pedal (docs/ROADMAP.md, after G6): mono, in place, at the base rate.
class IPedal
{
public:
    virtual ~IPedal() = default;
    virtual void prepare(double sampleRate) noexcept = 0;
    virtual void process(float *buffer, std::uint32_t frames) noexcept = 0;
};

// The pedals in one place (in front of the amp, or in the effects loop), in
// order. Empty until there are pedals: it passes the signal through.
class PedalChain
{
public:
    static constexpr int kMaxPedals = 8;

    // Setup, before processing.
    bool add(IPedal &pedal) noexcept
    {
        if (m_count >= kMaxPedals)
            return false;
        m_pedals[std::size_t(m_count++)] = &pedal;
        return true;
    }
    int count() const noexcept { return m_count; }

    void prepare(double sampleRate) noexcept
    {
        for (int i = 0; i < m_count; ++i)
            m_pedals[std::size_t(i)]->prepare(sampleRate);
    }
    void process(float *buffer, std::uint32_t frames) noexcept
    {
        for (int i = 0; i < m_count; ++i)
            m_pedals[std::size_t(i)]->process(buffer, frames);
    }

private:
    std::array<IPedal *, kMaxPedals> m_pedals{};
    int m_count = 0;
};

} // namespace krunch
