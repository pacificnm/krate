#include "k909/dsp/Params.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace k909 {

namespace units {
int shuffleAmount(float v) { return int(std::lround(double(v) * 5.0)); }
int index(float v, int count) { return std::clamp(int(std::lround(double(v) * (count - 1))), 0, count - 1); }
double delayBeats(float v)
{
    static constexpr double kBeats[kDelayTimes] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
    return kBeats[index(v, kDelayTimes)];
}
} // namespace units

namespace format {
std::string percent(float v) { return std::format("{}%", std::lround(v * 100.0f)); }
std::string level(float v)
{
    const double g = units::gain(v);
    return g <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(g));
}
std::string pan(float v)
{
    const long p = std::lround(units::pan(v) * 100.0);
    return p == 0 ? std::string("C") : p < 0 ? std::format("L{}", -p) : std::format("R{}", p);
}
std::string onOff(float v) { return units::index(v, 2) == 1 ? "On" : "Off"; }
std::string delayTime(float v)
{
    static constexpr const char *kNames[kDelayTimes] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
    return kNames[units::index(v, kDelayTimes)];
}
std::string shuffle(float v)
{
    const int amount = units::shuffleAmount(v);
    return amount == 0 ? std::string("Straight") : std::format("{}", amount);
}
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }
} // namespace format

} // namespace k909
