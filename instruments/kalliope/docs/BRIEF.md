# Kalliope: brief

2026-10-28

Kalliope is an eight-voice polyphonic synthesizer for the [Krate](../../../README.md) suite: two oscillators per voice, a high-pass and a resonant low-pass filter, two envelopes, an LFO, an arpeggiator on the shared transport, chorus and delay, and 64 patches. It runs standalone (`kalliope`) and as a device in the Krate rack.

It follows the classic early-80s flagship polysynth layout: a black panel of vertical sliders under orange section headers, rotary range and waveform selectors, a row of colored function buttons, a keyboard. Every name, logo and graphic is our own: the panel reads "Kalliope" and "Polyphonic Synthesizer". The reference screenshot stays out of the repository (`reference/` is git-ignored).

Related docs: [DSP.md](DSP.md) (voices, filters, effects), [PLAY.md](PLAY.md) (voice modes, arpeggiator, patches), [UI.md](UI.md), [ROADMAP.md](ROADMAP.md). Suite design: [SHARED_FOUNDATIONS.MD](../../../docs/SHARED_FOUNDATIONS.MD).

## Scope (v1)

- **8 voices**, each: VCO-1 and VCO-2 (cross-mod, sync, noise), source mix, HPF, 12/24 dB resonant low-pass, VCA, two ADSR envelopes; one LFO shared by all voices with a delay (fade-in) per note.
- **Key modes**: Poly (8 voices, oldest-note stealing), Unison (all 8 on one note, detuned), Solo (one voice, last-note priority, legato).
- **Arpeggiator**: Up, Down, Up & Down, Random; 1–4 octaves; 1/8, 1/16, 1/8T, 1/16T; Hold; locked to the transport (standalone or the rack's).
- **Effects**: stereo chorus (off, I, II), tempo-synced delay.
- **Patches**: 8 banks × 8 patches; a set of factory patches; WRITE stores the panel into a patch.
- **MIDI**: notes with velocity, pitch bend (±2 semitones), mod wheel (adds LFO to pitch), sustain pedal (CC 64), Program Change selects a patch (standalone); MIDI Learn for every slider, selector and button; in the rack, default channel 2.
- **On-screen keyboard**: two octaves with octave shift, playable with the mouse.

Not in v1 (see [ROADMAP.md](ROADMAP.md#later)): Dual and Split key modes (two patches at once), reverb, portamento, per-voice outputs, an aftertouch destination.

## Parameters

All parameters are normalized 0..1 in the `IInstrument` descriptor table; selectors and switches are detented. Keys are stable (panel, MIDI map, patches, rack sessions). A **patch** is every parameter except those marked *panel*.

| Section | Key | Label | Range / positions |
| --- | --- | --- | --- |
| LFO | `lfo_rate` | LFO Rate | 0.05–30 Hz, exponential |
| | `lfo_delay` | LFO Delay | 0–5 s fade-in after each note-on (Poly) or first note (Solo, Unison) |
| | `lfo_wave` | LFO Waveform | Sine, Up saw, Down saw, Square, Random (5) |
| VCO modulator | `mod_lfo` | VCO LFO Mod | 0–1 octave of LFO on pitch |
| | `mod_env` | VCO Env Mod | 0–2 octaves of ENV-1 on pitch |
| | `mod_dest` | Mod Destination | VCO-1, VCO-1 + 2, VCO-2 (3) |
| | `pwm` | Pulse Width / Mod | manual width 50–95%, or modulation depth |
| | `pwm_src` | PWM Source | LFO, Manual, ENV-1 (3) |
| VCO-1 | `cross_mod` | Cross Mod | VCO-2 → VCO-1 frequency modulation depth |
| | `vco1_range` | VCO-1 Range | 16', 8', 4', 2' (4) |
| | `vco1_wave` | VCO-1 Waveform | Triangle, Saw, Pulse, Square (4) |
| VCO-2 | `vco2_range` | VCO-2 Range | Low, 16', 8', 4', 2' (5; Low = sub-audio, about 0.3–30 Hz across the keyboard) |
| | `vco2_fine` | VCO-2 Fine Tune | ±50 cents |
| | `vco2_wave` | VCO-2 Waveform | Sine, Saw, Pulse, Noise (4) |
| | `sync` | Sync | Off, On (VCO-2 synced to VCO-1) (2) |
| Mixer | `mix` | Source Mix | VCO-1 … equal … VCO-2 |
| HPF | `hpf` | HPF Cutoff | off … 1 kHz, non-resonant |
| VCF | `cutoff` | VCF Cutoff | 20 Hz–20 kHz, exponential |
| | `res` | Resonance | 0 … self-oscillation |
| | `slope` | Slope | −12 dB, −24 dB (2) |
| | `vcf_env` | VCF Env Mod | 0–8 octaves |
| | `vcf_env_src` | VCF Env Source | ENV-1, ENV-2 (2) |
| | `vcf_lfo` | VCF LFO Mod | 0–4 octaves |
| | `key_follow` | Key Follow | 0–100% of the note's pitch on cutoff |
| VCA | `vca_level` | VCA Level | voice level |
| | `vca_lfo` | VCA LFO Mod | tremolo depth |
| ENV-1 | `env1_a`, `env1_d`, `env1_s`, `env1_r` | ENV-1 A/D/S/R | 1 ms–10 s (A, D, R, exponential); 0–100% (S) |
| ENV-2 | `env2_a`, `env2_d`, `env2_s`, `env2_r` | ENV-2 A/D/S/R | as ENV-1; ENV-2 always shapes the VCA |
| | `env_key` | Env Key Follow | Off, On: higher notes have shorter envelopes (2) |
| Voice | `key_mode` | Key Mode | Poly, Unison, Solo (3) |
| | `velocity` | Velocity | 0–100% of velocity on VCA and VCF env amount |
| | `bend_range` | Bend Range | 1–12 semitones (12) |
| Arpeggiator | `arp_on` | Arpeggio | Off, On (2) |
| | `arp_mode` | Arp Mode | Up, Down, Up & Down, Random (4) |
| | `arp_range` | Arp Range | 1–4 octaves (4) |
| | `arp_rate` | Arp Rate | 1/8, 1/16, 1/8T, 1/16T (4) |
| | `arp_hold` | Hold | Off, On (2) *panel* |
| Effects | `chorus` | Chorus | Off, I, II (3) |
| | `delay_level` | Delay Level | 0–100% wet |
| | `delay_time` | Delay Time | 1/16, 1/8T, 1/8, 1/8 dotted, 1/4, 1/4 dotted, 1/2 (7), synced to the tempo |
| | `delay_fb` | Delay Feedback | 0–90% |
| Master | `volume` | Volume | output level *panel* |
| | `tempo` | Tempo | 40–240 BPM; drives the standalone transport, unused in the rack (as k303 and K909) *panel* |

49 parameters, within `ParamValues`' limit of 64.

## MIDI

| Message | Effect |
| --- | --- |
| Note on/off | Play voices per Key Mode, or feed the arpeggiator; velocity scaled by `velocity` |
| Pitch bend | ±`bend_range` semitones on every voice |
| CC 1 (mod wheel) | Adds LFO vibrato to every voice's pitch, up to ±1 semitone at full, on top of VCO LFO Mod |
| CC 64 (sustain) | Holds released notes until the pedal lifts |
| Program Change n | Patch n (bank = n / 8, number = n % 8). In the rack, Program Change may be taken for focus switching first (the rack's setting) |
| Any other CC | MIDI Learn mappings |

## Registry entry

| Field | Value |
| --- | --- |
| id / name | `kalliope` / `Kalliope` |
| default MIDI channel | 1 (channel 2) |
| QML module / panel | `Kalliope` / `Panel` |
| actions | `bank1` … `bank8`, `patch1` … `patch8`, `arp`, `hold`, `write` |

## Outputs

Stereo: the voices are mono, spread across the stereo field by the chorus and the delay (and a slight fixed spread in Unison).
