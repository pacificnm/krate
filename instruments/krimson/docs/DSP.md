# Krimson: DSP

Realtime-safe throughout, as every Krate instrument. Values here are design starting points; this page follows the code once it exists, and the sounds are tuned by ear against the target character in C9.

## Reuse

Kalliope's building blocks moved to `shared/dsp/` in C1 (Kalliope, Korrode and the rack renders byte-identical): the band-limited `krate::Oscillator` (`dsp/BlepOscillator.h`: every jump corrected with a band-limited step, sync included), `krate::Envelope` and `krate::Lfo` (which gained Triangle and Soft random). Kalliope's `Chorus` and `Delay` followed in C6 (`dsp/Effects.h`), with the new `Reverb`. Krimson adds what the target synth has and Kalliope doesn't: ring mod, a multi-mode filter with drive, the mod envelope, the second LFO, morph and the reverb.

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
| **Filter** | `MultiFilter`: a TPT state-variable core (k = 2 … 0.06 with resonance) gives LP 12, band-pass, high-pass and notch (LP + HP; notch + LP adds a second, unresonant Butterworth LP 12). Its resonant (band-pass) state saturates softly (a ceiling of ±2.5), so high resonance compresses and growls rather than whistling (at 95 % resonance, 30 dB more in is well under 30 dB more out at the peak). **LP 24 is a four-pole ladder**: four TPT one-poles with the resonance (k = 0 … 3.9, just under self-oscillation) fed back around all four through a soft saturator, solved without delay (y₄ = (G⁴x + S) / (1 + kG⁴)), the input raised by 1 + 0.75k so the bass doesn't vanish as resonance rises (it still dips a few dB, as an analog ladder's does); −24 dB/octave, about +14 dB of peak at 90 %. Drive: tanh(g·x)/√g with g = 1 … 8 before the filter. A tanh on the output keeps full resonance bounded. Cutoff = knob + filter envelope (8 octaves × velocity scaling) + key tracking (0, ⅓, ⅔, 1 from middle C) + LFOs + aftertouch (when Pressure goes to Filter) + the note's analog offset, smoothed over 3 ms |
| **Envelopes** | `krate::Envelope`: ADSR (filter, amp) and AD (mod: sustain 0, always restarts from zero) with exponential segments, 0.5 ms … 10 s, snappy at the short end like the target's |
| **LFOs** | LFO 1 per voice (triangle, saw, square, sample & hold, soft random; restarts when a silent voice starts); LFO 2 per slot (triangle; amp tremolo, filter, or OSC 2 pitch). Vibrato (Wheel set to Vibrato) has its own 5.5 Hz sine, up to 0.7 semitones |
| **Unison** | 3, 5 or 7 voices (`unison_voices`) stacked on the top held note, detuned evenly from − to + by `unison_detune` (the outer ones up to ±35 cents) and spread evenly from left to right (±0.7); each voice at (3 / n)^0.85 so 5 or 7 play about as loud as 3 (between adding as one, close and in step, and by power, wide); mono, last-note priority, retriggering |
| **Analog** | `analog` (0 … 100 %, default 30 %) per slot: each voice draws, per note, where its two oscillators start in their cycle, a fixed offset for each (up to ±5 cents) and for its cutoff (up to ±¼ octave); while it plays, each oscillator wanders slowly (new targets every 0.2 … 0.6 s, reached over ~0.4 s, up to ±12 cents), all scaled by Analog. Each voice has its own repeatable random sequence, so renders are the same every time. At 0 the voice is exactly as before. Factory programs that are designed hits (Clang Hit, Factory Hit, Minimal Blip, Bleep, Laser Zap) keep it at 0 so their attack stays exact |
| **Pressure** | `pressure_dest`: aftertouch (channel pressure, or a key's poly pressure, for the slots that key reaches) to Off, Filter (the cutoff up to 2.5 octaves), Vibrato (up to 0.7 semitones on the wheel's vibrato LFO) or both |
| **Glide** | Exponential pitch glide from the last note played (the knob is the time to get almost there, up to 2 s), in every mode; in Legato only between held notes (a fresh phrase starts on its note) |

## Morph

Every continuous slot parameter can have a **morph target** (`Engine::setMorph`; −1 = none; detented ones are ignored). Each block, a slot with targets gets its sound worked out twice: at the panel values and with every target in place (`settings(slot, true)`). Each voice then plays `blend(panel, targets, amount)`. Here `amount` is the slot's mod wheel when its Wheel is set to Morph, else the voice's velocity. The blend is linear for levels, amounts, cutoff (already in octaves), semitones and pan. Envelope times, glide and LFO rate blend in the log domain, so they move as their exponential knobs do. Lists and switches come from the panel values. It costs a few dozen multiply-adds per voice per segment.

## Voices and slots

16 voices for the whole instrument, each remembering the slot it plays (C3 lets the slots share them). **Poly**: a note already sounding reuses its voice; else a silent voice; else the oldest released (or pedal-held) voice; else the oldest held one. A taken voice fades out over 2 ms before its new note starts. **Mono**: one voice, last-note priority, retriggering (also when going back to a held key). **Legato**: as Mono without retriggering while keys are held. **Unison**: see above. Changing Voice Mode releases every note; the sustain pedal (CC 64) holds released notes; CC 120 / 123 release everything. Voices are rendered in stereo at 2× (each with its pan) and decimated per side. Each slot keeps its own key list, sustain, bend, wheel, voice mode, glide and LFO 2 (and, from C8, its arpeggiator); Mono, Legato and Unison slots keep the voices they took while they still have them. Each voice is scaled by its slot's level and placed by its slot's pan (plus its unison position).

## Effects (per slot, then summed)

Each slot's voices go to its own stereo bus (at 2×, decimated per slot). Then come that slot's effects, in this order, and then the four slots are summed. All of them are in `shared/dsp/`:

| Effect | Design |
| --- | --- |
| Drive | `fx_drive` (0 = off): the slot's bus at 2×, before decimation (so what it adds above the band is filtered away): a soft, slightly asymmetric clip (gain 1 … 25), its level kept, the top softened (a low-pass from 12 kHz down to 6 kHz as it rises) |
| Chorus / Ensemble | Krimson's own `Ensemble` (Kalliope keeps the shared chorus, unchanged): a delay line per side (each with a little of the other), read with a 4-point curve, the copies darkened above ~8 kHz and gently saturated as through bucket-brigade chips. Chorus: one copy per side, ±2.6 ms around 7 ms, a 0.45 Hz triangle, the sides a quarter cycle apart. Ensemble: three copies per side a third of a cycle apart, ±1.8 ms around 9 ms at 0.62 Hz, with a 5.8 Hz shimmer on top. The dry level is kept (dry + copies, levelled) |
| Delay | `krate::Delay` (Kalliope's ping-pong delay), its time a note value at the transport's tempo (the Tempo knob without a transport), feedback 0–90 % |
| Reverb | `krate::Reverb`: eight delay lines (30–73 ms) through a Hadamard mix, each damped (6 kHz one-pole), feedback set so the tail falls 60 dB in 0.4–8 s (Size); stereo out from different lines |

A delay or reverb at zero level isn't processed, and is cleared when turned up, so no stale tail plays. Buffers (2 s of delay and the reverb lines at up to 192 kHz, per slot) are allocated when the engine is constructed. Effects follow the panel values; they don't morph.

## CPU

The analog upgrades (drift, the ladder, saturation, drive, the ensemble) took `--bench` (16 voices, every effect) from 11.2 % to 12.6 % of one core.

Budget: 16 voices at 2× with all four slots' effects under 15 % of a core at 48 kHz / 64 frames (`krimson-render --bench`). C6 measures 16 voices with sync, FM, drive and full resonance through ensemble, delay and reverb at 11.1 % (one voice: 0.71 %).
