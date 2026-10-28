# K909: sequencer

Runs on the audio thread, sample-accurately, on the shared `krate::StepClock` (as k303's does). Standalone it follows the app's transport; in the rack, the rack's transport bar.

## Pattern data

```
Pattern
  name         "Pattern A"
  length       1..16 steps
  scale        steps per beat: 4 (1/16, default), 8 (1/32), 3 (1/8T), 6 (1/16T)
  tracks[11]   one per voice, in panel order: BD SD LT MT HT RS CP CH OH CR RD
    steps[16]  0 = off, 1 = on, 2 = accent
```

8 patterns, A–H. Total Accent, Shuffle and Tempo are panel knobs (they belong to the sound, not the pattern), as on the original style of machine.

## Playback

On each step start, every track whose step is on triggers its voice on that exact sample (accented if the step is 2). Closed Hat on the same step as Open Hat wins (the open hat is choked).

**Shuffle** delays every second step in time (the 2nd, 4th, … step played since START, whatever the play order) by `shuffle/5 × 1/3` of a step, in 6 positions from straight to fully swung. It uses `StepClock`'s gate point as the delayed trigger, so it stays sample-accurate at any tempo. Steps keep their position in the pattern; only the trigger moves.

**Velocities.** A sequenced step plays at velocity 0.8; an accented step at 1.0 plus Total Accent (docs/DSP.md#accent-and-velocity).

**Play order and scale** come from `StepClock` (Forward, Reverse, Fwd+Rev, Random), the same as k303's.

**Pattern change while playing** takes effect at the end of the current pattern pass (the next time step 1 comes round), so a change never cuts a bar short. While stopped, it is immediate.

## Editing (GUI → audio)

The controller owns the pattern bank and sends edits to the engine through the lock-free command queue (`SetStep`, `SetLength`, `SetScale`, `SelectPattern`, `LoadPattern`). The engine publishes the current step on its event queue for the step LEDs. QML never owns timing.

| Action | How |
| --- | --- |
| Select track | Click a voice name in the track strip under the step buttons (or press that voice's MIDI note while EDIT is on) |
| Toggle a step | Click step 1–16: off → on → accent → off |
| Pattern | A–H buttons; LCD shows the name |
| Length | LCD ▲▼ |
| Clear | Clears the selected track; Shift+Clear clears the whole pattern |
| Run/stop | START and STOP buttons (standalone; in the rack they follow the rack's transport, as k303's are hidden) |

## Storage

`~/.local/share/k909/patterns.json`, saved 2 s after the last edit and on quit, through `krate::JsonFile` (as k303's `PatternFile`). The first run seeds A–H with a few demo patterns (four-on-the-floor, a broken beat, a shuffle groove and so on).

```json
{
  "version": 1,
  "current": 0,
  "patterns": [
    { "name": "Pattern A", "length": 16, "scale": 4,
      "tracks": { "bd": "2000100020001000", "sd": "0000100000001000", "ch": "1111111111111111" } }
  ]
}
```

Tracks are strings of `0`/`1`/`2`, one character per step; missing tracks are empty. Unknown keys are ignored so the format can grow (flam, parameter locks).
