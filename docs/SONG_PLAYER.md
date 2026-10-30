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
- **Any track in view**: click a track to see it; **TAB / NOTES** switches between tab and standard notation (drum tracks: drum notation).
- **Copy to the sequencer**: Shift-click bars of the track in view to select them, then **TO SEQ** copies them into the rack sequencer's patterns ([SEQUENCER.md](SEQUENCER.md#copying-bars-from-a-song)).
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

## Library

Every song you open is kept in Krate's **song library**, by artist, album and track, so it can be found and played again exactly as you left it.

- **Files are copied in**: opening a `.gp` and/or `.mid` file copies them to `~/.local/share/krate/library/<artist>/<album>/`, so tidying Downloads never breaks a song. Opening the same files again finds the song that's already there (by content).
- **Artist, album, title and track number** come from the tab file (title, artist, album) or, for a MIDI file alone, from its name (`Artist-Song_Title-date.mid` reads as artist "Artist", title "Song Title"). All four can be edited.
- **What a song remembers**:
  - its **track setup**: each track's instrument, level, mute, solo and file (TAB/MIDI), and "my track", saved as you change them;
  - its **sounds**: every instrument's full sound (all its knobs; Krimson's four slots with their morph targets) and the **mixer** (each strip's fader, pan, mute and solo, and the master), saved when you press **SAVE** on the Song screen, so experimenting never overwrites a good setup.
- **Opening a song** puts its saved sounds and mixer on the rack. **Closing it** (or opening another, or quitting) puts back what it changed (the instruments it has sounds for, and the mixer if it has one) as they were before it opened: a song never leaves your patches changed. Instruments it has no sound for (added to the rack after it was saved) keep whatever you change while it's open. SAVE gives the song a sound for every instrument.
- The **library list** on the Song screen: artists, their albums, the songs in track order; a search box (artist, album or title); open a song by clicking it; edit its details; remove it (which deletes its copied files).
- **Storage**: one SQLite database, `~/.local/share/krate/library/library.db` (Qt SQL), with tables for songs, their tracks' setup, their instrument sounds and their mixer, and the last song open (which reopens with the rack). It replaces S4's `songs.json`.

## Backing track

A song can have a recording to play along with: an MP3, in sync with the tab and the MIDI tracks.

- **Load**: **LOAD MP3…** on the Song screen's BACKING bar, or drop an MP3 on an open song. It's copied into the library beside the song (`<artist>/<album>/<title> backing.mp3`, replacing an earlier one); **REPLACE…** chooses another, **×** removes it (and its copy). Decoding (dr_mp3, `third_party/dr_libs`) runs off the GUI thread; the bar says so meanwhile.
- **Sync**: **BAR 1 AT** is where the song's bar 1 is in the recording (type seconds or m:ss.mmm; negative if the recording starts after bar 1); nudge it by a beat (at the song's opening tempo), 0.1 s or 0.01 s. Kept with the song, as is the file.
- **Level**: the mixer's **Backing** strip (fader, pan, mute, solo), kept with the song's mixer when you SAVE.
- **How it plays** (`shared/song/BackingTrack.cpp`): the backing track is a rack device after the instruments (a mixer strip and a `backing_L/R` output, no panel or tab; focus passes it by). Each block it asks the song player for the song's time at the block's start (`SongPlayer::blockSeconds()`: the fractional tick through the tempo map) and plays the recording from there plus the offset, reading between samples with a 4-point curve (so a 44.1 kHz file plays right at 48 kHz). So it starts, stops and jumps with the song, follows tempo changes (the tempo map is the song's time) and the speed (as varispeed: slower is also lower in pitch); a start or a jump fades in over 5 ms. Silent while the song isn't playing.
- A jump while playing now takes its tempo from where it lands for its first block too (before, that block ran at the old place's tempo: across a tempo change everything landed a block off).
- Schema version 2 of `library.db`: `songs.backing_file`, `songs.backing_offset` (a version 1 library is upgraded as it opens).
- Tests: `tst_backingtrack` (decoding an MP3; in sync with the offset to within 2 samples; after a jump through a tempo change; at half speed; silent when stopped; fading in), `tst_library` (copies, replacing, the offset, removal, the upgrade).

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
| L1 ✅ | **Library: database** | `SongLibrary` (Qt SQL, SQLite): the schema with a version; adding a song copies its files and reads artist / album / title / track from the tab file or the MIDI file's name; the same files again find the same song; songs by artist and album, search, edit, remove; each song's track setup stored there (instead of `songs.json`) and the last song reopening; `tst_library`. Result: `library.db` (schema version 1: `songs`, `song_tracks`, `song_sounds`, `song_mixer`, `settings`; foreign keys cascade) in `~/.local/share/krate/library/`, beside the copies under `<artist>/<album>/<title>.gp|.mid` (names made safe; a second song of the same title gets " (2)"); songs found again by a SHA-256 of their files; search by artist, album or title (case-insensitive, no wildcards); removing a song deletes its copies and empty folders. Opening files in the Song screen adds them and plays the library's copies; `songs.json` from S4 is brought in once (its last song and setup) and renamed `.old`. `tst_library` (6 tests), `tst_song` now runs on a library |
| L2 ✅ | **Library: sounds and mixer** | `InstrumentInstance::saveSound()` / `loadSound()` (every knob; Krimson's four slots), the mixer as a snapshot; SAVE stores both with the song; opening a song applies them, closing (or opening another) restores what was there before, also when Krate quits with a song open; tests. Result: the default `saveSound` / `loadSound` keep every parameter by key and set them as the host would (the panel follows and saves); Krimson adds its four slots with morph targets and set-up (and saves `slots.json`); `RackController::mixerState()` / `setMixerState()` (every strip by instrument id, and the master); **SAVE** on the Song screen (orange until a song's sounds are saved; the note says "sounds saved" / "not saved"); the Song controller keeps your sounds and mixer from before the first song, puts them back when a song closes, before another song's go on, and on quitting (the panels now check for a host change once more as they close, so it's saved). `tst_song` (sounds come and go), `tst_krimson` (the whole sound) |
| L3 ✅ | **Library: the list** | The library on the Song screen: artists → albums → songs, search, click to open, edit details, remove; SAVE with a "sounds saved" note; screenshot. Result: a **LIBRARY** panel on the left of the Song screen (shown while no song is open; the LIBRARY button toggles it): artists (any case), their albums ("Unknown album" when there's none), songs in track order with their numbers, a dot on songs with saved sounds, the open song highlighted, search by artist, album or title; click a song to open it. **EDIT** (for the open song): artist, album, title, track number, and REMOVE (asks once more; deletes the song's copies). `tst_song` (the list, editing, removing) |
| S5b ✅ | **Any track, tab or notes** | Click a track to view it (MINE still marks yours); TAB / NOTES above the view. Result: `layoutNotation` shares the tab layout's bar packing, cursor and clicks: a five-line staff with a treble or bass clef from the track's range (guitar and bass written an octave up), note heads by pitch (black or white, tilted), sharps and naturals within a bar, ledger lines, stems away from the middle line (or by voice when two share a bar), flags, beams and part-beams, dots, tuplet numbers, short ties, rests (Noto Music glyphs); drum tracks on a percussion staff with x heads for hi-hats and cymbals and an "o" for an open hi-hat. `tst_tablayout` (6 notation tests), `tst_song` (the view follows clicks) |
| S6 | **Practice** | Speed 50–100 %, loop A–B (drag in the tab or set at the cursor), count-in on Klick, mute/guide of "my track"; all remembered per song; learnable rack actions for play, back to start, loop, speed, set A / B |
| S7 | **Polish** | Tested with several real songs (repeats, tempo changes, odd meters, drop tunings, bass and 7-string); accessibility; docs final |

## Later

| Feature | Notes |
| --- | --- |
| Notation details | Key signatures, ties as curves, grace notes drawn small, two staves (piano), notation for MIDI-only songs |
| Fallback sound | A simple built-in voice for unassigned tracks |
| MIDI out | Send tracks to a JACK MIDI port for external synths or a soundfont player |
| Older tab formats | `.gp5` / `.gp4` / `.gp3` (binary), `.gpx` |
| Slides and whammy as sound | Slides play as two notes today (the MIDI export plays a chromatic run); a pitch glide or run, and whammy-bar dips, would be closer |
| Backing audio | Play an audio file in sync with the song |
| Guitar input | Hear your guitar through the rack (JACK audio in), maybe with a tuner |
