#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace krate {

// Fixed-size, lock-free single-producer / single-consumer queue. One thread
// pushes, one thread pops; neither ever blocks or allocates. Capacity is
// N - 1 items (one slot stays empty to tell full from empty).
template<typename T, std::size_t N>
class SpscQueue
{
    static_assert(N >= 2 && (N & (N - 1)) == 0, "capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>, "items are copied by value across threads");

public:
    // Producer thread. Returns false if the queue is full (the item is dropped).
    bool push(const T &item) noexcept
    {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & (N - 1);
        if (next == m_tail.load(std::memory_order_acquire))
            return false;
        m_items[head] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread.
    std::optional<T> pop() noexcept
    {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire))
            return std::nullopt;
        T item = m_items[tail];
        m_tail.store((tail + 1) & (N - 1), std::memory_order_release);
        return item;
    }

    static constexpr std::size_t capacity() { return N - 1; }

private:
    std::array<T, N> m_items{};
    alignas(64) std::atomic<std::size_t> m_head{0}; // written by the producer
    alignas(64) std::atomic<std::size_t> m_tail{0}; // written by the consumer
};

} // namespace krate
