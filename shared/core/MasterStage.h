#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace krate {

// The host's last stage before the outputs (docs/SHARED_FOUNDATIONS.MD:
// instruments stay bounded but don't limit; the host or the rack's mixer owns
// the master stage). v1 is a tanh soft clip, which never produces a hard over,
// plus a peak meter for the GUI. Realtime-safe.
class MasterStage
{
public:
    // Audio thread: soft-clip `channels` buffers in place and track the peak.
    void process(float *const *buffers, int channels, std::uint32_t frames) noexcept
    {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            float *b = buffers[c];
            for (std::uint32_t i = 0; i < frames; ++i) {
                b[i] = std::tanh(b[i]);
                peak = std::max(peak, std::fabs(b[i]));
            }
        }
        // Single writer; the GUI's exchange may drop one block's peak, which is harmless.
        if (peak > m_peak.load(std::memory_order_relaxed))
            m_peak.store(peak, std::memory_order_relaxed);
    }

    // GUI thread: peak absolute output since the last call (0..1).
    float takePeak() noexcept { return m_peak.exchange(0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> m_peak{0.0f};
};

} // namespace krate
