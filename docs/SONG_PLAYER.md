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
  - A track sent to k303 plays one note at a time, the newest (k303 is monophonic); its own sequencer is switched off while it plays a song track.
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

### What real files look like (S0)

From one real exported song (a `.gp` and the `.mid` downloaded with it):

- **They line up one to one**: the same tracks in the same order with the same names. The MIDI file has the repeats written out (180 bars in the tab, 182 played), so the tab's play order must expand repeats the same way.
- **MIDI: one channel per string.** Each instrument track uses channels 1–9; a note's channel is its string, so bends stay on one string. A MIDI track is therefore one part, not one per channel (S1 fixed). Tempo and time signatures are repeated in every track. The resolution is high (15 360 ticks per quarter). Names are Latin-1. There is no conductor track, so no title: the tab file has it.
- **`.gp`: the current version (8.x)**: a zip of `Content/score.gpif` (1.4 MB of XML here, 57 KB compressed) plus layout and style files we ignore.
  - Everything is shared by id and **deduplicated**: 180 master bars × 8 tracks = 1 440 bars, but only 243 distinct notes and 8 rhythms. The reader resolves ids and never assumes an element belongs to one parent.
  - **Every note carries its MIDI pitch** (`Midi` property) as well as `String` and `Fret`, so playback doesn't need the tuning. Strings count from the lowest (0). Tuning is `Pitches` on the staff (`Staves/Staff/Properties`).
  - Tracks aren't all six-string guitars: a 5-string bass (B0–G2), keyboards written as 7-string tabs, drums. Drum notes have the kit's note number in `Fret` and `Midi`, and an `InstrumentArticulation` index.
  - Techniques seen: hammer-on/pull-off (`HopoOrigin`/`HopoDestination`), slides, dead notes (`Muted`), palm mutes, harmonics, ties, vibrato, accents; bends as origin/destination offset and value (25 = a quarter tone) and whammy on beats.
  - **Lyrics** are in each track and on beats. The reader skips them.
  - The tempo map is the master track's tempo automations ("131 2": BPM and the beat unit).

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
- **One channel per instrument**: a track's per-string channels are folded onto the one channel its instrument hears, so a bend on one string bends the instrument's whole channel (fine for bass and leads; a bent note inside a chord bends the chord). Only sustain and the mod wheel are passed on as controllers; volume, pan and effect controllers in the file would fight the rack's mixer.
- **Levels** scale the track's velocities (several tracks can share one instrument, so there's no per-track fader); the instrument's mixer strip sets its overall level.

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
| S0 ✅ | **Look at real files** | A `.gp` and a `.mid` from the same song in the git-ignored `reference/` folder; a throwaway dump tool lists what is in them (tracks, tunings, tempo map, a few bars of notes, the MIDI tracks and how they line up). This doc updated with anything that surprises us. Result: `song-dump` (`apps/song-dump`); one real song checked (8 tracks, 180 bars, a 6/4 bar, a repeat). Findings below, under *What real files look like*; S1's reader was corrected for them |
| S1 ✅ | **MIDI files** | `shared/song`: `Song`, tempo map, tick/beat/bar conversions; `MidiFileReader` (SMF 0/1, running status, tempo, names, notes, bend, CC); `tst_midifile` with generated files. Result: library `krate_song` (no Qt): `Song`, `Track`, `Event`, `TempoMap` (tempo and time-signature changes; tick ↔ seconds, bar and beat, bar starts across meter changes) and `readMidiFile` (format 0 and 1; running status; tempo, time signature, track and instrument names; notes, pitch bend, CCs, the first program change; velocity-0 note-ons as note-offs; every note-on paired with a note-off; each file track one part with its events keeping their channels (files from tab software put each string on its own channel), format 0 files split into a part per channel; tracks without notes dropped; Latin-1 names converted to UTF-8; the conductor track names the song; lyrics, sysex and aftertouch skipped; SMPTE and format 2 refused with a message). `tst_midifile` (9 tests); 200 000 mutated files read cleanly under AddressSanitizer and UBSan |
| S2 ✅ | **Tab files** | Zip reader (stored + deflate via zlib); `GpReader` for tracks, tunings, bars, time signatures, repeats, tempo, notes with string/fret and techniques, as `Song` + score; `tst_gpfile` with small generated `.gp` files; the dump tool reads a real file. Result: `ZipArchive` (zlib), a small XML reader (`song/Xml`, no Qt), the `Score` model (bars as written; per track voices of beats with start, length, note value, dots, tuplets and velocity from the dynamic; notes with string, fret, sounding pitch, techniques and bend points; tempo marks) and `readGpFile` (score + `Song`: repeats and alternate endings expanded into the play order, the tempo map with beat units and changes inside bars, each string on its own channel, ties held, let-ring, short soft dead notes, accents and ghost notes, bends as pitch bend for a ±2 semitone range, pinch/artificial and natural harmonics at the pitch they sound, grace notes taking no time, drums on channel 10, tracks kept in the score's order even when empty). Checked against the real song's MIDI file with `song-dump --compare`: **every one of the tab's notes in all 8 tracks matches the MIDI file's** in pitch and beat (a few guitar notes 5 ms apart); the MIDI file's extra notes are the exporter's slide runs. 50 ms to read. `tst_gpfile` (8 tests); 3 400 damaged files (zip and XML) read cleanly under AddressSanitizer and UBSan |
| S3 ✅ | **Playback in the rack** | `SongPlayer` on the audio thread; device-addressed events in `Rack`; assignments, levels, mutes; the tempo map drives the transport; the drum map to K909; note-offs on stop/jump; `krate-render --song FILE --assign ...` renders a song through the instruments offline; `tst_songplayer` (events on the exact sample, tempo changes, stop releases everything). Result: `IEventSource` (`core/EventSource.h`) so the rack never depends on songs; `RackBlock::deviceMidi` merged into each device's MIDI in frame order, past routing and learn; `RackProcessor::setEventSource` for the app; `SongPlayer` (songs handed over and back lock-free; per track assignment, level, mute, solo; speed 0.25–2; cue and jump; notes, bends and sustain ended on stop, jump, reassign, mute, solo and load; `ended`); `mapToBasicKit` (china, splash, ride bell, congas, cowbell, shakers… onto the 11 voices) and `guessRole` (drums, bass, guitar, keys, other); k303's sequencer can be switched off so MIDI keys play it while the transport runs (on by default, renders byte-identical). `krate-render --song` with `--assign TRACK:INSTRUMENT[:SLOT]`, `--start-bar`, `--bars`, `--speed`, `--click`; without `--assign`: drums to K909, the first bass to k303, keys and others to Kalliope then Krimson A–D, guitars left for you. The real song renders through all instruments in under a second for 16 bars. `tst_songplayer` (8 tests); a sanitizer stress run (jumps, reloads, reassigning, mute, solo, speed, stops on the real song) left no note hanging |
| S4 ✅ | **Song tab** | The Song tab: open/drop files, track list with assignment, level, mute, solo and source, "my track", position; play/stop and back to start; assignments remembered in `songs.json`; the last song reopens. Result: a **Song** tab after Mixer (also **S**, or the learnable `rack:song`); `SongController` (QML `Song`) with OPEN (a file dialog for `.gp`/`.mid`), drag and drop, and a lone file bringing its namesake from the same folder; title, artist, files and read errors (a bad file leaves the open song alone); per track: "mine" (the first guitar by default), name, tuning (low to high, "E A D G B E"), part, notes, instrument (every rack instrument but Klick; Krimson as slots A–D), level, M, S, and TAB/MIDI when both files are open (MIDI by default for tracks that line up, on the tab's timeline); \|◂, PLAY/STOP, BAR bar.beat / bars and time; CLOSE. While a song is open the instruments' own sequencers (k303, K909, Kalliope's step sequencer: `IInstrument::setSequencerEnabled`) are off and the song's tempo map runs the transport bar's tempo; at the end the transport stops and goes back to the top. `~/.config/krate/songs.json` keeps every song's set-up by file path and the last song, which reopens with the rack; `rack.json` remembers the Song screen. `tst_song` (6 tests), more in `tst_racklearn` |
| S5 ✅ | **Tab view** | The scrolling tab of "my track": strings and tuning, bar lines and numbers, frets, rhythm stems, techniques; the cursor locked to the transport; pages ahead; click a bar to jump; fast on long songs. Result: `TabLayout` (`shared/song`, no Qt) packs bars into systems across the width (beats spaced by duration, full lines stretched to the edge) and returns simple drawing items: staff lines, tuning letters, bar lines and numbers, time signatures where they change, repeat bars with dots and counts, ending brackets, fret numbers that clear the line behind them, dead notes `x`, harmonics `<12>`, tied and ghost notes in brackets, vibrato `~`, palm mutes `P.M.`, accents, hammer-on and pull-off arcs with `h` / `p`, slide lines, bend arrows with ½ / full labels, and stems, beams, flags, dots, tuplet numbers and rests below; `cursorAt` maps a song position (repeats expanded) to its written bar and x, between beats in proportion; `barAt` for clicks. In the app, `TabView` (a painted item) draws only the systems in view and publishes the cursor for a QML overlay, so playback never repaints the tab; the Song screen gives the tracks up to a third of the height and the tab the rest; while playing, the cursor's line pages to the top; a click on a bar jumps to its next time through. Drum tracks have no tab view (a message says so). The real song's tracks lay out in under 0.3 ms each and the cursor lands in the right bar everywhere. `tst_tablayout` (8 tests) |
| S6 | **Practice** | Speed 50–100 %, loop A–B (drag in the tab or set at the cursor), count-in on Klick, mute/guide of "my track"; all remembered per song; learnable rack actions for play, back to start, loop, speed, set A / B |
| S7 | **Polish** | Tested with several real songs (repeats, tempo changes, odd meters, drop tunings, bass and 7-string); accessibility; docs final |

## Later

| Feature | Notes |
| --- | --- |
| Standard notation | Notes on a staff above the tab |
| Fallback sound | A simple built-in voice for unassigned tracks |
| MIDI out | Send tracks to a JACK MIDI port for external synths or a soundfont player |
| Older tab formats | `.gp5` / `.gp4` / `.gp3` (binary), `.gpx` |
| Slides and whammy as sound | Slides play as two notes today (the MIDI export plays a chromatic run); a pitch glide or run, and whammy-bar dips, would be closer |
| Backing audio | Play an audio file in sync with the song |
| Guitar input | Hear your guitar through the rack (JACK audio in), maybe with a tuner |
