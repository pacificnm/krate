# Kobalt: brief

2026-10-30

Kobalt is a monophonic synthesizer for the [Krate](../../../README.md) suite: one oscillator with saw, pulse and a sub-oscillator mixed with noise, a self-oscillating four-pole low-pass filter, one envelope, an LFO, portamento, an arpeggiator and a step sequencer on the shared transport, delay and reverb, and 64 patches. It runs standalone (`kobalt`) and as a device in the Krate rack.

It follows the classic early-80s one-oscillator monosynth layout: a slim panel of vertical sliders grouped under section headers, a source mixer, small switches for the waveforms and modes, a bender and modulation lever, and a sequencer you load from the keys. Every name, logo and graphic is our own: the panel reads "Kobalt" and "Monophonic Synthesizer", on a cobalt-blue case. Reference material stays out of the repository (`reference/` is git-ignored).

Related docs: [MANUAL.md](MANUAL.md) (the player's manual), [ROADMAP.md](ROADMAP.md). Suite design: [SHARED_FOUNDATIONS.MD](../../../docs/SHARED_FOUNDATIONS.MD).

## Scope (v1)

- **One voice**, running at twice the host rate: the oscillator (saw, pulse with manual / LFO / envelope width modulation, a sub-oscillator one or two octaves down or a narrow pulse two octaves down, white noise) into the **source mixer** (a level for each), then the low-pass filter, then the VCA.
- **Filter**: four cascaded one-pole stages with a soft limit in every stage (as a chain of transconductance amplifiers saturates), resonance up to self-oscillation, cutoff modulated by the envelope, the LFO, the keys, and the bender.
- **Envelope**: ADSR, triggered by **Gate + Trig** (every new key restarts it), **Gate** (legato playing doesn't restart it) or **LFO** (the LFO restarts it every cycle while a key is down). The VCA follows the envelope or just the **Gate**.
- **LFO**: triangle, square, random (sample and hold), noise; on the oscillator's pitch, the pulse width, the cutoff.
- **Portamento**: Off, Auto (glides only between overlapping notes), On (always), with its time.
- **Keys**: one note at a time, last-note priority; a released key goes back to a key still held.
- **Bender and lever**: pitch bend to the oscillator (0–12 semitones) and the filter; the mod wheel adds LFO vibrato.
- **Analog**: the oscillator drifts slowly, each note sits slightly off in tuning and cutoff, as the original's parts do.
- **Arpeggiator**: Up, Up & Down, Down; 1–3 octaves; 1/8, 1/16, 1/8T, 1/16T; Hold; on the transport.
- **Step sequencer**: 8 patterns of up to 64 steps (notes, rests, ties, legato), loaded from the keys a note at a time, transposed from the keys while it plays.
- **Effects**: tempo-synced ping-pong delay, reverb.
- **Patches**: 8 banks × 8 patches; 16 factory sounds; WRITE stores the panel into a patch.
- **MIDI**: notes with velocity, pitch bend, mod wheel, sustain pedal, Program Change selects a patch; MIDI Learn for every control and button; in the rack, default channel 7.
- **On-screen keyboard**: played with the mouse, key for key with the controller's octave.

## Parameters

All parameters are normalized 0..1 in the `IInstrument` descriptor table; selectors and switches are detented. Keys are stable (panel, MIDI map, patches, rack sessions). A **patch** is every parameter except those marked *panel*.

| Section | Key | Label | Range / positions |
| --- | --- | --- | --- |
| LFO | `lfo_rate` | LFO Rate | 0.1–30 Hz, exponential |
| | `lfo_wave` | LFO Waveform | Triangle, Square, Random, Noise (4) |
| VCO | `vco_mod` | VCO Mod | LFO on pitch, 0 … 1 octave (squared: fine vibrato low on the slider) |
| | `vco_range` | Range | 16', 8', 4', 2' (4) |
| | `pw` | Pulse Width | the pulse's width 50 % … 5 %, or how far the source moves it |
| | `pwm_src` | PWM Source | LFO, Manual, Env (3) |
| Source mixer | `pulse_level` | Pulse | 0–100 % |
| | `saw_level` | Saw | 0–100 % |
| | `sub_level` | Sub | 0–100 % |
| | `sub_type` | Sub Type | 1 Oct, 2 Oct, 2 Oct Pulse (3) |
| | `noise_level` | Noise | 0–100 % |
| VCF | `cutoff` | Cutoff | 20 Hz–20 kHz, exponential |
| | `res` | Resonance | 0 … self-oscillation |
| | `vcf_env` | Env | 0–8 octaves of the envelope |
| | `vcf_mod` | Mod | 0–4 octaves of the LFO |
| | `key_follow` | Keyboard | 0–100 % of the note's pitch |
| VCA | `vca_mode` | VCA Mode | Env, Gate (2) |
| ENV | `env_a`, `env_d`, `env_s`, `env_r` | Attack / Decay / Sustain / Release | 1 ms–10 s (A, D, R, exponential); 0–100 % (S) |
| | `env_trig` | Env Trigger | Gate + Trig, Gate, LFO (3) |
| Portamento | `porta_time` | Portamento | 0 … 2 s |
| | `porta_mode` | Portamento Mode | Off, Auto, On (3) |
| Bender | `bend_vco` | Bend VCO | 0–12 semitones (13) |
| | `bend_vcf` | Bend VCF | 0–4 octaves of the bender on the cutoff |
| | `lever_mod` | Lever Mod | the mod wheel's vibrato, 0 … ±2 semitones |
| Character | `analog` | Analog | 0–100 % drift and note-to-note spread |
| | `velocity` | Velocity | 0–100 % of velocity on the VCA and the filter envelope |
| Play | `play_mode` | Play Mode | Note, Arpeggio, Sequencer (3) |
| | `arp_mode` | Arp Mode | Up, Up & Down, Down (3) |
| | `arp_range` | Arp Range | 1–3 octaves (3) |
| | `arp_rate` | Rate | 1/8, 1/16, 1/8T, 1/16T (4), for the arpeggiator and the sequencer |
| | `arp_hold` | Hold | Off, On (2) *panel* |
| | `gen_density` | Density | 0–100 %: how many steps the pattern generator fills *panel* |
| Effects | `delay_level` | Delay Level | 0–100 % wet |
| | `delay_time` | Delay Time | 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2 (7), synced to the tempo |
| | `delay_fb` | Delay Feedback | 0–90 % |
| | `reverb_level` | Reverb Level | 0–100 % wet |
| | `reverb_size` | Reverb Size | decay 0.4–8 s |
| Keys | `octave` | Octave | −3 … +3 octaves on the player's keys (7) *panel* |
| Master | `volume` | Volume | output level *panel* |
| | `tempo` | Tempo | 40–240 BPM; drives the standalone transport, unused in the rack *panel* |

43 parameters, within `ParamValues`' limit of 64.

## Sound engine

- **Oscillator**: one phase drives the saw and the pulse (two band-limited `krate::Oscillator`s advanced together, so they never drift apart), and the sub-oscillators at a half and a quarter of the increment (a square one octave down; a square, or a 25 % pulse, two octaves down), so their edges fall on the main wave's. Free-running: a new note doesn't reset the phase. Noise is white.
- **Mixer**: each source times its level, summed, into the filter at half scale (all four up overdrives the filter's first stage, as on the original).
- **Filter**: four one-pole stages (TPT, 2× rate) with the feedback solved per sample; each stage's input goes through a soft limit, and so does the feedback, so a loud resonant note rounds off instead of growing. Part of the passband lost to resonance is made up.
- **Envelope**: `krate::Envelope` (exponential segments). Gate mode on the VCA: a 1.5 ms ramp on and off instead.
- **Portamento**: the pitch follows the played note through a one-pole glide in semitones, the time being how long it takes to get 99 % of the way.
- **Output**: mono; the delay spreads it across the stereo field, then the reverb, then Volume.

## Arpeggiator

Play Mode ARP: the held keys (or, with Hold, the last chord) play one at a time, sorted low to high, Up, Up & Down (the top and bottom notes not repeated) or Down, across 1–3 octaves, at the Rate, each note for half a step, on the transport. Stopped, nothing sounds; leaving ARP, a key still held sounds again. Standalone, ARP starts the transport (and leaving it stops what it started); in the rack, the transport bar runs it.

## Step sequencer

Play Mode SEQ: 8 patterns of up to 64 steps, each step a **note**, a **rest** or a **tie** (the note before goes on), and any note or tie can be **legato**: it runs straight into the next step's note, with no gap, so the envelope goes on with Env Trigger Gate and Portamento Auto glides. Steps play at the Rate, a note for half a step unless the next step is a tie or it is legato.

- **Playing**: with the transport running (and Kobalt's RUN on, in the rack), the pattern plays; joining a running transport starts on the next bar. The keys transpose it by their distance from middle C, and don't sound themselves. Stopped, the keys play as usual.
- **Loading**: click a step on the grid (it blinks), then play notes one at a time: each goes into that step and the next is picked, wrapping at the pattern's length. A key going down while the one before is still held makes the step before legato. **REST** and **TIE** fill the step; **LEGATO** turns legato on or off for the step just loaded; **CLEAR** empties the pattern (16 rests). Shift-click a step to make it the last. Click the blinking step again to stop loading.
- **Generating**: **GEN** writes a new random pattern into the current one, at its length: notes from the held keys, else the pattern's own notes, else C minor pentatonic from C2, spread over Arp Range's octaves, moving mostly by small steps with the odd leap; **DENSITY** is the share of steps that play (the first always does). About one playing step in eight that follows a note is a tie, and a quarter of the notes that follow a note have the one before slide into them (legato). Every press makes a different one.
- **Patterns**: the arrows next to the display pick P1–P8.
- **Storage**: `~/.local/share/kobalt/sequences.json`, saved 2 s after an edit: each pattern's length and steps, a step `null` (rest), `{"note": n}` or `{"tie": true}`, with `"legato": true` when it runs on.

## MIDI

| Message | Effect |
| --- | --- |
| Note on/off | One note at a time, last-note priority; or feed the arpeggiator or transpose the sequencer; shifted by Octave (each key keeps the shift it went down with, a song's notes play as written) |
| Pitch bend | ±`bend_vco` semitones, ±`bend_vcf` octaves on the cutoff |
| CC 1 (mod wheel) | LFO vibrato, up to `lever_mod` |
| CC 64 (sustain) | Holds the note after the keys are released |
| CC 120, 123 | All sound / notes off |
| Program Change n | Patch n (bank n / 8, number n % 8) |
| Any other CC | MIDI Learn mappings |

## Registry entry

| Field | Value |
| --- | --- |
| id / name | `kobalt` / `Kobalt` |
| default MIDI channel | 6 (channel 7) |
| QML module / panel | `Kobalt` / `Panel` |
| actions | `bank1` … `bank8`, `patch1` … `patch8`, `note`, `arp`, `seq`, `hold`, `write`, `start`, `rest`, `tie`, `legato`, `generate`, `octave_down`, `octave_up` |

## Outputs

Stereo: the voice is mono, spread by the ping-pong delay and the reverb.
