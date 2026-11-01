#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace krate {

// Stereo audio from the audio thread to another (recording to disk): one
// writer, one reader, no locks. The writer never waits: what doesn't fit is
// dropped and counted (overruns), which only happens if the disk falls
// seconds behind (docs/DAW.md, D2).
class AudioRing
{
public:
    explicit AudioRing(std::size_t frames)
        : m_left(frames + 1)
        , m_right(frames + 1)
    {
    }

    std::size_t capacity() const noexcept { return m_left.size() - 1; }

    // Writer (audio thread): frames written (all of them unless it's full).
    std::size_t write(const float *left, const float *right, std::size_t frames) noexcept
    {
        const std::size_t size = m_left.size();
        const std::size_t w = m_write.load(std::memory_order_relaxed);
        const std::size_t r = m_read.load(std::memory_order_acquire);
        const std::size_t free = (r + size - w - 1) % size;
        const std::size_t n = std::min(frames, free);
        for (std::size_t i = 0; i < n; ++i) {
            m_left[(w + i) % size] = left[i];
            m_right[(w + i) % size] = right[i];
        }
        m_write.store((w + n) % size, std::memory_order_release);
        if (n < frames)
            m_overruns.fetch_add(frames - n, std::memory_order_relaxed);
        return n;
    }

    // Reader: up to `frames`, oldest first.
    std::size_t read(float *left, float *right, std::size_t frames) noexcept
    {
        const std::size_t size = m_left.size();
        const std::size_t r = m_read.load(std::memory_order_relaxed);
        const std::size_t w = m_write.load(std::memory_order_acquire);
        const std::size_t available = (w + size - r) % size;
        const std::size_t n = std::min(frames, available);
        for (std::size_t i = 0; i < n; ++i) {
            left[i] = m_left[(r + i) % size];
            right[i] = m_right[(r + i) % size];
        }
        m_read.store((r + n) % size, std::memory_order_release);
        return n;
    }

    std::size_t available() const noexcept
    {
        const std::size_t size = m_left.size();
        return (m_write.load(std::memory_order_acquire) + size - m_read.load(std::memory_order_relaxed)) % size;
    }
    // Frames dropped because the reader fell behind (since the last call).
    std::uint64_t takeOverruns() noexcept { return m_overruns.exchange(0, std::memory_order_relaxed); }

private:
    std::vector<float> m_left, m_right;
    std::atomic<std::size_t> m_write{0};
    std::atomic<std::size_t> m_read{0};
    std::atomic<std::uint64_t> m_overruns{0};
};

} // namespace krate
