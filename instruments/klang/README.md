# Klang

A sampler for the [Krate](../../README.md) suite: 16 pads playing your own WAV and MP3 files as one-shots, held notes or loops locked to the tempo, each through its own filter, drive, bitcrusher and ring modulator, with a trance gate, delay and reverb; patterns with accents, stutters, reverse and pitch per step; loops sliced across the pads. Built for cut-up, gated, mechanical industrial and trance sounds. Runs standalone (`klang`) and in the Krate rack (default MIDI channel 11).

Status: L4, complete. Playable standalone (`pw-jack ./build/bin/klang`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 11): a starter kit of its own sounds on first run; load WAV or MP3 files onto the pads (LOAD, or drop them on a pad), slice them, shape each pad, program patterns.

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): every control, with recipes and troubleshooting. In the app, **HELP** on the panel opens it. `./build/bin/klang-render --out-dir build` writes `klang-sounds.wav` (sounds Klang makes itself). See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, sound engine, starter kit, registry entry |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones L1–L4 |
