# Krimson: brief

2026-10-28

Krimson is a four-part multitimbral virtual-analog synthesizer for the [Krate](../../../README.md) suite, aimed at the sound of the classic late-90s red virtual-analog lead synth: bright, aggressive and agile. That means sync and FM leads, ring-mod bells, fat unison stacks, squelchy 12 dB basses, and the mod-wheel **morph** that sweeps a whole sound. It runs standalone (`krimson`) and as a device in the Krate rack.

Its panel follows the layout of the maker's current red analog-modeling synth: a red body and a black control panel laid out left to right:
- arpeggiator and voice mode;
- vibrato and LFO;
- mod envelope;
- oscillators;
- filter;
- amp envelope;
- effects;
- output.

Slot buttons A–D sit on the left. Every name, logo and graphic is our own: the panel reads "Krimson" and "Virtual Analog Synthesizer". The reference photo stays out of the repository (`reference/` is git-ignored).

Related docs: [DSP.md](DSP.md), [PLAY.md](PLAY.md) (slots, voice modes, morph, arpeggiator, programs), [UI.md](UI.md), [ROADMAP.md](ROADMAP.md). Suite design: [SHARED_FOUNDATIONS.MD](../../../docs/SHARED_FOUNDATIONS.MD).

## Scope (v1)

- **4 slots (A–D)**, each a complete sound: its own program, MIDI channel, level, pan and on/off. Slots can be played together (layered) from one keyboard, split across it, or each from its own channel.
- **16 voices**, shared by the slots as they need them.
- **Per voice:**
  - **OSC 1:** sine, triangle, saw or pulse (with pulse width).
  - **OSC 2:** triangle, saw, pulse or noise, with semitone and fine tune.
  - **Oscillator modulation:** sync (OSC 2 to OSC 1), FM (OSC 2 into OSC 1) and ring mod, plus the oscillator mix.
  - **Filter:** five types (LP 12, LP 24, band-pass, high-pass, notch + LP 12) with drive, resonance, envelope amount, velocity and keyboard tracking (0, ⅓, ⅔, 1).
  - **Envelopes:** filter ADSR and amp ADSR, plus an attack/decay mod envelope (to OSC 2 pitch, FM or pulse width).
  - **LFOs:** two. LFO 1 has 5 waves and a destination; LFO 2 is a triangle to amp, filter or OSC 2.
- **Voice modes:** poly, mono, legato, unison (detuned stack), each with glide.
- **Morph:** the mod wheel (or velocity) moves any continuous control from its panel value to a morph value set for it. One gesture sweeps a whole sound.
- **Arpeggiator** per slot on the transport: Up, Down, Up/Down, Random; 1–4 octaves; hold.
- **Effects** per slot: chorus/ensemble, a tempo-synced ping-pong delay and a reverb.
- **Memory:**
  - **Programs:** 8 banks × 8, with factory sounds in banks 1–2.
  - **Performances:** 8, each holding the four slots' programs, channels, levels, pans and layering.
- **MIDI:**
  - notes, velocity, pitch bend, mod wheel (morph or vibrato), sustain, and Program Change per slot;
  - MIDI Learn on the slot being edited;
  - in the rack, channels 3–6 by default (slots A–D).

Not in v1 (see [ROADMAP.md](ROADMAP.md#later)): more oscillator shapes (the newer model's wave tables), extra effect types (phaser, flanger, comb, ring), mutate/randomize, per-slot outputs.

## Parameters: one slot at a time

Each slot has the full sound below. Krate's `IInstrument` parameter table (at most 64 entries) shows **the slot being edited** (the panel's selected slot). Knobs, MIDI Learn mappings, rack follow-focus knobs and `panel.json` all act on that slot, as the hardware's knobs edit the selected slot. The engine keeps all four slots' values. Switching the edited slot changes what the table reports, and the panel follows.

| Section | Keys | Meaning |
| --- | --- | --- |
| Oscillators | `osc1_wave` (Sine, Triangle, Saw, Pulse), `osc2_wave` (Triangle, Saw, Pulse, Noise), `osc2_semi` (−24…+24), `osc2_fine` (±50 cents), `osc2_kbd` (On/Off: OSC 2 follows the keyboard), `pw` (pulse width 50…95 %), `osc_mix` (OSC 1 … OSC 2), `fm` (OSC 2 → OSC 1 amount), `sync` (Off/On), `ring` (Off/On: OSC 2 becomes OSC 1 × OSC 2) |
| Mod envelope | `menv_a`, `menv_d`, `menv_amt` (−/+), `menv_dest` (OSC 2 pitch, FM, Pulse width) |
| LFO 1 | `lfo1_rate`, `lfo1_wave` (Triangle, Saw, Square, Sample & hold, Soft random), `lfo1_amt`, `lfo1_dest` (OSC 1+2 pitch, OSC 2 pitch, Filter, Pulse width, FM) |
| LFO 2 | `lfo2_rate`, `lfo2_amt`, `lfo2_dest` (Amp, Filter, OSC 2 pitch) |
| Filter | `filter_type` (LP 12, LP 24, BP, HP, Notch + LP), `cutoff`, `res`, `drive`, `fenv_amt`, `fenv_vel`, `kbd_track` (0, ⅓, ⅔, 1) |
| Filter envelope | `fenv_a`, `fenv_d`, `fenv_s`, `fenv_r` |
| Amp envelope | `aenv_a`, `aenv_d`, `aenv_s`, `aenv_r`, `amp_vel`, `gain` |
| Voice | `voice_mode` (Poly, Mono, Legato, Unison), `unison_detune`, `glide`, `bend_range`, `wheel_dest` (Morph, Vibrato) |
| Arpeggiator | `arp_on`, `arp_mode`, `arp_range`, `arp_rate`, `arp_hold` |
| Effects | `chorus` (Off, Chorus, Ensemble), `delay_level`, `delay_time` (note values, synced), `delay_fb`, `reverb_level`, `reverb_size` |
| Slot | `slot_level`, `slot_pan` |
| Instrument | `volume`, `tempo` (standalone), `octave` |

About 60 entries. The exact list is fixed in C0, staying within 64.

## MIDI

| Message | Effect |
| --- | --- |
| Note on/off | Slots whose channel it is; notes on the keyboard channel go to the active slots (PLAY.md#slots), shifted by Octave |
| Pitch bend, CC 1 (mod wheel: morph or vibrato), CC 64 sustain | The slots that receive that channel |
| Program Change | That slot's program |
| Other CCs | MIDI Learn, on the edited slot |

## Registry entry

| Field | Value |
| --- | --- |
| id / name | `krimson` / `Krimson` |
| MIDI channels | 3–6 in the rack (A–D); the registry gains "extra channels" for this (C8) |
| QML module / panel | `Krimson` / `Panel` |
| actions | `slotA` … `slotD`, `bank1` … `bank8`, `program1` … `program8`, `store`, `arp`, `hold`, `morph`, `octave_down`, `octave_up` |

## Outputs

Stereo. Each slot is mixed at its level and pan, after its own effects.
