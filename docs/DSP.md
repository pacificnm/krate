# DSP

One monophonic voice: oscillator → diode-ladder lowpass → VCA, shaped by a filter envelope, an amp envelope and the accent circuit. All numbers marked *starting value* are approximations of the original and are tuned by ear against reference recordings.

```
 pitch (note + octave + tuning, slide glide)
        │
   ┌────▼─────┐   ┌──────────────────────────┐   ┌─────┐   ┌────────┐
   │ Osc      │──►│ Diode ladder (4x OS)     │──►│ VCA │──►│ Volume │──► out_L/out_R
   │ saw / sq │   │ cutoff ← base + envmod·  │   └──▲──┘   └────────┘
   └──────────┘   │   MEG + accent sweep     │      │
                  └───────▲──────────────────┘      │
                     MEG (decay knob)          VEG (fixed) + accent gain
```

## Parameters

UI and MIDI values are normalized 0–1. Each parameter maps to engine units with the curve below. All are smoothed with a one-pole lowpass (~10 ms, *starting value*) per sample to avoid zipper noise.

| Id | Panel label | Range | Curve | Default |
| --- | --- | --- | --- | --- |
| `tuning` | Tuning | −12 … +12 semitones | linear | 0 |
| `cutoff` | Cut Off Freq | ~60 Hz … ~5 kHz base cutoff (*starting value*) | exponential | 0.5 |
| `resonance` | Resonance | 0 … just below self-oscillation | linear, slight ease-in | 0.5 |
| `envmod` | Env Mod | 0 … ~+4 octaves of filter sweep (*starting value*) | linear in octaves | 0.5 |
| `decay` | Decay | MEG decay ~200 ms … ~2 s (*starting value*) | exponential | 0.5 |
| `accent` | Accent | accent depth 0 … 1 | linear | 0.5 |
| `waveform` | Waveform | saw / square | switch | saw |
| `volume` | Volume | −inf … 0 dB | audio taper: gain = v³ (0.8 → −5.8 dB) | 0.8 |
| `tempo` | Tempo | 40 … 240 BPM | linear | 120 |

Ids are stable strings: they are the keys in `midimap.json`. The mappings live in `src/dsp/Params.h` (`k303::units`); keep that file and this table in step.

## Oscillator

- Phase accumulator **at the oversampled rate (4x)**, band-limited with **PolyBLEP** on the saw reset (and both edges for square). Running it at 4x is cheaper and cleaner than upsampling a 1x oscillator.
- Square: classic analog bass synths often derive the square from the saw through a transistor shaper, giving a slightly uneven pulse. v1 uses a PolyBLEP square; an optional shaper (saw → tanh-driven comparator) is a later refinement.
- Pitch: `f = 440 · 2^((midiNote − 69 + tuning) / 12)`; step note mapping is in [SEQUENCER.md](SEQUENCER.md).

## Slide

When a slide is active, pitch moves exponentially (one-pole in the log-frequency domain) toward the new note, time constant ~60 ms (*starting value*, fixed like the hardware, not a user control). Slide never retriggers either envelope.

## Filter: diode ladder

The core of the sound. Requirements:

1. **Topology:** four-stage diode ladder lowpass with global feedback (resonance), solved as a **zero-delay-feedback (ZDF / TPT)** structure so cutoff and resonance can be modulated per sample without instability.
2. **Nonlinearity:** `tanh` saturation in the stages and the feedback path. v1 uses the cheaper approach: `tanh(1.5·x)` on the input of a linear ZDF solve. If it lacks the bite at high resonance, upgrade to an iterative (Newton-Raphson, 1–3 iterations) solve of the nonlinear system.
3. **Oversampling:** the oscillator and filter run at **4x** (`Voice::kOversample`). Two 31-tap Blackman-windowed half-band FIR stages decimate back to the base rate (flat to ~15 kHz at 48 kHz, stopband > 70 dB down). Filter coefficients update once per base-rate sample.
4. **Character:** a diode ladder rolls off more gently than a transistor ladder (often described as ~18 dB/oct) and its resonance stays below strong self-oscillation. Clamp feedback so the maximum knob position is loud and squelchy but stable.
5. **Bass loss at high resonance:** the ladder's passband gain is 1 / (1 + k) (−24 dB at k = 15). Makeup gain of √(1 + k) restores half of that in dB, so high resonance still thins the bass, as intended, without collapsing the level.
6. **Input coupling:** a gentle highpass (~10 Hz, *starting value*) before the filter removes DC from the square.
7. **Cutoff tuning:** the ladder's resonant peak sits at ~0.67 × its coefficient frequency, so the coefficient frequency is 1.5 × the Cutoff value: the knob names where the resonance sings.
8. **Feedback range:** k = 15 × `resonance`; this structure self-oscillates at k = 17.

References to work from:
- Vadim Zavalishin, *The Art of VA Filter Design* (free PDF): TPT/ZDF method and the diode ladder.
- Will Pirkle's virtual-analog diode ladder filter (published application note and his synth-design book): the stage-coupling structure implemented in `src/dsp/DiodeLadder.cpp`.
- Open-source diode-ladder filter models as a sound reference. Read for understanding; do not copy code without checking its license.
- The brief cites Stilson/Smith ladder analysis; that is transistor-ladder groundwork and useful background.

Acceptance: at max resonance across the whole cutoff range and at 44.1/48/96 kHz, a 10-minute render has no NaN/Inf and peak output stays below +6 dBFS before the volume stage.

## Envelopes

| Envelope | Drives | Shape |
| --- | --- | --- |
| MEG (main) | Filter cutoff via `envmod` | Instant attack, exponential decay set by `decay` (time constant = decay / 3, so ~5% remains after the knob's time). Retriggers on every new (non-slid, non-tied) note |
| VEG (volume) | VCA | Linear 3 ms attack (from the current level, so retriggers don't click), exponential decay τ = 1 s while the gate is held, release τ = 3 ms on gate off (*starting values*) |

Envelopes are exponential one-pole segments computed per sample.

## Accent

On an accented note:

1. **MEG decay forced short:** MEG uses the minimum decay (~200 ms) regardless of the `decay` knob.
2. **Louder:** VCA gain raised by up to ~+6 dB × `accent` (*starting value*).
3. **Accent sweep:** an extra filter-cutoff push from a capacitor-like smoother driven by `MEG × accent`: it charges with τ = 30 ms and discharges with τ = 150 ms. Depth: up to 2 octaves × (0.5 + `resonance`). Consecutive accents stack and drive the filter higher. Depth scales with `accent` and with `resonance`, as on the hardware. This stacking is what makes accented runs "scream"; treat it as a required feature, not an option.

## Output

Mono voice copied to `out_L` and `out_R`. A final soft clip (`tanh`) after the volume stage prevents harsh digital overs.

## Performance budget

Target < 5% of one core at 48 kHz / 64-frame buffers with 4x oversampling on the dev machine. Measure with `k303-render --bench`.

Measured (M3, dev machine, RelWithDebInfo): **1.0%** at 48 kHz, **2.0%** at 96 kHz.
