# Krate: Song player

A **Song** tab in the Krate rack for practicing guitar along with a song: load a tab file (`.gp`) and, optionally, a MIDI file of the same song, assign the song's tracks to Krate's instruments (drums to K909, bass to k303, keys and pads to Kalliope or Krimson), and play. The tab of the track you are learning scrolls with a cursor, locked to the rack's transport, while the instruments play the rest of the band.

Song files are the user's own downloads. None ship with Krate, none are committed to this repository, and the tests build their own small files.

Related: [SHARED_FOUNDATIONS.MD](SHARED_FOUNDATIONS.MD) (rack, transport, MIDI routing).

## What it does

- **Load** a song: a `.gp` tab file, a `.mid` file, or both (by file dialog or by dropping the files on the tab).
  - The `.gp` file is the score: tracks with names and instruments, tuning, bars, time signatures, the tempo map, and every note with its string and fret (plus bends, slides, hammer-ons, palm mutes, dead notes, ties).
  - A `.mid` file of the same song is optional. With both, the **tab** comes from the `.gp` file and each track plays from whichever source you pick (the default is the MIDI file when its track lines up, as it has the performance details: velocities, exact bend curves). With only a MIDI file you get playback and assignment but no tab.
- **Tracks**: a list of the song's tracks. For each: the name and instrument from the file, **assign to** (none, k303, K909, Kalliope, Krimson slot A–D), level, mute, solo. Unassigned tracks are silent.
  - Drum tracks map standard drum-kit note numbers to K909's voices (kick, snare, toms, hats, cymbals, claps, rim). Drum notes K909 doesn't have are dropped, or mapped to the closest voice.
  - A track sent to k303 plays its top note when it has chords (k303 is monophonic).
  - Assigning goes straight to the instrument, whatever its MIDI channel or focus, so the keyboard still plays whichever instrument it plays now.
- **Your track**: pick the track you are playing on guitar. Its tab is shown. Its sound can be **muted** or played quietly as a **guide** (if it is assigned to an instrument).
- **Tab view**: six lines (or as many strings as the track has, e.g. 4 for bass, 7 for a 7-string), tuning at the left, bar numbers, fret numbers on the strings with rhythm stems under them, and technique marks (h, p, /, \, b, PM, x, ~, tied notes). A few systems fit on screen. A **cursor** follows the transport to the exact beat, and the view pages ahead so the next line is always visible. Click a bar to move there.
- **Transport**: the rack's transport plays the song. Play/stop, return to start, bar position; the tempo follows the song's tempo map (changes included).
- **Practice**:
  - **Speed** 50–100 % (in 5 % steps): the whole song slows down; the instruments stay in pitch (they are synthesizers, so slowing down is just a slower clock).
  - **Loop** bars A–B: drag across bars in the tab view, or set A and B at the cursor. The loop repeats until switched off.
  - **Count-in**: one bar of Klick before play starts (and before each loop pass, as an option).
  - **Mute / guide** your track (above).
- **Remembered per song**: assignments, levels, your track, speed and loop are saved next to the song files' names in `~/.config/krate/songs.json`, so a song reopens the way you left it. The last song reopens with the rack.
- **MIDI Learn**: play/stop, back to start, loop on/off, speed up/down and set A / set B are learnable rack actions, so a foot controller or the LX49's buttons can run practice hands-free.

## Not in the first version

Standard notation above the tab; editing tabs; a fallback sound for unassigned tracks; audio backing tracks; pitch-shifting audio; per-track MIDI out. (See *Later*.)

## Design

### Files

- **`.gp` files** (the current tab format) are zip archives. The score is the XML file `Content/score.gpif` inside. Krate reads the zip with a small reader of its own (central directory + stored or deflated entries, through zlib, which the build already has) and parses the XML with `QXmlStreamReader`. Only what the player needs is read:
  - `Tracks` (name, instrument / drum kit, tuning via the staff's `Tuning` property, capo), `MasterBars` (time signature, repeats, the bars' order), `Bars` → `Voices` → `Beats` → `Notes`, `Rhythms` (durations, dots, tuplets), the tempo automations on the master track.
  - Notes: string, fret, tie, bend points, slides, hammer-on/pull-off, palm mute, dead note, harmonic, vibrato, let-ring.
  - Repeats and alternate endings are expanded into a play order. The tab view shows the score as written; the cursor jumps with the repeats.
- **`.mid` files**: Standard MIDI File format 0 and 1; tempo map; track names; note on/off; pitch bend; controllers kept (sustain, modulation, volume). Tracks are matched with the `.gp` tracks by name and order; you can rematch them by hand.

### Model (`shared/song`, no Qt in the core)

- `Song`: tracks, the tempo map, bars (start tick, time signature) and the play order; conversions tick ↔ beat ↔ bar position.
- `SongTrack`: name, kind (pitched or drums), strings and tuning, and the events to play (`tick`, note on/off, pitch bend, CC) from the `.gp` notes or from the MIDI file.
- `ScoreTrack` (from `.gp` only): what the tab view draws: per bar, beats with durations and notes with string, fret and techniques.
- Readers: `GpReader` (zip + gpif → `Song` + score), `MidiFileReader` (SMF → `Song`). Both are plain C++ with no Qt dependency. The XML parsing sits behind an interface, so the parser can be Qt's or a small one of our own.

### Playback

- A `SongPlayer` on the audio thread, called by the rack once per block before the devices run. It knows the transport's beat range for the block and emits each assigned track's events in that range, each at its exact sample. Events go **directly to a device** (a new `Rack` entry point for device-addressed events), re-channelled to that device's first channel, or to the chosen Krimson slot's channel.
- The song's tempo map drives the transport's tempo while a song plays: the rack's BPM is set per block from the map, times the speed. Tempo changes land on the block boundary (≤ 1.3 ms at 64 frames).
- Stopping, looping or jumping sends note-offs (and resets bend and sustain) for every assigned track, so nothing hangs.
- The GUI sends the player its commands (load a prepared song, assignments, levels, mutes, speed, loop, jump) through a lock-free queue. Songs are parsed and prepared off the audio thread, and the player swaps them in at a block boundary.
- **Drum map**: a table from standard drum-kit note numbers to K909's voices, in `shared/song` (data only), so other drum instruments can have their own later.

### UI

- A **Song** tab after the instrument tabs (next to Mixer), learnable like the others. The tab shows:
  - the top bar: song title and artist from the file, Open, the source of each track, position (bar.beat), speed, loop, count-in;
  - the track list (assignment, level, mute, solo, "my track");
  - the tab view, filling the rest of the screen.
- The tab view is drawn with Qt Quick shapes and text, laid out once per song and width, and only the cursor moves while playing. It must keep up on long songs (hundreds of bars).

## Roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. The rack, the instruments' reference renders and all tests stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| S0 | **Look at real files** | A `.gp` and a `.mid` from the same song in the git-ignored `reference/` folder; a throwaway dump tool lists what is in them (tracks, tunings, tempo map, a few bars of notes, the MIDI tracks and how they line up). This doc updated with anything that surprises us |
| S1 | **MIDI files** | `shared/song`: `Song`, tempo map, tick/beat/bar conversions; `MidiFileReader` (SMF 0/1, running status, tempo, names, notes, bend, CC); `tst_midifile` with generated files |
| S2 | **Tab files** | Zip reader (stored + deflate via zlib); `GpReader` for tracks, tunings, bars, time signatures, repeats, tempo, notes with string/fret and techniques, as `Song` + score; `tst_gpfile` with small generated `.gp` files; the dump tool reads a real file |
| S3 | **Playback in the rack** | `SongPlayer` on the audio thread; device-addressed events in `Rack`; assignments, levels, mutes; the tempo map drives the transport; the drum map to K909; note-offs on stop/jump; `krate-render --song FILE --assign ...` renders a song through the instruments offline; `tst_songplayer` (events on the exact sample, tempo changes, stop releases everything) |
| S4 | **Song tab** | The Song tab: open/drop files, track list with assignment, level, mute, solo and source, "my track", position; play/stop and back to start; assignments remembered in `songs.json`; the last song reopens |
| S5 | **Tab view** | The scrolling tab of "my track": strings and tuning, bar lines and numbers, frets, rhythm stems, techniques; the cursor locked to the transport; pages ahead; click a bar to jump; fast on long songs |
| S6 | **Practice** | Speed 50–100 %, loop A–B (drag in the tab or set at the cursor), count-in on Klick, mute/guide of "my track"; all remembered per song; learnable rack actions for play, back to start, loop, speed, set A / B |
| S7 | **Polish** | Tested with several real songs (repeats, tempo changes, odd meters, drop tunings, bass and 7-string); accessibility; docs final |

## Later

| Feature | Notes |
| --- | --- |
| Standard notation | Notes on a staff above the tab |
| Fallback sound | A simple built-in voice for unassigned tracks |
| MIDI out | Send tracks to a JACK MIDI port for external synths or a soundfont player |
| Older tab formats | `.gp5` / `.gp4` / `.gp3` (binary), `.gpx` |
| Backing audio | Play an audio file in sync with the song |
| Guitar input | Hear your guitar through the rack (JACK audio in), maybe with a tuner |
