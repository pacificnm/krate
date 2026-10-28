#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "core/ParamDescriptor.h"

namespace krate {

// Lock-free normalized parameter values for an instrument with N parameters,
// initialised from its descriptor table, plus "changed by audio" bits for the
// GUI. Relaxed atomics: each value is independent and readers smooth.
template<std::size_t N>
class ParamValues
{
    static_assert(N <= 64, "changed-bits mask is 64 bits");
    static_assert(std::atomic<float>::is_always_lock_free);

public:
    explicit ParamValues(const std::array<ParamDescriptor, N> &params) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            m_values[i].store(params[i].defaultValue, std::memory_order_relaxed);
    }

    float get(int index) const noexcept
    {
        return valid(index) ? m_values[std::size_t(index)].load(std::memory_order_relaxed) : 0.0f;
    }

    void set(int index, float value) noexcept
    {
        if (valid(index))
            m_values[std::size_t(index)].store(std::clamp(value, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    // Audio thread: set, and flag it so the panel follows.
    void setFromAudio(int index, float value) noexcept
    {
        if (!valid(index))
            return;
        set(index, value);
        m_changed.fetch_or(std::uint64_t(1) << index, std::memory_order_relaxed);
    }

    std::uint64_t takeChanged() noexcept { return m_changed.exchange(0, std::memory_order_relaxed); }

private:
    static constexpr bool valid(int index) noexcept { return index >= 0 && index < int(N); }

    std::array<std::atomic<float>, N> m_values;
    std::atomic<std::uint64_t> m_changed{0};
};

} // namespace krate
