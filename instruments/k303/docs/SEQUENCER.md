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

Time comes from the transport ([SHARED_FOUNDATIONS.MD](../../../docs/SHARED_FOUNDATIONS.MD#transport)): every block carries `playing`, `restarted`, `bpm` and `beatAtBlockStart`. Each sample's beat is `beatAtBlockStart + i · bpm / 60 / sampleRate`.

- `krate::StepClock` turns beats into steps: `stepPosition = anchorPos + (beat − anchorBeat) · stepsPerBeat`; a step starts when `floor(stepPosition)` increases. Steps are 16ths by default (4 per beat; see Scale below).
- Gate-off happens when the step's fractional position reaches `gateLength = 0.5` (*starting value*; constant, not a panel control).
- Tempo changes need nothing special: the beat keeps moving, only faster or slower, so the current step's fraction is kept. Scale changes re-anchor the clock at the current position, which also keeps the fraction.
- `krate::Transport` computes beat position from the sample count since the last anchor (play, or a tempo change), not by summing per block, so it never drifts (tested over an hour at 133 BPM).
- Events land on their exact sample: the sequencer is ticked once per sample inside the block.
- Two sequencers on one transport step together sample for sample (tested), which is what the Krate rack relies on.

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

## Scale and play mode

**Scale** sets the step length: 1/16 (4 steps per beat), 1/32 (8) or 1/16T, sixteenth-note triplets (6). `samplesPerStep = sampleRate · 60 / (bpm · stepsPerBeat)`; changing it mid-step keeps the step's phase, like a tempo change. The beat LED lights on the first step of each beat at any scale.

**Play mode** sets the step order. The next step is chosen when a step starts, so slide and tie rules (which look at the next step) still work in every mode.

| Mode | Order (4-step pattern) |
| --- | --- |
| Forward | 0 1 2 3 0 1 … |
| Reverse | 3 2 1 0 3 2 … |
| Fwd+Rev | 0 1 2 3 2 1 0 1 … (end steps not repeated) |
| Invert | Forward order; each note mirrored around its octave's C: `n → (12 − n) mod 12`, so C stays C, D# ↔ A, G ↔ F, the top C plays as the bottom C |
| Random | Uniformly random step each time (realtime-safe xorshift generator) |

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
| `LoadPattern` | full `Pattern` (used on pattern switch, clear and startup) |
| `LiveNoteOn` / `LiveNoteOff` | MIDI note: Keyboard mode plays the voice directly; ignored while the sequencer runs |

Editing rules: setting a note on a rest step turns it into a Note. Octave stays within −1..+1. Edits are sent only when they change something.

Transport and tempo don't use the queue: in the standalone app, `Synth.playing` and the Tempo knob set the `krate::Transport` owned by `Synth`, which the JACK callback advances every block and passes to the engine. The engine never reads the Tempo parameter; the standalone app forwards it (including MIDI-mapped tempo changes) to its transport. The playhead is reported back through `Engine::currentStep()` → `Synth.currentStep`, polled at 60 Hz for display only.

Edits while playing apply from the next step boundary that reads that step. No locks, no double-buffer swap needed: the audio thread copies the step into its own pattern when it drains the queue.

## Pattern storage

`~/.local/share/k303/patterns.json`, written on quit and 2 s after the last edit (debounced, GUI thread, write to temp file then rename).

```json
{
  "version": 2,
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

64 patterns, 8 banks (A–H) of 8, in order: A1 … A8, B1 … H8; a file from before banks (8 patterns) loads as bank A, the rest empty. Unknown keys are ignored and values are clamped; a missing or unreadable file loads the defaults: four factory riffs in patterns 1–4 (Demo, Rolling, Twelve — a 12-step loop — and High), the rest empty (all `Rest`). A newer `version` than supported is not overwritten: the app warns and saves to `patterns.json.v1` instead.

Versions: v2 has the same format as v1. Loading a v1 file adds factory riffs 2–4 to its first empty slots (a slot with any note in it is never touched); the next save writes v2, so this happens once.

**Randomize** replaces the current pattern with a random riff in C minor pentatonic (plus the top C): the first step is always a note, ~12% rests and ~10% ties, mostly the middle octave, ~25% accents, ~20% slides. Length is kept.
