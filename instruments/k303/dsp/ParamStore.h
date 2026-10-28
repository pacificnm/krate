#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include "dsp/Params.h"

namespace k303 {

// Normalized (0..1) parameter values shared between the GUI and the audio
// thread. Plain relaxed atomics: each value is independent and the audio
// thread smooths whatever it reads, so no ordering is needed.
class ParamStore
{
public:
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(ParamCount <= 64, "changed-bits mask is 64 bits");

    ParamStore() noexcept
    {
        for (std::size_t i = 0; i < kParams.size(); ++i)
            m_values[i].store(kParams[i].defaultValue, std::memory_order_relaxed);
    }

    float get(ParamId id) const noexcept
    {
        return m_values[index(id)].load(std::memory_order_relaxed);
    }

    void set(ParamId id, float value) noexcept
    {
        m_values[index(id)].store(std::clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    // Audio thread: set a value that came from MIDI and flag it for the GUI.
    void setFromAudio(ParamId id, float value) noexcept
    {
        set(id, value);
        m_changedByAudio.fetch_or(std::uint64_t(1) << index(id), std::memory_order_relaxed);
    }

    // GUI thread: bitmask (bit = ParamId) of values the audio thread changed since last call.
    std::uint64_t takeChangedByAudio() noexcept
    {
        return m_changedByAudio.exchange(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t index(ParamId id) { return static_cast<std::size_t>(id); }

    std::array<std::atomic<float>, ParamCount> m_values;
    std::atomic<std::uint64_t> m_changedByAudio{0};
};

} // namespace k303
