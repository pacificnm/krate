# Kalliope: DSP

Realtime-safe throughout: no allocation, locks or I/O in `process()`; buffers are fixed-size (the engine renders in chunks of 128 frames). The values below are what the code does (`dsp/`); sounds are tuned by ear in P7.

## Signal flow (per voice)

```
            ┌─ VCO-1 (tri/saw/pulse/square) ◄── cross mod (VCO-2 → VCO-1 FM)
 pitch ─────┤                                ▲
 (note, bend,│                         sync ──┘ (VCO-2 hard-synced to VCO-1)
  LFO, ENV-1)└─ VCO-2 (sine/saw/pulse/noise)
                  │
          Source Mix ──► HPF (1-pole) ──► VCF (4-pole ladder, 12/24 dB) ──► VCA ──► voice out
                                            ▲ cutoff: knob + env (ENV-1|2) + LFO + key follow + velocity
                                                                               ▲ ENV-2 × level × LFO tremolo × velocity
```

Voices are summed (mono), then the chorus makes stereo, then the delay, then Volume and the output trim. The host's master stage soft-clips (as for every Krate instrument).

## Oversampling

Each voice runs at **2× the host rate**; the voices are summed at 2× and the sum goes through one half-band FIR decimator (`krate::HalfbandDecimator<31>`, from `shared/dsp/`, the same one k303 uses). Cross mod, sync and the resonant ladder all create content above Nyquist. `shared/dsp/` (header-only `krate_dsp`) also holds `krate::polyBlep`, now used by k303 and K909; both render byte-identically after the move.

## Building blocks

| Block | Design |
| --- | --- |
| **Oscillators** | `Oscillator`: phase accumulators where every jump (a saw's wrap, a pulse's edges, a sync reset) is corrected by a two-sample polynomial band-limited step of the jump's actual height at its exact sub-sample time (the sample before gets h·t²/2, the one after −h·(1−t)²/2). Saw, pulse (width 0.05–0.5 from PWM, its DC removed as on an AC-coupled circuit), square; triangle and sine are continuous (the triangle's corners alias little at 2×). **Sync**: VCO-2 resets when VCO-1 wraps, with the reset's jump corrected the same way. **Cross mod**: VCO-2's last output scales VCO-1's frequency by 1 ± 4 × depth (linear FM, clamped at 0) |
| **Ranges** | Footages are octaves: 16' = note −1 octave, 8' = as played, 4' = +1, 2' = +2. VCO-2 Low: a fixed 0.3 Hz at C2, tracking the keyboard, for slow sweeps and cross-mod effects |
| **Noise** | xorshift32 white noise per voice (VCO-2's fourth waveform) |
| **HPF** | 1-pole TPT high-pass, 0 (off) to 1 kHz |
| **VCF** | `Ladder`: 4 TPT one-pole stages, the feedback solved per sample (zero delay), tanh at the input; −24 dB from the 4th stage, −12 dB from the 2nd, feedback from the 4th; k = 4.2 × Resonance (self-oscillates near the top); input makeup √(1+k). Cutoff = knob + ENV (8 oct) × velocity + LFO (4 oct) + key follow, summed in octaves, smoothed (4 ms) and clamped under 0.42 × the 2× rate |
| **Envelopes** | `Envelope`: ADSR with exponential (RC-like) segments, 1 ms–10 s; the attack aims at 1.2 and stops at 1 (`attack` = time to full level); decay and release times are to −60 dB of the way. Env Key Follow shortens times by up to half (C2 → top of the keyboard). Retriggering starts from the current level (no click) |
| **LFO** | One per instrument (all voices share its phase, as on the original style of machine); per-voice delay ramp (linear fade-in over `lfo_delay`) so newly played notes start dry |
| **VCA** | ENV-2 × level² × (1 − tremolo) × velocity (1 − amount × (1 − velocity)), level smoothed over 4 ms; a 2 ms fade when the voice is stolen |

## Voices and allocation

8 voices, summed at 2× and decimated once. **Poly**: a note already sounding (or releasing) reuses its voice; else a silent voice; else the oldest released (or pedal-held) voice; else the oldest held one. A sounding voice that is taken fades out over 2 ms before its new note starts (no click). Each new Poly note starts its oscillators at a different (pseudo-random, deterministic) phase, like free-running analog oscillators, so a chord's notes don't all start on the same edge. **Unison**: all 8 voices on the newest note, detuned evenly from −12 to +12 cents, start phases spread, each at 1/√8 so the stack sits near one voice's level; last-note priority, legato (no retrigger while a key is held). **Solo**: one voice, last-note priority, legato. Changing Key Mode releases every note. Sustain (CC 64) keeps released notes sounding until the pedal lifts; CC 120 and 123 release everything.

## Effects

| Effect | Design |
| --- | --- |
| **Chorus** | `Chorus`: one short delay line read at two modulated taps (left and right, sine LFOs a quarter cycle apart), linear-interpolated, each mixed with the dry signal at equal parts × 0.707. I: 0.5 Hz, ±2 ms around 7 ms; II: 0.85 Hz, ±3 ms; Off: both sides dry. This is what makes Kalliope stereo |
| **Delay** | `Delay`: stereo ping-pong; the input (mid of the chorus output) enters the left line, each line feeds the other, so echoes alternate left, right, left…; feedback 0–90% through a one-pole low-pass at 4 kHz so repeats darken. Time = the `delay_time` note value (1/16 … 1/2) at the transport's tempo (the Tempo knob when there is none, as in `kalliope-render`), smoothed over 50 ms so tempo changes glide. The buffer (2 s at up to 192 kHz) is allocated once, when the engine is constructed |

## Levels and CPU

The engine applies Volume and a −3 dB output trim: single notes sit around −15 dBFS and a four-note pad peaks around −4 dBFS at the default levels, under the host's soft clip. Eight voices at full resonance stay bounded (tested). Budget: 8 voices, 2× oversampled, chorus and delay under 10% of a core at 48 kHz / 64 frames (`kalliope-render --bench`). P4 measures all 8 voices held with sync, cross mod and full resonance, chorus II and the delay at 4.0% (one voice: 0.55%). Silent voices are skipped.
