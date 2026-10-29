# Krimson

A four-part multitimbral virtual-analog synthesizer for the [Krate](../../README.md) suite, aimed at the sound of the classic late-90s red virtual-analog lead synth. It has sync, FM and ring-mod oscillators, a five-mode filter with drive, and the mod-wheel morph. Its 16 voices are shared by four slots, each with its own arpeggiator and effects. Runs standalone (`krimson`) and in the Krate rack (default MIDI channels 3–6).

Status: C4 (16 voices shared by four slots A–D: own channels 3–6, layering, split; the full panel for the edited slot, wheels and keyboard; slots saved in `~/.config/krimson/slots.json`). Morph, effects, programs, the arpeggiator and the rack to come. `pw-jack ./build/bin/krimson`; `./build/bin/krimson-render --out-dir build` writes `krimson-sounds.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters (one slot at a time), MIDI, registry entry |
| [DSP.md](docs/DSP.md) | Voice, filter, morph, voices and slots, effects |
| [PLAY.md](docs/PLAY.md) | Slots, voice modes, morph, arpeggiator, programs and performances |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones C0–C9 |
