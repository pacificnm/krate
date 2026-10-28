# Kalliope

An eight-voice polyphonic synthesizer for the [Krate](../../README.md) suite: two oscillators per voice with sync and cross mod, high-pass and resonant 12/24 dB low-pass filters, two envelopes, an arpeggiator on the shared transport, chorus and delay, 64 patches, and a hardware-style panel of sliders. Runs standalone (`kalliope`) and in the Krate rack (default MIDI channel 2).

Status: P2 (8 voices, Poly / Unison / Solo, sustain, played from MIDI; panel frame only). `pw-jack ./build/bin/kalliope`; `./build/bin/kalliope-render --out-dir build` writes `kalliope-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, MIDI, registry entry |
| [DSP.md](docs/DSP.md) | Oscillators, filters, envelopes, voices, effects |
| [PLAY.md](docs/PLAY.md) | Key modes, arpeggiator, patches and storage |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones P0–P7 |
