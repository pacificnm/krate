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
- **RUN** works like an instrument's RUN / STOP. Pressed while the rack is stopped, it starts the rack with the sequencer playing alone (the instruments' own sequencers stay off, so their patterns don't play over it). Pressed while it plays, it stops it, and the rack too when nothing else is running. Pressed while the rack plays without it, it joins. The rack's **PLAY** also plays it when RUN is on (and every instrument's own sequencer too).
- **Chain**: up to 64 steps, each a pattern and how many times it plays (1–16); in CHAIN mode the chain plays instead of one pattern looping.
- **Saved** in `~/.local/share/krate/sequencer.json`, written a moment after each change.

## The Seq screen

- **RUN** plays or stops the sequencer (see above); its light is on while it plays, dim while it waits for the rack's PLAY. **PATTERN 1–16**: click to choose; shaded ones have notes, a red dot marks the one playing. **COPY / PASTE** copies a whole pattern to another; **CLEAR** empties this one. **LENGTH −/+** in bars (by steps below a bar), up to 4 bars; **GRID** sets the step.
- **Tracks**: each row has its instrument (None, k303, K909, Kalliope, Krimson A–D), the number of notes it has in this pattern and **M** (mute). Click a row to edit it; **CLEAR TRACK** empties it in this pattern.
- **Piano roll** for the selected track: keys down the left (C labelled), steps across (bars and beats marked). A K909 track shows its 16 voices by name instead.
  - Click an empty place: a note of one step (velocity 100).
  - Drag a note to move it (it snaps to the grid); drag its right edge to make it longer or shorter.
  - Right-click or double-click a note to delete it.
  - Mouse wheel over a note, or drag its bar in the **velocity** lane: how hard it plays (for K909, 100 and up plays accented).
- The red line is the playhead (when the pattern shown is the one playing).

## Chains and playing live

- **The chain** (the strip above the piano roll) is a list of patterns, each played a number of times: **+ ADD** puts the pattern you're on at its end; click a step to edit its pattern; mouse wheel over a step sets how many times it plays (1–16); right-click removes it; **CLEAR** empties the chain.
- **CHAIN** on: the rack's PLAY plays the chain from its first step, through to the end and around again. The step playing has a red border (and which time through it is). Switched on while playing, the chain starts at the end of the current pass. With CHAIN off, the pattern you choose loops.
- **Live**: with CHAIN off, choosing a pattern while it plays switches at the end of the current one, in time. **◂ ▸** step through the patterns.
- **From a controller** (LEARN, then click the control, then press a button on the controller): RUN, CHAIN, ◂, ▸ and each of the 16 pattern buttons (rack actions `rack:seqrun`, `rack:seqchain`, `rack:seqprevious`, `rack:seqnext`, `rack:seqpattern1`–`16`).

## Copying bars from a song

On the Song screen, with a song open:

1. Click a track to view it, then **Shift-click** a bar in its tab (or notes) and **Shift-click** another: the bars between are selected (blue). **×** clears the selection.
2. **TO SEQ** opens the copy dialog: the bars (from the selection; edit them if you like), the pattern to start at, the sequencer track, and the instrument it plays (the song track's, to begin with). It picks the sequencer track already playing that instrument, or the first free one.
3. It says what will happen: how many patterns (4 bars each; 3/4 bars five to a pattern), and which patterns' notes on that track will be replaced. **COPY** does it and says what was fitted.

What's fitted, automatically:
- **Timing**: each pattern gets the grid that moves the fewest notes (1/16, else 16th triplets, else 1/32), and notes are snapped to it; the count of moved notes is reported.
- **Drums** going to K909 are mapped to its voices as the song player maps them; drum sounds it has no voice for are left out (counted).
- **k303** plays one note at a time: a chord keeps its top note (counted).
- **Repeats**: bars are numbered as written in the tab; a bar that plays more than once is taken from its first time through.
- Only that track of each pattern changes; the pattern takes the copied bars' length and grid. Patterns stop at 16.

## Build plan

| # | Milestone | Done when |
| --- | --- | --- |
| Q1 ✅ | **Engine** (`shared/seq`, no Qt): the bank (16 patterns × 8 tracks of notes), the player as an `IEventSource` on the rack's transport (loops, cued pattern switches at the loop end, note-offs on stop, retarget and mute; nothing hangs), an event merger so the song player and the sequencer both play into the rack; the file (`sequencer.json`); wired into the krate app | `tst_racksequencer`: notes on the exact sample at several tempos and grids, loop wrap, cued switch, chords, offs on stop / mute / retarget / bank swap, merge order; file round trip. Result: `krate_seq` (`seq/Pattern.h`, `seq/Sequencer`), `core/EventMerge.h` (the krate app plays the song player and the sequencer through one `EventMerge<2>`), `apps/krate/SequencerFile`, `NoteTargets` (the instrument list shared with the Song screen); `tst_racksequencer` (8 tests), `tst_sequencerfile` (2) |
| Q2 ✅ | **Screen**: a SEQ tab beside MIXER and SONG: the 8 tracks (target, mute), pattern select / copy / clear, length and grid, RUN; a piano-roll editor for the selected track (add, move, resize, delete notes; velocity); the playhead | Edits play at once; screenshot; qmllint clean. Result: `SequencerController` (QML singleton `Sequencer`: the GUI's copy of the bank, every edit hands the player a fresh copy, saved a second later; a newer version's file is left alone), `qml/SeqScreen.qml` (RUN, patterns 1–16 with used / chosen / playing marks, COPY / PASTE / CLEAR, LENGTH by bars (steps below a bar), GRID; the 8 tracks with instrument, note count and M; the piano roll: 128 keys or, for K909, its 16 voices by name; a drag previews and commits on release; the velocity lane; the playhead); the Seq tab (**Q**, learnable `rack:seq`, `"view": "seq"` in rack.json); the rack's tabs now fit the window. `tst_seqcontroller` |
| Q3 ✅ | **Song import**: on the Song screen, select a track and a range of bars and copy them into a pattern and track (4 bars per pattern, longer ranges continue into the next patterns), confirming before anything is overwritten. Notes are fitted automatically and the result is reported (snapped to the grid; drum notes mapped to K909's voices; k303 tracks keep the top note of a chord) | `tst_songimport`, `tst_seqcontroller`; a real song copied and played back. Result: `SongImport` (bars to spans: a written bar from the first time it plays, a MIDI file's bars by the tempo map; bars into patterns of up to 16 beats; the grid that moves fewest notes, 1/16 before 1/16T before 1/32; doubles merged; drums through the song player's kit map; k303's chords to their top note; a note held into a bar the song doesn't play next stops at its bar's end), `SequencerController::importChunks` (replaces one track's notes per pattern, sets length and grid, the other tracks stay), `SongController` (`writtenBarCount`, the selection, `planCopy`, `copyToSequencer` with its report), the Song screen (Shift-click bars, TO SEQ, the copy dialog), the tab view paints the selection. Measured: bars 31–34 of a real tab file, bass to k303 and drums to K909: 64 + 90 notes, none moved or dropped; played twice through the rack with the real engines, every note sent, peak −5.8 dBFS |
| Q4 ✅ | **Chain**: a list of patterns (with repeats) that plays as a song; live pattern switching from a controller (learnable) | Chain plays through and loops; switching is learnable. Result: the chain lives in the bank (`ChainStep{pattern, repeats}`, 64 steps, repeats 1–16) so it swaps with every edit; `Sequencer::setChainMode` (from the chain's first step on play; switched on while playing, from the next pass; an empty chain plays the cued pattern), `chainStep()` / `chainPass()` for the screen; `sequencer.json` keeps `chainMode` and `chain`; the Seq screen's chain strip (CHAIN, the steps, + ADD, CLEAR) and ◂ ▸ beside the patterns; learnable rack actions `rack:seqrun`, `rack:seqchain`, `rack:seqnext`, `rack:seqprevious`, `rack:seqpattern1`–`16`. `tst_racksequencer` (chain), `tst_sequencerfile`, `tst_seqcontroller` (chain edits) |
