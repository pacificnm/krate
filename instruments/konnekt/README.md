# Konnekt

A modular synthesizer, its own app in the [Krate](../../README.md) suite: a rack of modules (oscillators, filters, envelopes, LFOs, VCAs, mixers, utilities, effects, a clock and a sequencer) built from the parts of Krate's synths and wired together with virtual patch cables, up to 8 voices, played from a MIDI controller. Made for learning how synthesizers work: every jack shows its signal live, every module, knob and jack explains itself, and lesson patches build a synth up a module at a time.

Status: N9, complete. `pw-jack ./build/bin/konnekt`: add modules (+ ADD MODULE), click a jack and then another to wire them, point at anything for a line about it and right-click it for the whole story (what it is, how it works, why you'd want it, what it's doing now), right-click an output to see it on the scope; LESSONS builds a synth up a module at a time; PRESETS has 84 finished sounds (basses, leads, 80s and synth-pop leads, pads, drum loops, effects) to play and take apart; PATCHES saves yours by name; your MIDI keyboard plays it and LEARN maps its knobs. `./build/bin/konnekt-render --out-dir build` writes `konnekt-patch.wav`.

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): signals, wiring, voices, every module, the lessons, MIDI. In the app, **HELP** opens it.

| Doc | Contents |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [BRIEF.md](docs/BRIEF.md) | Scope, modules, signals |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones N1–N9 |
