# Konnekt

A modular synthesizer, its own app in the [Krate](../../README.md) suite: a rack of modules (oscillators, filters, envelopes, LFOs, VCAs, mixers, utilities, effects, a clock and a sequencer) built from the parts of Krate's synths and wired together with virtual patch cables, up to 8 voices, played from a MIDI controller. Made for learning how synthesizers work: every jack shows its signal live, every module, knob and jack explains itself, and lesson patches build a synth up a module at a time.

Status: N2. The engine and all 23 modules play through `konnekt-render` and the tests; the app and the lessons come next. `./build/bin/konnekt-render --out-dir build` writes `konnekt-patch.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).
