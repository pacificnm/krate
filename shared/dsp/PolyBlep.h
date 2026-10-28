#pragma once

namespace krate {

// Two-sample polynomial approximation of a band-limited step residual, for an
// oscillator with a unit-height-2 discontinuity at phase 0 (a saw's wrap).
// t: phase 0..1, dt: phase increment per sample. Add (rising step) or
// subtract (falling step) it to the naive waveform.
inline double polyBlep(double t, double dt) noexcept
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

} // namespace krate
