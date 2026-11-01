#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace krate {

// A sound to audition (the sound library, docs/SOUNDS.md): its audio at its
// own rate (right empty: mono).
struct PreviewSound
{
    std::vector<float> left, right;
    double rate = 48000.0;
    std::size_t frames() const { return left.size(); }
};

// Plays one sound at a time into the rack's master output, after the mixer
// (the rack processor mixes it in), once, from its start: auditioning while
// browsing, whatever the transport does. The GUI hands a sound over (it owns
// it: free the one handed back once blocks() has moved on by two).
class Previewer
{
public:
    // --- GUI thread
    const PreviewSound *play(const PreviewSound *sound) noexcept
    {
        m_generation.fetch_add(1, std::memory_order_relaxed);
        return m_sound.exchange(sound, std::memory_order_acq_rel);
    }
    const PreviewSound *stop() noexcept { return play(nullptr); }
    bool playing() const noexcept { return m_playing.load(std::memory_order_relaxed); }
    std::uint64_t blocks() const noexcept { return m_blocks.load(std::memory_order_acquire); }
    void setLevel(float level) noexcept { m_level.store(std::clamp(level, 0.0f, 2.0f), std::memory_order_relaxed); }

    // --- Audio thread: adds it to `left` / `right`.
    void mix(float *left, float *right, std::uint32_t frames, double sampleRate) noexcept
    {
        const PreviewSound *sound = m_sound.load(std::memory_order_acquire);
        const std::uint64_t generation = m_generation.load(std::memory_order_relaxed);
        if (generation != m_seen) { // a new one (or the same again): from its start
            m_seen = generation;
            m_position = 0.0;
        }
        bool playing = false;
        if (sound && sound->frames() > 1 && sampleRate > 0.0) {
            const float level = m_level.load(std::memory_order_relaxed);
            const double step = sound->rate / sampleRate;
            const bool mono = sound->right.empty();
            const double last = double(sound->frames() - 1);
            for (std::uint32_t i = 0; i < frames && m_position < last; ++i, m_position += step) {
                const auto k = std::size_t(m_position);
                const float t = float(m_position - double(k));
                const float l = sound->left[k] + t * (sound->left[k + 1] - sound->left[k]);
                const float r = mono ? l : sound->right[k] + t * (sound->right[k + 1] - sound->right[k]);
                left[i] += level * l;
                right[i] += level * r;
            }
            playing = m_position < last;
        }
        m_playing.store(playing, std::memory_order_relaxed);
        m_blocks.fetch_add(1, std::memory_order_release);
    }

private:
    std::atomic<const PreviewSound *> m_sound{nullptr};
    std::atomic<std::uint64_t> m_generation{0};
    std::atomic<float> m_level{0.8f};
    std::atomic<bool> m_playing{false};
    std::atomic<std::uint64_t> m_blocks{0};
    // Audio thread.
    std::uint64_t m_seen = 0;
    double m_position = 0.0;
};

} // namespace krate
