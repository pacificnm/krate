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

Voices are summed (mono), then the chorus makes stereo, then the delay, then Volume. The host's master stage soft-clips (as for every Krate instrument).

## Oversampling

Each voice runs at **2× the host rate**; the voices are summed at 2× and the sum goes through one half-band FIR decimator (`krate::HalfbandDecimator<31>`, from `shared/dsp/`, the same one k303 uses). Cross mod, sync and the resonant ladder all create content above Nyquist. `shared/dsp/` (header-only `krate_dsp`) also holds `krate::polyBlep`, now used by k303 and K909; both render byte-identically after the move.

## Building blocks

| Block | Design |
| --- | --- |
| **Oscillators** | `Oscillator`: phase accumulators where every jump (a saw's wrap, a pulse's edges, a sync reset) is corrected by a two-sample polynomial band-limited step of the jump's actual height at its exact sub-sample time (the sample before gets h·t²/2, the one after −h·(1−t)²/2). Saw, pulse (width 0.05–0.5 from PWM), square; triangle and sine are continuous (the triangle's corners alias little at 2×). **Sync**: VCO-2 resets when VCO-1 wraps, with the reset's jump corrected the same way. **Cross mod**: VCO-2's last output scales VCO-1's frequency by 1 ± 4 × depth (linear FM, clamped at 0) |
| **Ranges** | Footages are octaves: 16' = note −1 octave, 8' = as played, 4' = +1, 2' = +2. VCO-2 Low: a fixed 0.3 Hz at C2, tracking the keyboard, for slow sweeps and cross-mod effects |
| **Noise** | xorshift32 white noise per voice (VCO-2's fourth waveform) |
| **HPF** | 1-pole TPT high-pass, 0 (off) to 1 kHz |
| **VCF** | `Ladder`: 4 TPT one-pole stages, the feedback solved per sample (zero delay), tanh at the input; −24 dB from the 4th stage, −12 dB from the 2nd, feedback from the 4th; k = 4.2 × Resonance (self-oscillates near the top); input makeup √(1+k). Cutoff = knob + ENV (8 oct) × velocity + LFO (4 oct) + key follow, summed in octaves, smoothed (4 ms) and clamped under 0.42 × the 2× rate |
| **Envelopes** | `Envelope`: ADSR with exponential (RC-like) segments, 1 ms–10 s; the attack aims at 1.2 and stops at 1 (`attack` = time to full level); decay and release times are to −60 dB of the way. Env Key Follow shortens times by up to half (C2 → top of the keyboard). Retriggering starts from the current level (no click) |
| **LFO** | One per instrument (all voices share its phase, as on the original style of machine); per-voice delay ramp (linear fade-in over `lfo_delay`) so newly played notes start dry |
| **VCA** | ENV-2 × level² × (1 − tremolo) × velocity (1 − amount × (1 − velocity)), level smoothed over 4 ms. Stealing declick arrives with polyphony (P2) |

## Voices and allocation

8 voices. **Poly**: a free voice if any; otherwise the oldest released voice; otherwise the oldest held voice (with the declick fade). **Unison**: all 8 voices on the newest note, detuned ±12 cents across them, last-note priority, legato (no retrigger while a key is held). **Solo**: one voice, last-note priority, legato. Sustain (CC 64) keeps released notes sounding until the pedal lifts.

## Effects

| Effect | Design |
| --- | --- |
| **Chorus** | Two modulated delay lines (one per side, LFOs in quadrature), linear-interpolated. I: 0.5 Hz, ±2 ms around 7 ms; II: 0.85 Hz, ±3 ms. Wet and dry mixed equally |
| **Delay** | Stereo ping-pong, delay time from the transport's tempo and `delay_time`'s note value; feedback through a gentle low-pass (4 kHz) so repeats darken; buffer sized in `prepare()` for 2 s at the sample rate |

## Levels and CPU

Eight voices at full resonance must stay bounded (tests check peaks with every voice playing). Budget: 8 voices, 2× oversampled, chorus and delay under 10% of a core at 48 kHz / 64 frames (`kalliope-render --bench`). P1 measures one voice (sync, cross mod, full resonance) at 0.55%. Silent voices are skipped.
