#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "song/Score.h"
#include "song/Song.h"

// Reader for .gp tab files (the current zip + XML format; docs/SONG_PLAYER.md).
// Gives the score as written (for the tab view) and a Song to play: repeats
// and alternate endings expanded, the tempo map, and each track's notes as
// events. Pitched tracks put each string on its own channel (string n on
// channel n, skipping the drum channel), so a bend bends one string; bends are
// pitch-bend events for a ±2 semitone range. Drums are on channel 10.
// Lyrics are never read. No Qt.
namespace krate::song {

struct GpFileResult
{
    std::optional<Score> score;
    std::optional<Song> song;
    std::string error; // why there's no score
};

GpFileResult readGpFile(std::span<const std::uint8_t> data);
GpFileResult readGpFile(const std::string &path);

// The play order for bars with these repeats and endings (exposed for tests).
std::vector<int> playOrder(const std::vector<MasterBar> &bars);

} // namespace krate::song
