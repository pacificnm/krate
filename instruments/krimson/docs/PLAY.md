# Krimson: slots, voice modes, morph, arpeggiator, programs

## Slots

Four slots, A–D, each a complete sound with its own MIDI channel (default 3–6 for A–D, standalone and in the rack), level, pan, on/off and keyboard zone. `SlotController` (the `Slots` QML singleton) runs them; the engine routes MIDI.

| Action | How |
| --- | --- |
| Play and edit one slot | Click its slot button: only that slot is lit (the keyboard plays it) and the panel and MIDI Learn edit it |
| Layer | Shift-click more slot buttons: every lit slot plays from the keyboard; the last one lit is edited. Shift-click a lit slot to take it out (one stays lit) |
| Split | A split key, and each slot's zone (whole, lower, upper): a lower slot plays only below the split, an upper one only from it up |
| Per-channel | A slot always plays notes on its own channel (if it is on), whatever is lit: a sequencer track or a second keyboard per slot |

Routing: a message on a channel that belongs to an on slot goes to that slot; any other channel is the **keyboard**, and goes to the lit slots (notes by zone when a split is set). Mod wheel, pitch bend, sustain and all-notes-off follow the same rule. A note-off goes to every slot holding that key, so changing the lit slots or the split while keys are down never leaves a note stuck. Each slot has its own held keys, sustain, bend, wheel, voice mode and glide.

**Storage**: `~/.config/krimson/slots.json` keeps all four slots' sounds (every slot parameter by key) and setup (channel, on, zone, lit), the split key and the edited slot; saved 2 s after any change and on quit. (`panel.json` holds only the edited slot.)

## Voice modes (per slot)

Poly; Mono (last note, retrigger); Legato (last note, no retrigger, glide only between held notes); Unison (mono with a detuned stack). Glide time per slot.

## Morph

1. Press **MORPH** (it turns blue; learnable as the `morph` action): every knob now shows and sets its **morph target**, starting from its current value. A target appears as a blue arc on the knob's ring, from its value to the target.
2. Press MORPH again: the knobs edit values again, and the morph source sweeps every knob with a target from its value to its target.
3. **Source**: the mod wheel when WHEEL is set to Morph; with WHEEL on Vibrato, each note's velocity (soft = the panel's sound, hard = the targets).
4. With MORPH lit, right-click a knob to clear its target; **CLEAR** clears the edited slot's targets.

Only continuous controls morph; waves, filter type, switches and lists don't. Each slot has its own targets; they are saved in `slots.json` with the slot (and in programs from C7).

## Arpeggiator (per slot)

As Kalliope's (PLAY.md in instruments/kalliope): Up, Down, Up/Down, Random; 1–4 octaves; 1/8, 1/16, 1/8T, 1/16T; hold; locked to the transport.

## Programs and performances

- **Programs**: 8 banks × 8 (`11`…`88`), each one slot's full sound **including its morph targets**. The display shows the edited slot's program, a dot while the slot differs from it, the name under it.
  - **Select**: ▲▼ (wrapping), or Program Change (on a slot's channel: that slot; on the keyboard channel: the lit slots), or the learnable `bank1`…`bank8` (program N1) and `program1`…`program8` (that number in the current bank) actions.
  - **Store**: STORE (it turns red), ▲▼ to choose the destination (it blinks; it starts on the current number), STORE again to write. An `Init` destination gets a name like `Program 35`.
- **Factory programs** (banks 1–2, aimed at the target character): 11 Sync Lead, 12 FM Bells, 13 Ring Clang, 14 Acid 12, 15 Unison Lead, 16 Saw Bass, 17 Morph Pad, 18 Pluck; 21 Strings, 22 Brass, 23 Notch Sweep, 24 Rave Stab, 25 Sub Bass, 26 Perc Organ, 27 Arp Pluck, 28 Noise Sweep. Several have morph targets (Sync Lead, FM Bells, Acid 12, Morph Pad, Brass): try the wheel. Banks 3–8 start as `Init`. Tuning by ear is C9.
- **First run** (no files yet): slots A–D start on programs 11–14.
- **Performances**: 8, each a snapshot of all four slots (sounds, morph targets, channel, on, zone, lit, the split, the edited slot and each slot's program number). PERF ◂ n ▸ chooses one (a dot after the number = empty), LOAD recalls it, SAVE writes the current state into it.
- **Storage** (`~/.local/share/krimson/`): `programs.json` (saved on every store and 2 s after a selection) and `performances.json` (on every save). Programs are stored by parameter key, so they survive new parameters, and values are clamped. Morph targets on stepped controls are dropped. The live state of all four slots is in `~/.config/krimson/slots.json` (`panel.json` only holds the edited slot).
