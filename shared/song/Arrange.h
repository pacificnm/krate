#pragma once

#include <vector>

#include "song/Score.h"
#include "song/Song.h"

// Helpers for putting a song's tracks on Krate's instruments
// (docs/SONG_PLAYER.md). No Qt.
namespace krate::song {

// A drum-kit note number for an 11-voice kit (kick, rim, snare, clap, low /
// mid / high tom, closed / open hat, crash, ride: the notes K909 plays), from
// any standard drum-kit note: pieces the kit lacks go to the closest voice
// (china and splash to crash, ride bell to ride, congas and bongos to toms,
// cowbell, wood block and claves to rim, shakers and tambourine to the closed
// hat). -1 for sounds with no sensible stand-in.
int mapToBasicKit(int note) noexcept;

// What a track is, for suggesting an instrument for it.
enum class TrackRole { Drums, Bass, Guitar, Keys, Other };

// From the instrument name and type in the file and the notes' range.
TrackRole guessRole(const Track &track, const ScoreTrack *score = nullptr) noexcept;

const char *roleName(TrackRole role) noexcept;

// A suggested instrument for each track: drums to the drum machine, the first
// bass to the bass synth, keys and the rest to the polysynth parts in order
// (part 0, 1, ...), guitars to nothing (they're for the player). Tracks
// without notes get nothing.
struct Suggestion
{
    enum class Kind { None, Drums, Bass, Poly } kind = Kind::None;
    int part = 0; // Poly: 0, 1, 2 ...
};
std::vector<Suggestion> suggestInstruments(const Song &song, const std::vector<const ScoreTrack *> &score = {});

} // namespace krate::song
