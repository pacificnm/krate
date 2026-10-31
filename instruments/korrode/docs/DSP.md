# DSP

One monophonic voice: oscillator → diode-ladder lowpass → VCA, shaped by a filter envelope, an amp envelope and the accent circuit; then the output stage (drive and tone, delay, reverb, width, volume). All numbers marked *starting value* are approximations of the original and are tuned by ear against reference recordings.

```
 pitch (note + octave + tuning, slide glide)
        │
   ┌────▼─────┐   ┌──────────────────────────┐   ┌─────┐   ┌─────────────┐   ┌──────────────┐   ┌────────┐
   │ Osc      │──►│ Diode ladder (4x OS)     │──►│ VCA │──►│ Drive, Tone │──►│ Delay, Reverb│──►│ Volume │──► out_L/out_R
   │ saw / sq │   │ cutoff ← base + envmod·  │   └──▲──┘   └─────────────┘   │ (Width: wet) │   └────────┘
   └──────────┘   │   MEG + accent sweep     │      │                        └──────────────┘
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
| `scale` | Scale | 1/16, 1/32, 1/16T (4, 8, 6 steps per beat) | 3 detents | 1/16 |
| `playmode` | Play Mode | Forward, Reverse, Fwd+Rev, Invert, Random | 5 detents | Forward |
| `sweep` | Sweep | accent sweep 0 … 4 octaves at full Accent (× (0.5 + resonance)) | linear | 0.5 (2 octaves, as before) |
| `drive` | Drive | 0 (bypassed) … +28 dB into a tanh after the VCA | gain 1 + 24 v² | 0 |
| `tone` | Tone | low-pass after the drive, 800 Hz … 16 kHz (only with Drive up) | exponential | 0.6 (~4.8 kHz) |
| `delay_level` | Delay | 0 (off) … 100 % wet | linear | 0 |
| `delay_time` | Time | 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2 at the tempo | 7 detents | 1/8. |
| `delay_fb` | Feedback | 0 … 90 % | linear | 31.5 % |
| `reverb_level` | Reverb | 0 (off) … 100 % wet | linear | 0 |
| `reverb_size` | Size | decay 0.4 … 8 s | exponential | 0.4 (1.3 s) |
| `width` | Width | the effects' stereo spread: 0 mono, 0.5 as they are, 1 double the side | linear | 0.5 |

Ids are stable strings: they are the keys in `midimap.json` and `panel.json`. All positions are saved to `~/.config/korrode/panel.json` 2 s after a change and on quit, and restored at startup. The mappings live in `src/dsp/Params.h` (`korrode::units`), and each parameter's `ParamDescriptor` there carries its key, label, default, detent count and display-text function (`src/dsp/Params.cpp`, `korrode::format`); keep that file and this table in step. MIDI CCs snap multi-position controls (Waveform, Scale, Play Mode) to their positions.

## Oscillator

- Phase accumulator **at the oversampled rate (4x)**, band-limited with **PolyBLEP** on the saw reset (and both edges for square). Running it at 4x is cheaper and cleaner than upsampling a 1x oscillator.
- **Saw**: the PolyBLEP ramp is bent slightly, `v + 0.12 (v² − 1/3)`, as a capacitor charging through the hardware's current source bends it: a little even-harmonic warmth, no DC (the term averages to zero), and no extra discontinuity (v² is 1 at both ends of the cycle, so the reset's step stays the one the BLEP corrects). Mid-cycle it sits 0.04 under the straight ramp.
- **Square**: made from that saw, as the hardware does, by a transistor-style shaper: `tanh(14 (saw − 0.06)) / tanh(14)`. Its rising edge is the comparator's fast slope, its falling edge the saw's reset, and the threshold above the saw's middle makes the pulse a little under half high (45 %: the bent saw crosses 0.06 at a ramp value of 0.099), so it is slightly uneven and less hollow than an ideal square. Running at 4× keeps the shaper's edges clean.
- Pitch: `f = 440 · 2^((midiNote − 69 + tuning) / 12)`; step note mapping is in [SEQUENCER.md](SEQUENCER.md).

## Slide

When a slide is active, pitch moves exponentially (one-pole in the log-frequency domain) toward the new note, time constant ~60 ms (*starting value*, fixed like the hardware, not a user control). Slide never retriggers either envelope.

## Filter: diode ladder

The core of the sound. Requirements:

1. **Topology:** four-stage diode ladder lowpass with global feedback (resonance), solved as a **zero-delay-feedback (ZDF / TPT)** structure so cutoff and resonance can be modulated per sample without instability.
2. **Nonlinearity:** `tanh(1.5·x)` on the signal going in (1.5 to 2.25 on accented notes: accents grind the filter), and the ladder's input, solved by the linear ZDF step, through a soft ceiling `1.2 · tanh(u / 1.2)`, where the feedback meets the signal, as the diodes and transistors there limit it. Small signals stay linear (the tests drive the ladder at 0.05 to check its linear response); a loud resonant peak compresses and growls instead of growing (tested: at k = 15 a full-scale sine at the peak gets under half the boost of a quiet one). An iterative (Newton-Raphson) solve of the nonlinear system remains possible if wanted.
3. **Oversampling:** the oscillator and filter run at **4x** (`Voice::kOversample`). Two 31-tap Blackman-windowed half-band FIR stages decimate back to the base rate (flat to ~15 kHz at 48 kHz, stopband > 70 dB down). Filter coefficients update once per base-rate sample.
4. **Character:** a diode ladder rolls off more gently than a transistor ladder (often described as ~18 dB/oct) and its resonance stays below strong self-oscillation. Clamp feedback so the maximum knob position is loud and squelchy but stable.
5. **Bass loss at high resonance:** the ladder's passband gain is 1 / (1 + k) (−24 dB at k = 15). Makeup gain of (1 + k)^0.8 restores 80 % of that in dB. With half (√(1 + k), as before), the default resonance left a held low A's fundamental 8.3 dB under no resonance, which is what made the line sound thin; now it is 1.9 dB under at 1 kHz cutoff, and full resonance 3 dB under, so resonance still thins the bass a little, as on the hardware, but the line keeps its weight. Measured (held A1, Env Mod 0): resonance 0 / 0.5 / 1 → fundamental −8.5 / −10.4 / −11.5 dB (was −7.7 / −16.0 / −18.8).
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
3. **Accent sweep:** an extra filter-cutoff push from a capacitor-like smoother driven by `MEG × accent`: it charges with τ = 30 ms and discharges with τ = 150 ms. Depth: up to `sweep` octaves (0–4, default 2) × (0.5 + `resonance`). Consecutive accents stack and drive the filter higher. Depth scales with `accent` and with `resonance`, as on the hardware. This stacking is what makes accented runs "scream"; treat it as a required feature, not an option.
4. **Harder into the filter:** the accent's VCA gain (1 … 2) also raises the ladder's input drive by up to 1.5× (`kAccentDrive` 0.5 × the extra gain), so accented notes grind the filter as well as opening it.

## Output

The voice is scaled by −4 dB (`kOutputGain` 0.63): the fuller bass and the saturating ladder made the demo pattern 5.6 dB louder; trimmed, it is 1.8 dB louder than before (and 2.1 dB more below 150 Hz), so Korrode sits about where it did in a mix, bigger rather than just louder. Then, in the engine:

1. **Drive** (bypassed at 0): tanh(g·x) with g = 1 + 24 · drive², normalized so a typical peak (0.5) stays where it is (the body squares off and comes up) and trimmed by 14 % at full drive: measured on the demo, +1.1 dB at 25 %, +2.6 dB at 50 %, +3 dB at 100 %, peaks lower. First-order antiderivative anti-aliasing (the average of tanh over each sample step, F = ln cosh): far less aliasing than a plain tanh, no oversampling, no latency. **Tone** follows it: a one-pole low-pass from 800 Hz to 16 kHz.
2. **Delay** (`krate::Delay`, the shared tempo-synced ping-pong; runs only while Delay is up) and **reverb** (`krate::Reverb`, the shared FDN, fed the bass and its echoes; runs only while Reverb is up). Their wet signal is mixed in M/S: **Width** scales its side (0 mono, 0.5 as the effects make it, 1 twice as wide); the dry bass stays mono in the centre, solid on any system. With both off, the two outputs are identical (tested).
3. **Volume**, smoothed.

The engine does not limit: the host's master stage (`krate::MasterStage`, a soft clip, linear to −6 dBFS ([Master stage](../../../docs/SHARED_FOUNDATIONS.MD#master-stage)), in the JACK host and in `korrode-render`) prevents hard overs and feeds the level meter. In the Krate rack the mixer's master stage takes that role.

## Performance budget

Target < 5% of one core at 48 kHz / 64-frame buffers with 4x oversampling on the dev machine. Measure with `korrode-render --bench`.

Measured (M3, dev machine, RelWithDebInfo): **1.0%** at 48 kHz, **2.0%** at 96 kHz. With the fuller voice (tanh in the ladder and the shaper at 4×): **1.4%**; with Drive, delay and reverb on (`korrode-render --bench --drive 0.6 --delay_level 0.4 --reverb_level 0.3`): **1.7%**.
