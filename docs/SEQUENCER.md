# Sequencer

A 16-step acid-bass pattern sequencer that runs inside the JACK process callback and is timed by sample count. The GUI edits it and displays it; the GUI never times anything.

## Step data

```cpp
enum class Gate : uint8_t { Note, Tie, Rest };

struct Step {
    uint8_t note   = 0;          // 0..12  = C..C (13 keys, one octave plus the top C)
    int8_t  octave = 0;          // -1 down, 0 normal, +1 up
    bool    accent = false;
    bool    slide  = false;      // slide FROM this step INTO the next
    Gate    gate   = Gate::Note;
};

struct Pattern {
    std::array<Step, 16> steps;
    uint8_t length = 16;         // 1..16
};
```

Pitch: `midiNote = 24 + 12 · (octave + 1) + note + transpose`, giving C1 (24, at octave −1) up to C4 (60, at octave +1, note 12) with `transpose = 0`. Live transpose from a MIDI key is a later feature; v1 keeps `transpose = 0`. Base note 24 is a *starting value*; adjust so the default pattern sits in the bass range.

## Timing

Steps are 16th notes.

```
samplesPerStep = sampleRate · 60 / (bpm · 4)       // double precision
```

- A `double stepPhase` counts samples into the current step. When it reaches `samplesPerStep`, subtract `samplesPerStep` (do not reset to 0) and advance. This keeps long-run timing exact at fractional step lengths.
- Gate-off happens at `gateLength · samplesPerStep` into the step, `gateLength = 0.5` (*starting value*; constant, not a panel control).
- Tempo changes recompute `samplesPerStep` and rescale `stepPhase` proportionally, so a tempo change mid-step does not skip or double a step.
- Step and gate boundaries split the audio block (see [ARCHITECTURE.md](ARCHITECTURE.md)), so events land on their exact sample.

Acceptance: with 48 kHz and 133 BPM (fractional samples per step), step *n* starts within ±1 sample of `round(n · samplesPerStep)` for n up to 100 000.

## Playback rules

Evaluated when step *n* starts, with *p* = the previous step:

| Step *n* gate | Previous step *p* | Result |
| --- | --- | --- |
| `Rest` | any | No note. If the gate was still high (tie/slide into a rest), it goes low now |
| `Note` | *p* had `slide` and *p* was `Note`/`Tie` | **Slide**: gate stays high, pitch glides to *n*'s note, envelopes not retriggered |
| `Note` | otherwise | **New note**: gate high, retrigger MEG and VEG, set pitch instantly |
| `Tie` | *p* sounded (Note or Tie) | Extend *p*: gate stays high through *n*, pitch unchanged, no retrigger. *n*'s note/accent fields are ignored |
| `Tie` | *p* was `Rest` or stopped | Treated as `Rest` |

Gate-off at `gateLength` happens only if step *n+1* will not continue the note (not a Tie, and *n* has no slide into a Note/Tie). Otherwise the gate stays high across the boundary.

Accent applies to the step that starts a note (new note or slide target). A tied continuation keeps the accent of the note it extends.

The pattern wraps at `length`; wrap-around follows the same rules (step `length−1` can slide or tie into step 0).

## Transport

- **Play** starts at step 0 on the next block, sample offset 0 of that block.
- **Stop** sends gate-off immediately (VEG releases) and sets `currentStep = −1`.
- Clock source v1: internal only. External MIDI clock (24 PPQN → 6 pulses per step, start/stop/continue) is a later milestone; the design leaves a `ClockSource` seam so the step-advance code takes "advance now" ticks from either source.

## Editing (GUI side)

All edits go through `Pattern` (singleton `PatternModel`): it updates its mirror and pushes a `Command` to the audio thread.

| Command | Payload |
| --- | --- |
| `SetStep` | step index, full `Step` |
| `SetLength` | 1..16 |
| `LoadPattern` | full `Pattern` (used on pattern switch and on load) |
| `Transport` | play / stop |
| `SetTempo` | BPM (also mirrored in `ParamStore` for the knob) |

Edits while playing apply from the next step boundary that reads that step. No locks, no double-buffer swap needed: the audio thread copies the step into its own pattern when it drains the queue.

## Pattern storage

`~/.local/share/k303/patterns.json`, written on quit and 2 s after the last edit (debounced, GUI thread, write to temp file then rename).

```json
{
  "version": 1,
  "current": 0,
  "patterns": [
    {
      "length": 16,
      "steps": [
        { "note": 0, "octave": 0, "accent": true, "slide": false, "gate": "note" }
      ]
    }
  ]
}
```

8 patterns. Unknown keys are ignored; a missing or unreadable file loads 8 default patterns (pattern 0 is a short demo riff, others empty: all `Rest`). A newer `version` than supported is not overwritten: the app warns and saves to `patterns.json.v1` instead.
