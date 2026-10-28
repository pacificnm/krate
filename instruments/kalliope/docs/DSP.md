# Kalliope: DSP

Realtime-safe throughout: no allocation, locks or I/O in `process()`; every buffer sized in `prepare()`. Values below are the design starting points; this page follows the code once it exists.

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

Each voice runs at **2× the host rate** and is decimated by a half-band FIR: cross-mod, sync and the resonant ladder all create content above Nyquist. The half-band decimator and the PolyBLEP oscillator come from k303 and move to `shared/dsp/` (a new `krate_dsp` library) in P1; k303 keeps byte-identical renders.

## Building blocks

| Block | Design |
| --- | --- |
| **Oscillators** | Phase accumulators with PolyBLEP-corrected saw, pulse (variable width) and square; triangle by leaky integration of the band-limited square; sine for VCO-2. **Sync**: VCO-2's phase resets when VCO-1 wraps, with a PolyBLEP step at the reset sized by the jump. **Cross mod**: VCO-2's output scales VCO-1's frequency (linear FM through zero, clamped) |
| **Ranges** | Footages are octaves: 16' = note −1 octave, 8' = as played, 4' = +1, 2' = +2. VCO-2 Low: a fixed 0.3 Hz at C2, tracking the keyboard, for slow sweeps and cross-mod effects |
| **Noise** | xorshift32 white noise per voice (VCO-2's fourth waveform) |
| **HPF** | 1-pole TPT high-pass, 0 (off) to 1 kHz |
| **VCF** | 4-pole transistor-ladder model, zero-delay feedback (TPT) with a tanh at the input; −24 dB from the 4th stage, −12 dB from the 2nd. Resonance compensates passband loss (as k303's ladder does) and self-oscillates at the top. Cutoff modulation is summed in octaves and converted once per sample |
| **Envelopes** | ADSR with exponential segments (RC-like), times 1 ms–10 s; attack aims at 1.2 and clips at 1 for the classic curve. Env Key Follow shortens times by up to half across the keyboard |
| **LFO** | One per instrument (all voices share its phase, as on the original style of machine); per-voice delay ramp (linear fade-in over `lfo_delay`) so newly played notes start dry |
| **VCA** | ENV-2 × level × (1 − tremolo) × velocity curve; a 2 ms declick fade when a voice is stolen |

## Voices and allocation

8 voices. **Poly**: a free voice if any; otherwise the oldest released voice; otherwise the oldest held voice (with the declick fade). **Unison**: all 8 voices on the newest note, detuned ±12 cents across them, last-note priority, legato (no retrigger while a key is held). **Solo**: one voice, last-note priority, legato. Sustain (CC 64) keeps released notes sounding until the pedal lifts.

## Effects

| Effect | Design |
| --- | --- |
| **Chorus** | Two modulated delay lines (one per side, LFOs in quadrature), linear-interpolated. I: 0.5 Hz, ±2 ms around 7 ms; II: 0.85 Hz, ±3 ms. Wet and dry mixed equally |
| **Delay** | Stereo ping-pong, delay time from the transport's tempo and `delay_time`'s note value; feedback through a gentle low-pass (4 kHz) so repeats darken; buffer sized in `prepare()` for 2 s at the sample rate |

## Levels and CPU

Eight voices at full resonance must stay bounded (tests check peaks with every voice playing). Budget: 8 voices, 2× oversampled, chorus and delay under 10% of a core at 48 kHz / 64 frames (`kalliope-render --bench`). Silent voices are skipped.
