# Kalliope

A sixteen-voice polyphonic synthesizer for the [Krate](../../README.md) suite: two oscillators per voice with sync and cross mod, analog drift, high-pass and a driven, resonant 12/24 dB ladder low-pass, two envelopes, an arpeggiator and a step sequencer with a pattern generator on the shared transport, chorus, delay and reverb, aftertouch, 64 patches, and a hardware-style panel of sliders. Runs standalone (`kalliope`) and in the Krate rack (default MIDI channel 2).

Status: P6. Playable standalone (`pw-jack ./build/bin/kalliope`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 2): 16 voices, the full panel, on-screen keyboard, chorus, delay and reverb, 64 patches with 16 factory sounds, the arpeggiator and a 16-step sequencer with a pattern generator, all on the transport. Polish (P8) to come. `pw-jack ./build/bin/kalliope`; `./build/bin/kalliope-render --out-dir build` writes `kalliope-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): every control, what it does and how to use it, with recipes and troubleshooting. In the app, **HELP** on the panel opens it (with a search).

| Doc | Contents |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, MIDI, registry entry |
| [DSP.md](docs/DSP.md) | Oscillators, filters, envelopes, voices, effects |
| [PLAY.md](docs/PLAY.md) | Key modes, arpeggiator, patches and storage |
| [SEQUENCER.md](docs/SEQUENCER.md) | Step sequencer and pattern generator |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones P0–P8 |
