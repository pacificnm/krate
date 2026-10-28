# K909

A drum machine for the [Krate](../../README.md) suite: eleven synthesized drum voices, a tracks × steps sequencer with accent and shuffle, and a hardware-style panel. Runs standalone (`k909`) and in the Krate rack (default MIDI channel 10).

Status: D4 (voices, knobs, notes, sequencer, patterns A–H saved to `~/.local/share/k909/patterns.json`). `./build/bin/k909-render --pattern 0` renders a factory pattern. `pw-jack ./build/bin/k909` plays from MIDI notes (pads on channel 10 or any channel); `./build/bin/k909-render --out-dir build` writes `k909-voices.wav` and a demo `k909-beat.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, MIDI notes, registry entry |
| [DSP.md](docs/DSP.md) | How each voice is synthesized |
| [SEQUENCER.md](docs/SEQUENCER.md) | Pattern data, playback, shuffle, editing, storage |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones D0–D6 |
