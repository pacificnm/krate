# Konnekt

A modular synthesizer, its own app in the [Krate](../../README.md) suite: a rack of modules (oscillators, filters, envelopes, LFOs, VCAs, mixers, utilities, effects, a clock and a sequencer) built from the parts of Krate's synths and wired together with virtual patch cables, up to 8 voices, played from a MIDI controller. Made for learning how synthesizers work: every jack shows its signal live, every module, knob and jack explains itself, and lesson patches build a synth up a module at a time.

Status: N1. The engine and the first modules (MIDI In, Oscillator, Envelope, LFO, VCA, Mixer, Attenuverter, Output) play through `konnekt-render` and the tests; the other modules, the app and the lessons come next. `./build/bin/konnekt-render --out-dir build` writes `konnekt-patch.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).
