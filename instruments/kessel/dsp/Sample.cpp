#include "kessel/dsp/Sample.h"

#include <algorithm>
#include <cmath>

namespace kessel {

Sample Sample::fromChannels(const std::vector<float> &left, const std::vector<float> &right, double rate,
                            std::string name)
{
    Sample s;
    s.rate = rate;
    s.name = std::move(name);
    const auto frames = std::min(left.size(), std::size_t(kMaxSeconds * rate));
    s.data.resize(frames);
    float peak = 0.0f;
    for (std::size_t i = 0; i < frames; ++i) {
        s.data[i] = right.size() > i ? 0.5f * (left[i] + right[i]) : left[i];
        peak = std::max(peak, std::fabs(s.data[i]));
    }
    if (peak <= 0.0f || !std::isfinite(peak)) {
        s.data.clear();
        return s;
    }
    for (float &v : s.data)
        v *= kPeak / peak;
    return s;
}

} // namespace Kessel
