# Krimson: DSP

Realtime-safe throughout, as every Krate instrument. Values here are design starting points; this page follows the code once it exists, and the sounds are tuned by ear against the target character in C9.

## Reuse

Kalliope's building blocks move to `shared/dsp/` in C1, and Kalliope must keep rendering byte-identical: the band-limited `Oscillator` (every jump corrected with a band-limited step, sync included), `Envelope`, `Lfo`, the TPT `Ladder` and the `Chorus` / `Delay`. Krimson adds what the target synth has and Kalliope doesn't: ring mod, a multi-mode filter with drive, the mod envelope, the second LFO, morph and the reverb.

## Voice (runs at 2× and is decimated, as Kalliope)

```
 OSC 1 (sine/tri/saw/pulse) ◄─ FM (OSC 2 → OSC 1 phase) ─┐
     │                                                      │
     │  sync (OSC 2 reset by OSC 1)       OSC 2 (tri/saw/pulse/noise, semi, fine, kbd on/off)
     │                                                      │
     └──────── mix ◄──────────── ring: OSC 1 × OSC 2 ◄──────┘
                │
         drive ─► multi-mode filter (LP12 / LP24 / BP / HP / notch+LP12) ─► amp (ENV × gain × LFO 2) ─► slot
                        ▲ cutoff: knob + filter ENV × amount (+ velocity) + key track + LFOs + morph
```

| Block | Design |
| --- | --- |
| **FM** | Phase modulation of OSC 1 by OSC 2 (bright, digital-edged FM rather than Kalliope's analog-style linear FM), depth up to ~3 radians |
| **Ring mod** | OSC 2's mix input becomes OSC 1 × OSC 2 (bells, metallic leads) |
| **Filter** | State-variable core with zero-delay feedback for LP 12, BP, HP and notch (two outputs summed); LP 24 is two cascaded LP 12 stages sharing the resonance, tuned for the slightly thin, bright 24 dB of the target. Drive is a tanh with gain before the filter, its level compensated |
| **Envelopes** | ADSR (filter, amp) and AD (mod) with fast exponential segments; the target's envelopes are snappy, so the shortest times go down to 0.5 ms |
| **LFOs** | LFO 1 per voice (five waves, phase restarts at note-on in mono modes); LFO 2 per slot (triangle). Vibrato (wheel to Vibrato) has its own fixed 5.5 Hz LFO with a short fade-in |
| **Unison** | The slot's voices stacked on one note (3 per note by default), detuned symmetrically by `unison_detune`, spread in stereo |
| **Glide** | Exponential pitch glide; in Legato only between held notes |

## Morph

Every continuous parameter of a slot has an optional **morph value**. The morph source moves the parameter from its panel value toward that value, linearly in its normalized range. The source is the mod wheel when `wheel_dest` is Morph, else velocity. This happens per voice, at control rate (every 32 samples at 2×), so a wheel sweep changes cutoff, FM, envelope times and so on all together. Morph values are stored in programs.

## Voices and slots

16 voices for the whole instrument. A slot takes voices as notes arrive: a free voice, else the oldest released voice of any slot, else the oldest held voice of the slot playing (with a 2 ms declick). Unison spends 3 voices per note. Each slot keeps its own key list, sustain, bend, wheel, arpeggiator and LFO 2.

## Effects (per slot, then summed)

| Effect | Design |
| --- | --- |
| Chorus / Ensemble | Kalliope's chorus; Ensemble uses three taps with slower, deeper modulation |
| Delay | Kalliope's ping-pong delay, synced to the transport |
| Reverb | A small feedback-delay-network reverb (8 delay lines, Hadamard mix, damping), size = decay time; buffers allocated at construction |

## CPU

Budget: 16 voices at 2× with all four slots' effects under 15 % of a core at 48 kHz / 64 frames (`krimson-render --bench`).
