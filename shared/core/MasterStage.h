#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace krate {

// The host's last stage before the outputs (docs/SHARED_FOUNDATIONS.MD:
// instruments stay bounded but don't limit; the host or the rack's mixer owns
// the master stage). A soft clip that leaves everything up to kKnee (-6 dBFS)
// untouched and rounds the peaks above it into 1.0, so it never produces a hard
// over; plus a peak meter for the GUI. Realtime-safe.
//
// v1 was a plain tanh, which bends everything: a mix peaking at 0.85 picked
// up ~6 % distortion (23 dB below the music; docs/SHARED_FOUNDATIONS.MD, Master stage).
class MasterStage
{
public:
    static constexpr float kKnee = 0.5f;

    // The curve: linear to kKnee, then kKnee + (1 - kKnee) tanh(over / (1 - kKnee)),
    // which meets the line with the same slope and approaches 1.0.
    static float shape(float x) noexcept
    {
        const float a = std::fabs(x);
        if (!(a > kKnee)) // NaN falls through unchanged; the instruments never produce one
            return x;
        const float y = kKnee + (1.0f - kKnee) * std::tanh((a - kKnee) / (1.0f - kKnee));
        return std::copysign(y, x);
    }

    // Audio thread: soft-clip `channels` buffers in place and track the peak.
    void process(float *const *buffers, int channels, std::uint32_t frames) noexcept
    {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            float *b = buffers[c];
            for (std::uint32_t i = 0; i < frames; ++i) {
                b[i] = shape(b[i]);
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
