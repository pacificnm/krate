# Krate: Sequencer

A multitrack pattern sequencer for the whole rack, in the spirit of the classic 8-track hardware sequencers: eight tracks, each playing one of the rack's instruments; patterns of up to four bars that hold a whole section (drums, bass, pads and lead together); patterns chained into a song. Bars of a loaded song can be copied into it from the Song screen.

The instruments keep their own step sequencers (k303's, K909's, Kalliope's): those are part of their character. The rack sequencer plays them over MIDI like a keyboard would, so it works for every instrument, Krimson included, and isn't bound by their 16-step limit.

Related: [SHARED_FOUNDATIONS.MD](SHARED_FOUNDATIONS.MD) (rack, transport, MIDI routing), [SONG_PLAYER.md](SONG_PLAYER.md) (the Song screen it imports from).

## What it does

- **8 tracks.** Each plays one target: k303, K909, Kalliope or a Krimson slot (A–D), or nothing. A track can be muted. Targets and mutes are the sequencer's, not a pattern's.
- **16 patterns.** A pattern is up to **4 bars** (16 beats) long and holds every track's notes for that stretch.
  - **Notes** have a start, a length, a pitch and a velocity. A track can play chords (several notes at once). Notes are stored at 96 ticks per beat, so 16ths (24 ticks), 32nds (12), 16th triplets (16) and 8th triplets (32) all fall on whole ticks.
  - **Grid**: the step size used for editing and for snapping imported notes: 1/16, 1/32, 1/16T or 1/8T. Changing it never moves notes.
  - **Length**: any whole number of grid steps up to 4 bars (odd lengths make polymeters against the instruments' own sequencers).
- **Playing**: on the rack's transport (PLAY / STOP, tempo). The current pattern loops. Choosing another pattern while it plays switches at the end of the current one, in time; while stopped, at once. Notes go straight to their instrument (as the song player's do), whatever its MIDI channel or the keyboard's focus, and play as written (an instrument's OCTAVE is for the keys only).
- **RUN**: the sequencer has its own on/off; when off it stays silent while the rack plays.
- **Saved** in `~/.local/share/krate/sequencer.json`, written a moment after each change.

## Build plan

| # | Milestone | Done when |
| --- | --- | --- |
| Q1 ✅ | **Engine** (`shared/seq`, no Qt): the bank (16 patterns × 8 tracks of notes), the player as an `IEventSource` on the rack's transport (loops, cued pattern switches at the loop end, note-offs on stop, retarget and mute; nothing hangs), an event merger so the song player and the sequencer both play into the rack; the file (`sequencer.json`); wired into the krate app | `tst_racksequencer`: notes on the exact sample at several tempos and grids, loop wrap, cued switch, chords, offs on stop / mute / retarget / bank swap, merge order; file round trip. Result: `krate_seq` (`seq/Pattern.h`, `seq/Sequencer`), `core/EventMerge.h` (the krate app plays the song player and the sequencer through one `EventMerge<2>`), `apps/krate/SequencerFile`, `NoteTargets` (the instrument list shared with the Song screen); `tst_racksequencer` (8 tests), `tst_sequencerfile` (2) |
| Q2 | **Screen**: a SEQ tab beside MIXER and SONG: the 8 tracks (target, mute), pattern select / copy / clear, length and grid, RUN; a piano-roll editor for the selected track (add, move, resize, delete notes; velocity); the playhead | Edits play at once; screenshot; qmllint clean |
| Q3 | **Song import**: on the Song screen, select a track and a range of bars and copy them into a pattern and track (4 bars per pattern, longer ranges continue into the next patterns), confirming before anything is overwritten. Notes are fitted automatically and the result is reported (snapped to the grid; drum notes mapped to K909's voices; k303 tracks keep the top note of a chord) | `tst_song` import tests; a real song copied and played back |
| Q4 | **Chain**: a list of patterns (with repeats) that plays as a song; live pattern switching from a controller (learnable) | Chain plays through and loops; switching is learnable |
