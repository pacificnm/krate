# Konnekt

A modular synthesizer, its own app in the [Krate](../../README.md) suite: a rack of modules (oscillators, filters, envelopes, LFOs, VCAs, mixers, utilities, effects, a clock and a sequencer) built from the parts of Krate's synths and wired together with virtual patch cables, up to 8 voices, played from a MIDI controller. Made for learning how synthesizers work: every jack shows its signal live, every module, knob and jack explains itself, and lesson patches build a synth up a module at a time.

Status: N3. The app runs: `pw-jack ./build/bin/konnekt`. Add modules (+ ADD MODULE), click a jack and then another to wire them, point at anything to learn what it does, right-click an output to see it on the scope; your MIDI keyboard plays it and LEARN maps its knobs. Saved patches by name and the lessons come next. `./build/bin/konnekt-render --out-dir build` writes `konnekt-patch.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).
