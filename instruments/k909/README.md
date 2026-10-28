# K909

A drum machine for the [Krate](../../README.md) suite: eleven synthesized drum voices, a tracks × steps sequencer with accent and shuffle, and a hardware-style panel. Runs standalone (`k909`) and in the Krate rack (default MIDI channel 10).

Status: v1 (D0–D6). Playable standalone (`pw-jack ./build/bin/k909`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 10). Patterns are saved to `~/.local/share/k909/patterns.json`, MIDI mappings to `~/.config/k909/midimap.json` (standalone) or the rack session. `./build/bin/k909-render --pattern 0` renders a factory pattern. `pw-jack ./build/bin/k909` plays from MIDI notes (pads on channel 10 or any channel); `./build/bin/k909-render --out-dir build` writes `k909-voices.wav` and a demo `k909-beat.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

## Quick start

```sh
pw-jack ./build/bin/k909      # standalone; or pw-jack ./build/bin/krate for the rack
./build/bin/k909-render --out-dir build            # every voice, and a demo beat
./build/bin/k909-render --pattern 0 --out a.wav    # a factory pattern through the sequencer
```

1. Press **START** (or Space): pattern A plays.
2. Click a drum name under the step buttons to pick its track (right-click to hear it), then click steps: off → on → accent.
3. **A–H** pick a pattern; while playing, the change waits for the end of the bar (the LED blinks).
4. The LCD sets the pattern's length, step size and play order; **CLEAR** clears the track (Shift: the pattern).
5. **LEARN** (or L), click a knob or button, move an LX49 control. Pads and keys play the drums by note: 36 kick, 38 snare, 39 clap, 42/46 hats, 41/45/48 toms, 49 crash, 51 ride; velocity ≥ 100 is accented.

Patterns: `~/.local/share/k909/patterns.json`. Knobs: `~/.config/k909/panel.json`. MIDI map: `~/.config/k909/midimap.json` (in the rack, the rack's session).

| Doc | Contents |
| --- | --- |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, MIDI notes, registry entry |
| [DSP.md](docs/DSP.md) | How each voice is synthesized |
| [SEQUENCER.md](docs/SEQUENCER.md) | Pattern data, playback, shuffle, editing, storage |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones D0–D6 |
