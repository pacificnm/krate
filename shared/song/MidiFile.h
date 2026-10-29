#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "song/Song.h"

// Standard MIDI File reader (format 0 and 1) for the song player
// (docs/SONG_PLAYER.md). Keeps what the player needs: the tempo map, time
// signatures, track names, notes, pitch bend and controllers. Each part is
// one channel: a track using several channels (format 0 has only one track)
// becomes one Track per channel. Tracks with no channel events (a conductor
// track) are dropped. No Qt.
namespace krate::song {

struct MidiFileResult
{
    std::optional<Song> song;
    std::string error; // why there's no song
};

MidiFileResult readMidiFile(std::span<const std::uint8_t> data);
MidiFileResult readMidiFile(const std::string &path);

} // namespace krate::song
