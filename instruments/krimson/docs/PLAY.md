# Krimson: slots, voice modes, morph, arpeggiator, programs

## Slots

Four slots, A–D, each a complete sound with its own program, MIDI channel (default 3–6 in the rack, 1–4 standalone), level, pan, and on/off.

| Action | How |
| --- | --- |
| Edit a slot | Press its slot button: the panel (and MIDI Learn) shows and edits that slot |
| Layer | Shift-press more slot buttons: the keys play every lit slot at once (and the panel edits them together) |
| Split | Set a split key (SHIFT + a key); slots below play from the lower part, the rest from the upper |
| Per-channel | A slot always plays notes on its own channel, whatever is lit (a sequencer track per slot) |

Notes on the **keyboard channel** (the LX49's, or anything in the rack that reaches Krimson by focus) go to the lit slots. Notes on a slot's own channel go to that slot.

## Voice modes (per slot)

Poly; Mono (last note, retrigger); Legato (last note, no retrigger, glide only between held notes); Unison (mono with a detuned stack). Glide time per slot.

## Morph

1. Hold **MORPH** (or SHIFT + wheel) and turn any knob: that knob's morph value is set (its LED ring shows the range).
2. Release MORPH: the wheel (or velocity, `wheel_dest`) now sweeps every morphed knob from panel value to morph value.
3. MORPH + CLEAR removes a slot's morph values.

## Arpeggiator (per slot)

As Kalliope's (PLAY.md in instruments/kalliope): Up, Down, Up/Down, Random; 1–4 octaves; 1/8, 1/16, 1/8T, 1/16T; hold; locked to the transport.

## Programs and performances

- **Programs:** 8 banks × 8, each a full slot sound including its morph values. Banks 1–2 are factory sounds aimed at the target character: sync leads, FM bells, ring-mod clangs, 12 dB acid and saw basses, unison supersaw-style leads, pads, plucks, and morph sweeps. Selecting a program loads it into the edited slot. STORE writes the edited slot to a program.
- **Performances:** 8, each the four slots' programs, channels, levels, pans, on/off and layering (PERF button, then 1–8).
- **Storage** (`~/.local/share/krimson/`): `programs.json` and `performances.json`, both stored by parameter key so they survive new parameters, as Kalliope's patches do. The live state of all four slots is in `~/.config/krimson/slots.json`, since `panel.json` holds only the edited slot.
