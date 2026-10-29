# Krimson: DSP

Realtime-safe throughout, as every Krate instrument. Values here are design starting points; this page follows the code once it exists, and the sounds are tuned by ear against the target character in C9.

## Reuse

Kalliope's building blocks moved to `shared/dsp/` in C1 (Kalliope, k303 and the rack renders byte-identical): the band-limited `krate::Oscillator` (`dsp/BlepOscillator.h`: every jump corrected with a band-limited step, sync included), `krate::Envelope` and `krate::Lfo` (which gained Triangle and Soft random). Kalliope's `Chorus` / `Delay` follow in C6. Krimson adds what the target synth has and Kalliope doesn't: ring mod, a multi-mode filter with drive, the mod envelope, the second LFO, morph and the reverb.

## Voice (runs at 2× and is decimated, as Kalliope)

OSC 2 can be the noise source; with keyboard tracking off it stays at middle C plus its semitones (for fixed-pitch sync and FM timbres). The mix is OSC 1 ↔ OSC 2 (both full at the centre); with ring mod on, OSC 2's input is OSC 1 × OSC 2.

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
| **FM** | Linear FM: OSC 2's output scales OSC 1's frequency by 1 ± 3 × FM (clamped at 0), every sample at 2×; the mod envelope and LFO 1 can add to FM |
| **Ring mod** | OSC 2's mix input becomes OSC 1 × OSC 2 (bells, metallic leads) |
| **Filter** | `MultiFilter`: a TPT state-variable core (k = 2 … 0.06 with resonance) gives LP 12, band-pass, high-pass and notch (LP + HP); LP 24 and notch + LP add a second, unresonant (Butterworth) LP 12 stage. Drive: tanh(g·x)/√g with g = 1 … 8 before the filter. A tanh on the output keeps full resonance bounded. Cutoff = knob + filter envelope (8 octaves × velocity scaling) + key tracking (0, ⅓, ⅔, 1 from middle C) + LFOs, smoothed over 3 ms |
| **Envelopes** | `krate::Envelope`: ADSR (filter, amp) and AD (mod: sustain 0, always restarts from zero) with exponential segments, 0.5 ms … 10 s, snappy at the short end like the target's |
| **LFOs** | LFO 1 per voice (triangle, saw, square, sample & hold, soft random; restarts when a silent voice starts); LFO 2 per slot (triangle; amp tremolo, filter, or OSC 2 pitch). Vibrato (Wheel set to Vibrato) has its own 5.5 Hz sine, up to 0.7 semitones |
| **Unison** | Three voices stacked on the top held note, detuned −, 0, + by `unison_detune` (the outer ones up to ±35 cents) and placed left, centre, right (±0.7); mono, last-note priority, retriggering |
| **Glide** | Exponential pitch glide from the last note played (the knob is the time to get almost there, up to 2 s), in every mode; in Legato only between held notes (a fresh phrase starts on its note) |

## Morph

Every continuous parameter of a slot has an optional **morph value**. The morph source moves the parameter from its panel value toward that value, linearly in its normalized range. The source is the mod wheel when `wheel_dest` is Morph, else velocity. This happens per voice, at control rate (every 32 samples at 2×), so a wheel sweep changes cutoff, FM, envelope times and so on all together. Morph values are stored in programs.

## Voices and slots

16 voices for the whole instrument, each remembering the slot it plays (C3 lets the slots share them). **Poly**: a note already sounding reuses its voice; else a silent voice; else the oldest released (or pedal-held) voice; else the oldest held one. A taken voice fades out over 2 ms before its new note starts. **Mono**: one voice, last-note priority, retriggering (also when going back to a held key). **Legato**: as Mono without retriggering while keys are held. **Unison**: see above. Changing Voice Mode releases every note; the sustain pedal (CC 64) holds released notes; CC 120 / 123 release everything. Voices are rendered in stereo at 2× (each with its pan) and decimated per side. In C3 each slot keeps its own key list, sustain, bend, wheel, arpeggiator and LFO 2.

## Effects (per slot, then summed)

| Effect | Design |
| --- | --- |
| Chorus / Ensemble | Kalliope's chorus; Ensemble uses three taps with slower, deeper modulation |
| Delay | Kalliope's ping-pong delay, synced to the transport |
| Reverb | A small feedback-delay-network reverb (8 delay lines, Hadamard mix, damping), size = decay time; buffers allocated at construction |

## CPU

Budget: 16 voices at 2× with all four slots' effects under 15 % of a core at 48 kHz / 64 frames (`krimson-render --bench`). C2 measures 16 voices with sync, FM, drive and full resonance at 9.9 % (one voice: 0.71 %).
