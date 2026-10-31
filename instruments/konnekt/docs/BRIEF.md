# Konnekt: brief

2026-10-31

Konnekt is a modular synthesizer, part of the [Krate](../../../README.md) suite but its own app (`konnekt`, not a device in the rack): a rack of modules, oscillators, filters, envelopes, LFOs, VCAs, mixers, utilities, effects, a clock and a sequencer, built from the parts of Krate's synths, wired together with virtual patch cables. It is made for learning how synthesizers work: every jack shows what flows through it, every module, knob and jack explains itself, and lesson patches build a synth up one module at a time. It plays from a MIDI controller (no on-screen keyboard), up to 8 notes at once.

Every name and graphic is our own; it isn't modelled on any maker's modular format.

Related docs: [ROADMAP.md](ROADMAP.md).

## Scope (v1)

- **Modules** in rows on a scrolling rack: add one from the module list, drag it to move it, delete it from its menu. Up to 64 modules.
- **Cables**: click a jack, then another: an output to an input (either order). One output feeds any number of inputs; an input takes one cable (a new one replaces it). Click a cable's input end to unplug it; dragging from a plugged input lifts its cable to plug elsewhere. Cables sag and take colours in turn. Up to 256.
- **Voices**: 1 to 8 (VOICES in the top bar). The MIDI In module gives each new note a voice (the oldest is taken when all are busy; the same key reuses its voice). Per-voice modules (oscillators, filters, envelopes, VCAs...) run once per voice; the others (effects, the output, the clock, the sequencer, the scope) once. A per-voice cable into a one-voice input mixes its voices; a one-voice cable into a per-voice input reaches every voice.
- **Signals**: audio ±1; pitch 1 per octave, 0 = C4 (MIDI 60, 261.63 Hz); gates and triggers 0 or 1 (on above 0.5); modulation ±1 or 0..1. An unplugged input reads 0, unless it has a knob of its own (the knob's value then stands in). A modulation input has an amount knob beside it.
- **Feedback**: modules run in signal order each 16-sample block; a cable going back up the chain (a loop) arrives one block (16 samples) later.
- **Learning aids**: every jack shows its signal live (a small moving trace); a scope opens on any cable; hovering a module, knob or jack explains it in the help bar, and right-clicking it opens the whole explanation (what it is, how it works, why, where it goes, what to try, what it's doing now); lesson patches.
- **Patches**: saved and loaded by name (`~/.local/share/konnekt/patches/*.json`); the patch in use is kept when you quit.
- **MIDI**: notes with velocity, pitch bend, mod wheel, aftertouch, sustain into MIDI In; MIDI Learn: any knob from a controller's knob or slider.

## Modules (v1)

| Module | Voices | What it is | From |
| --- | --- | --- | --- |
| MIDI In | per voice | Pitch, gate, velocity per voice; bend, mod wheel, aftertouch | |
| Oscillator | per voice | Band-limited saw, pulse (width, PWM), triangle, sine, square; pitch, FM (exponential and linear), sync | Kalliope, Krimson |
| Noise | per voice | White and pink noise | Kobalt |
| Ladder filter | per voice | 4-pole transistor ladder, 12/24 dB, resonance to self-oscillation, drive | Kalliope |
| OTA filter | per voice | 4-pole with a soft limit in every stage | Kobalt |
| State-variable filter | per voice | Low-, band-, high-pass outputs at once | Klang |
| Envelope | per voice | ADSR, gate in, retrigger | all |
| LFO | per voice | Triangle, square, saw, sine, random, smooth random; rate, reset | Kalliope, Kobalt |
| VCA | per voice | Audio times a control: level, linear or exponential | |
| Mixer | per voice | 4 inputs with levels | |
| Attenuverter | per voice | Scale (−1..+1) and offset a signal: the basic tool of modulation | |
| Sample & hold | per voice | Takes the input when triggered and holds it | |
| Slew | per voice | Smooths jumps: glide, portamento | Kobalt |
| Drive | per voice | Soft clip into a wavefolder | Klang |
| Crusher | per voice | Bits and rate | Klang |
| Ring mod | per voice | Multiplies two signals | Klang |
| Clock | one | Tempo, divisions, start / stop | |
| Sequencer | one | 8 steps, each a pitch and on / off, clocked (length, gate length) | Korrode, Kobalt |
| Chorus | one | Stereo chorus | shared |
| Delay | one | Tempo-synced stereo ping-pong delay | shared |
| Reverb | one | Stereo reverb | shared |
| Scope | one | A big scope on what's plugged in | |
| Output | one | Left and right to the speakers, volume | |
