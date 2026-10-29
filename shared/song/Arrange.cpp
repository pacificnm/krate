#include "song/Arrange.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace krate::song {

namespace {
constexpr int kKick = 36, kRim = 37, kSnare = 38, kClap = 39, kLowTom = 41, kClosedHat = 42, kMidTom = 45,
              kOpenHat = 46, kHighTom = 48, kCrash = 49, kRide = 51;

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

bool contains(const std::string &haystack, const char *needle) { return haystack.find(needle) != std::string::npos; }
} // namespace

int mapToBasicKit(int note) noexcept
{
    switch (note) {
    case 35: case 36: return kKick;
    case 37: return kRim;
    case 38: case 40: return kSnare;
    case 39: return kClap;
    case 41: case 43: return kLowTom;
    case 42: case 44: return kClosedHat;
    case 45: case 47: return kMidTom;
    case 46: return kOpenHat;
    case 48: case 50: return kHighTom;
    case 49: case 52: case 55: case 57: return kCrash; // crash, china, splash
    case 51: case 53: case 59: return kRide;           // ride, ride bell
    case 54: case 69: case 70: case 82: return kClosedHat; // tambourine, cabasa, maracas, shaker
    case 80: return kClosedHat;                            // closed triangle
    case 81: return kOpenHat;                              // open triangle
    case 56: case 67: case 68: case 75: case 76: case 77: return kRim; // cowbell, agogo, claves, wood blocks
    case 31: case 33: case 34: return kRim;                // sticks, metronome click and bell
    case 60: case 62: case 65: return kHighTom;            // high bongo, mute high conga, high timbale
    case 61: case 63: case 66: return kMidTom;             // low bongo, open high conga, low timbale
    case 64: return kLowTom;                               // low conga
    case 27: case 28: case 29: case 30: case 32: return -1; // high Q, slap, scratches, square click
    default: return -1;
    }
}

TrackRole guessRole(const Track &track, const ScoreTrack *score) noexcept
{
    if (track.drums || (score && score->drums))
        return TrackRole::Drums;
    const std::string name = lower(track.name + " " + track.instrument + (score ? " " + score->instrument : std::string()));
    if (contains(name, "bass") && !contains(name, "bassoon"))
        return TrackRole::Bass;
    if (contains(name, "guitar") || contains(name, "gtr"))
        return TrackRole::Guitar;
    if (contains(name, "piano") || contains(name, "key") || contains(name, "organ") || contains(name, "synth")
        || contains(name, "clav") || contains(name, "pad") || contains(name, "string") || contains(name, "harpsichord")
        || contains(name, "atmosphere"))
        return TrackRole::Keys;
    if (track.program >= 32 && track.program <= 39) // the standard bass programs
        return TrackRole::Bass;
    if (track.program >= 24 && track.program <= 31) // guitars
        return TrackRole::Guitar;
    if (track.highestNote >= 0 && track.highestNote < 55) // everything below the G under middle C
        return TrackRole::Bass;
    return TrackRole::Other;
}

const char *roleName(TrackRole role) noexcept
{
    switch (role) {
    case TrackRole::Drums: return "drums";
    case TrackRole::Bass: return "bass";
    case TrackRole::Guitar: return "guitar";
    case TrackRole::Keys: return "keys";
    case TrackRole::Other: return "other";
    }
    return "other";
}

} // namespace krate::song
