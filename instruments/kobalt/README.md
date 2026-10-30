# Kobalt

A monophonic synthesizer for the [Krate](../../README.md) suite, after the classic early-80s one-oscillator monosynths: saw, pulse with width modulation, a sub-oscillator and noise in a source mixer, a self-oscillating four-pole low-pass, one envelope with three ways to trigger it, an LFO, portamento, bender and modulation lever, an arpeggiator and a step sequencer on the shared transport, delay and reverb, and 64 patches. Runs standalone (`kobalt`) and in the Krate rack (default MIDI channel 7).

Status: B1. The sound engine plays from MIDI through `kobalt-render` and the tests; the panel, patches and the rack come in B2. `./build/bin/kobalt-render --out-dir build` writes `kobalt-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, sound engine, MIDI, registry entry |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones B1–B4 |
