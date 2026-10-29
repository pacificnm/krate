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

- **Programs:** 8 banks × 8, each a full slot sound including its morph values. Banks 1–2 are factory sounds aimed at the target character: sync leads, FM bells, ring-mod clangs, 12 dB acid and saw basses, unison supersaw-style leads, pads, plucks, and morph sweeps. Selecting a program loads it into the edited slot. STORE writes the edited slot to a program.
- **Performances:** 8, each the four slots' programs, channels, levels, pans, on/off and layering (PERF button, then 1–8).
- **Storage** (`~/.local/share/krimson/`): `programs.json` and `performances.json`, both stored by parameter key so they survive new parameters, as Kalliope's patches do. The live state of all four slots is in `~/.config/krimson/slots.json`, since `panel.json` holds only the edited slot.
