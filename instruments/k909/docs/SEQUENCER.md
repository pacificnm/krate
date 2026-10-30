# K909: sequencer

Runs on the audio thread, sample-accurately, on the shared `krate::StepClock` (as k303's does). Standalone it follows the app's transport; in the rack, the rack's transport bar.

## Pattern data

```
Pattern (one of 16 bank slots, A..P)
  length       1..16 steps
  scale        steps per beat: 4 (1/16, default), 8 (1/32), 3 (1/8T), 6 (1/16T)
  order        Forward, Reverse, Fwd+Rev, Random (krate::StepClock::Order)
  tracks[15]   one per voice, in panel order: BD SD LT MT HT RS CP CH OH CR RD ME CK NZ BM
    steps[16]  0 = off, 1 = on, 2 = accent, 3 = flam
  velocity[15][16], tune[15][16], decay[15][16]   step details, 0 = none (below)
```

16 patterns in two pages of eight: A–H and I–P (the A–H / I–P buttons switch which the pattern buttons show; choosing a pattern shows its page). Total Accent, Shuffle and Tempo are panel knobs (they belong to the sound, not the pattern), as on the original style of machine.

## Playback

On each step start, every track whose step is on triggers its voice on that exact sample (accented if the step is 2). Closed Hat on the same step as Open Hat wins (the open hat is choked).

**Shuffle** delays every second step in time (the 2nd, 4th, … step played since START, whatever the play order) by `shuffle/5 × 1/3` of a step, in 6 positions from straight to fully swung. It uses `StepClock`'s gate point as the delayed trigger, so it stays sample-accurate at any tempo. Steps keep their position in the pattern; only the trigger moves.

**Velocities.** A sequenced step plays at velocity 0.8; an accented step at 1.0 plus Total Accent (docs/DSP.md#accent-and-velocity); a step with its own velocity plays at that (still plus Total Accent if accented).

## Step details

Every step of every track can carry, besides its state:

| Detail | What it does | Stored |
| --- | --- | --- |
| **Flam** (the 4th step state) | A grace hit on the step at 60 % of the step's velocity, then the main hit 18 ms later (the same voice, retriggered without a click) | `3` in the track string |
| **Velocity** | The hit's velocity, 1–127, instead of the pattern's 0.8 / 1.0 | 1..127; 0 = none |
| **Tune lock** | The voice's TUNE knob (Tone for Noise) for this step only | 1 + round(value × 254); 0 = none |
| **Decay lock** | The voice's DECAY knob for this step only (crash and ride have none) | as tune |

A pattern without any details plays exactly as before they existed (the reference renders are byte-identical). On the panel: step buttons cycle off → on → accent → flam (blue); **right-click** a step for its editor (VELOCITY, TUNE, DECAY; a knob shows the voice's value in orange until moved, then the step's own in blue; double-click puts it back; CLEAR clears the step's details); a small dot marks steps with details. CLEAR (track or pattern) clears details too.

**Play order and scale** are stored per pattern and run on `StepClock` (Forward, Reverse, Fwd+Rev, Random), the same clock as k303's.

**Pattern change while playing** waits for the end of the current pass (every `length` steps since START), so a change never cuts a bar short; the new pattern starts from its own first step (`StepClock::restartPass`). Choosing the playing pattern again cancels the change. While stopped, a change is immediate. The waiting pattern can be edited before it starts: every edit names its bank slot, and the engine applies it to the playing or the waiting pattern, whichever that is.

## Editing (GUI → audio)

The controller (`Drums`) keeps the GUI's copy of the bank and sends every edit to the engine through lock-free queues: `Engine::post` for commands (`SetStep`, `SetLength`, `SetStepsPerBeat`, `SetOrder`, each naming its slot, and `Trigger` for audition) and `Engine::postPattern` / `postReplace` for whole patterns (A–H, Clear). The engine publishes the playing step and the playing and waiting slots in atomics that the controller polls at 60 Hz (step LEDs, pattern LEDs). QML never owns timing.

| Action | How |
| --- | --- |
| Select track | Click a voice name in the track strip under the step buttons, or ↑ ↓ |
| Toggle a step | Click step 1–16: off → on → accent → flam → off |
| Step details | Right-click a step: velocity, tune lock, decay lock |
| Pattern | The pattern buttons (A–H or I–P, as the page switch shows), keys A–H (on the page shown), or the PATTERN LCD ▲▼. The dark button is the pattern shown; its LED is the one playing, blinking while it waits for the end of the pass |
| Length, scale, order | LCD ▲▼ |
| Clear | Clears the selected track; Shift+Clear clears the whole pattern |
| Run/stop | START and STOP buttons (standalone; in the rack they follow the rack's transport, as k303's are hidden) |

## Factory patterns

| Slot | Name | Groove |
| --- | --- | --- |
| A | Floor | Four on the floor, offbeat open hats, clap on 2 and 4 |
| B | Broken | Syncopated kick, snare with ghost notes, busy hats |
| C | Groove | Made for Shuffle: ride and toms |
| D | Fill | A one-bar tom and snare fill into a crash |
| I | Stomp | Four on the floor, a boom on the downbeat, metal and clank on the offbeats |
| J | Drive | The kick on every eighth, snare and clap together, accented sixteenth hats |
| K | Anvil | A broken kick under struck metal and clank |
| L | Machine Gun | A snare roll into the bar, noise stabs, rim ticks |
| M | Half-Time | Heavy and slow, the boom under the kick |
| N | Clatter | Clank running through on sixteenths |
| O | Gated Floor | Made for the Gated 80s kit: a straight beat with a tom fill |
| P | Breakdown | The boom twice a bar, metal on the beat, a crash and a noise wash |

They sound like the classic machine on the Classic kit; load an industrial kit (KITS.md) for their intended sound.

## Storage

`~/.local/share/k909/patterns.json`, saved 2 s after the last edit and on quit, through `krate::JsonFile` (`app/PatternFile`, as k303's). The first run seeds A–D with the classic factory patterns (Floor, Broken, Groove, Fill), E–H start empty, and I–P hold the industrial ones (below). An older file of 8 patterns keeps them and gains I–P. `current` is the pattern shown when K909 starts.

```json
{
  "version": 1,
  "current": 0,
  "patterns": [
    { "length": 16, "scale": 4, "order": 0,
      "tracks": { "bd": "2000100020001000", "cp": "0000200000002000", "ch": "1010101010101010" } }
  ]
}
```

Tracks are strings of `0`/`1`/`2`/`3`, one character per step, keyed `bd sd lt mt ht rs cp ch oh cr rd me ck nz bm`; empty tracks are left out and missing ones are empty. Out-of-range values are clamped. Unknown keys are ignored so the format can grow. Step details are optional objects keyed by track, each an array of 16 numbers, written only where a track has any: `"velocity": { "sd": [0, 0, 0, 0, 90, …] }`, `"tune"`, `"decay"` (older files simply have none).
