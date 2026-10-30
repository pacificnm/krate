# Klang

A sampler for the [Krate](../../README.md) suite: 16 pads playing your own WAV and MP3 files as one-shots, held notes or loops locked to the tempo, each through its own filter, drive, bitcrusher and ring modulator, with a trance gate, delay and reverb; patterns with accents, stutters, reverse and pitch per step; loops sliced across the pads. Built for cut-up, gated, mechanical industrial and trance sounds. Runs standalone (`klang`) and in the Krate rack (default MIDI channel 11).

Status: L3. Playable standalone (`pw-jack ./build/bin/klang`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 11): load WAV or MP3 files onto the pads (LOAD, or drop them on a pad), slice them, shape each pad, program patterns. The starter kit and the manual come next. `./build/bin/klang-render --out-dir build` writes `klang-sounds.wav` (sounds Klang makes itself). See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, sound engine, registry entry |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones L1–L4 |
