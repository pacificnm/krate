# K909

A drum machine for the [Krate](../../README.md) suite: fifteen synthesized drum voices (the classic eleven plus Metal, Clank, Noise and Boom for industrial sounds), a sample voice for your own WAV files, a per-voice mixer with drive, a gated reverb, delay, bus compressor and crusher, 16 kits, a tracks × steps sequencer with accent and shuffle, and a hardware-style panel. Runs standalone (`k909`) and in the Krate rack (default MIDI channel 10).

Status: v1 (D0–D6). Playable standalone (`pw-jack ./build/bin/k909`) and in the Krate rack (`pw-jack ./build/bin/krate`, MIDI channel 10). Patterns are saved to `~/.local/share/k909/patterns.json`, MIDI mappings to `~/.config/k909/midimap.json` (standalone) or the rack session. `./build/bin/k909-render --pattern 0` renders a factory pattern. `pw-jack ./build/bin/k909` plays from MIDI notes (pads on channel 10 or any channel); `./build/bin/k909-render --out-dir build` writes `k909-voices.wav` and a demo `k909-beat.wav`. See [docs/BRIEF.md](docs/BRIEF.md) and the [roadmap](docs/ROADMAP.md).

## Quick start

```sh
pw-jack ./build/bin/k909      # standalone; or pw-jack ./build/bin/krate for the rack
./build/bin/k909-render --out-dir build            # every voice, and a demo beat
./build/bin/k909-render --pattern 0 --out a.wav    # a factory pattern through the sequencer
```

1. Press **START** (or Space): pattern A plays.
2. Click a drum name under the step buttons to pick its track (right-click to hear it), then click steps: off → on → accent.
3. **A–H** pick a pattern (**I–P**: the industrial ones); while playing, the change waits for the end of the bar (the LED blinks).
4. The **KIT** display's ▲▼ load a kit: try **2 Industrial** on pattern **I**. **MIX** shows each voice's pan, drive and effect sends.
5. The LCD sets the pattern's length, step size and play order; **CLEAR** clears the track (Shift: the pattern).
6. **LEARN** (or L), click a knob or button, move an LX49 control. Pads and keys play the drums by note: 36 kick, 38 snare, 39 clap, 42/46 hats, 41/45/48 toms, 49 crash, 51 ride, 56 metal, 54 clank, 52 noise, 34 boom, 60 sample; velocity ≥ 100 is accented.

Patterns: `~/.local/share/k909/patterns.json`. Kits: `~/.local/share/k909/kits.json`. Samples: `~/.local/share/k909/samples/` (LOAD, or drop WAV files on the panel). Knobs: `~/.config/k909/panel.json`. MIDI map: `~/.config/k909/midimap.json` (in the rack, the rack's session).

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): every control, what it does and how to use it, with recipes and troubleshooting. In the app, **HELP** on the panel opens it (with a search).

| Doc | Contents |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [BRIEF.md](docs/BRIEF.md) | Scope, parameters, MIDI notes, registry entry |
| [DSP.md](docs/DSP.md) | How each voice is synthesized |
| [SEQUENCER.md](docs/SEQUENCER.md) | Pattern data, playback, shuffle, editing, factory patterns, storage |
| [KITS.md](docs/KITS.md) | Kits, the factory kits, storage |
| [UI.md](docs/UI.md) | Panel layout and controls |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones |
