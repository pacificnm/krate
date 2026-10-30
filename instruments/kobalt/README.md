# Kobalt

A monophonic synthesizer for the [Krate](../../README.md) suite, after the classic early-80s one-oscillator monosynths: saw, pulse with width modulation, a sub-oscillator and noise in a source mixer, a self-oscillating four-pole low-pass, one envelope with three ways to trigger it, an LFO, portamento, bender and modulation lever, an arpeggiator and a step sequencer on the shared transport, delay and reverb, and 64 patches. Runs standalone (`kobalt`) and in the Krate rack (default MIDI channel 7).

Status: B4, complete. Playable standalone (`pw-jack ./build/bin/kobalt`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 7): the full panel, on-screen keyboard, delay and reverb, 64 patches with 16 factory sounds, the arpeggiator and a 64-step sequencer with ties and legato on the transport. `./build/bin/kobalt-render --out-dir build` writes `kobalt-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): every control, what it does and how to use it, with recipes and troubleshooting. In the app, **HELP** on the panel opens it (with a search).

| Doc | Contents |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, sound engine, MIDI, registry entry |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones B1–B4 |
