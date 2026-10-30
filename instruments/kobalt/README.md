# Kobalt

A monophonic synthesizer for the [Krate](../../README.md) suite, after the classic early-80s one-oscillator monosynths: saw, pulse with width modulation, a sub-oscillator and noise in a source mixer, a self-oscillating four-pole low-pass, one envelope with three ways to trigger it, an LFO, portamento, bender and modulation lever, an arpeggiator and a step sequencer on the shared transport, delay and reverb, and 64 patches. Runs standalone (`kobalt`) and in the Krate rack (default MIDI channel 7).

Status: B3. Playable standalone (`pw-jack ./build/bin/kobalt`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 7): the full panel, on-screen keyboard, delay and reverb, 64 patches with 16 factory sounds, the arpeggiator and a 64-step sequencer on the transport. The manual comes in B4. `./build/bin/kobalt-render --out-dir build` writes `kobalt-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, sound engine, MIDI, registry entry |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones B1–B4 |
