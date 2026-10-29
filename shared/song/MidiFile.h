#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "song/Song.h"

// Standard MIDI File reader (format 0 and 1) for the song player
// (docs/SONG_PLAYER.md). Keeps what the player needs: the tempo map, time
// signatures, track names, notes, pitch bend and controllers.
//
// Format 1: each file track is one part, whatever channels it uses. Files
// exported from tab software put each string on its own channel (so a bend
// bends one string), which a Track keeps (Event::channel is the string's).
// Format 0 has a single track holding the whole band: it is split into one
// part per channel. Tracks without notes (a conductor track, set-up messages)
// are dropped. Names are returned as UTF-8 (Latin-1 names are converted). No Qt.
namespace krate::song {

struct MidiFileResult
{
    std::optional<Song> song;
    std::string error; // why there's no song
};

MidiFileResult readMidiFile(std::span<const std::uint8_t> data);
MidiFileResult readMidiFile(const std::string &path);

} // namespace krate::song
