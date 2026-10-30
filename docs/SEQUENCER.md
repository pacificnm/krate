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

## The Seq screen

- **RUN** switches the sequencer on or off (the rack's PLAY / STOP runs it). **PATTERN 1–16**: click to choose; shaded ones have notes, a red dot marks the one playing. **COPY / PASTE** copies a whole pattern to another; **CLEAR** empties this one. **LENGTH −/+** in bars (by steps below a bar), up to 4 bars; **GRID** sets the step.
- **Tracks**: each row has its instrument (None, k303, K909, Kalliope, Krimson A–D), the number of notes it has in this pattern and **M** (mute). Click a row to edit it; **CLEAR TRACK** empties it in this pattern.
- **Piano roll** for the selected track: keys down the left (C labelled), steps across (bars and beats marked). A K909 track shows its 16 voices by name instead.
  - Click an empty place: a note of one step (velocity 100).
  - Drag a note to move it (it snaps to the grid); drag its right edge to make it longer or shorter.
  - Right-click or double-click a note to delete it.
  - Mouse wheel over a note, or drag its bar in the **velocity** lane: how hard it plays (for K909, 100 and up plays accented).
- The red line is the playhead (when the pattern shown is the one playing).

## Build plan

| # | Milestone | Done when |
| --- | --- | --- |
| Q1 ✅ | **Engine** (`shared/seq`, no Qt): the bank (16 patterns × 8 tracks of notes), the player as an `IEventSource` on the rack's transport (loops, cued pattern switches at the loop end, note-offs on stop, retarget and mute; nothing hangs), an event merger so the song player and the sequencer both play into the rack; the file (`sequencer.json`); wired into the krate app | `tst_racksequencer`: notes on the exact sample at several tempos and grids, loop wrap, cued switch, chords, offs on stop / mute / retarget / bank swap, merge order; file round trip. Result: `krate_seq` (`seq/Pattern.h`, `seq/Sequencer`), `core/EventMerge.h` (the krate app plays the song player and the sequencer through one `EventMerge<2>`), `apps/krate/SequencerFile`, `NoteTargets` (the instrument list shared with the Song screen); `tst_racksequencer` (8 tests), `tst_sequencerfile` (2) |
| Q2 ✅ | **Screen**: a SEQ tab beside MIXER and SONG: the 8 tracks (target, mute), pattern select / copy / clear, length and grid, RUN; a piano-roll editor for the selected track (add, move, resize, delete notes; velocity); the playhead | Edits play at once; screenshot; qmllint clean. Result: `SequencerController` (QML singleton `Sequencer`: the GUI's copy of the bank, every edit hands the player a fresh copy, saved a second later; a newer version's file is left alone), `qml/SeqScreen.qml` (RUN, patterns 1–16 with used / chosen / playing marks, COPY / PASTE / CLEAR, LENGTH by bars (steps below a bar), GRID; the 8 tracks with instrument, note count and M; the piano roll: 128 keys or, for K909, its 16 voices by name; a drag previews and commits on release; the velocity lane; the playhead); the Seq tab (**Q**, learnable `rack:seq`, `"view": "seq"` in rack.json); the rack's tabs now fit the window. `tst_seqcontroller` |
| Q3 | **Song import**: on the Song screen, select a track and a range of bars and copy them into a pattern and track (4 bars per pattern, longer ranges continue into the next patterns), confirming before anything is overwritten. Notes are fitted automatically and the result is reported (snapped to the grid; drum notes mapped to K909's voices; k303 tracks keep the top note of a chord) | `tst_song` import tests; a real song copied and played back |
| Q4 | **Chain**: a list of patterns (with repeats) that plays as a song; live pattern switching from a controller (learnable) | Chain plays through and loops; switching is learnable |
