#include "dsp/Params.h"

#include <cmath>
#include <format>

namespace k909 {

namespace units {
int shuffleAmount(float v) { return int(std::lround(double(v) * 5.0)); }
} // namespace units

namespace format {
std::string percent(float v) { return std::format("{}%", std::lround(v * 100.0f)); }
std::string level(float v)
{
    const double g = units::gain(v);
    return g <= 0.0 ? "-inf dB" : std::format("{:.1f} dB", 20.0 * std::log10(g));
}
std::string shuffle(float v)
{
    const int amount = units::shuffleAmount(v);
    return amount == 0 ? std::string("Straight") : std::format("{}", amount);
}
std::string tempo(float v) { return std::format("{} BPM", std::lround(units::tempoBpm(v))); }
} // namespace format

} // namespace k909
