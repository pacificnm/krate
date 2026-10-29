# Kalliope: step sequencer and pattern generator

Besides playing notes directly and arpeggiating them, Kalliope has a 16-step pattern sequencer and a pattern generator that writes patterns for it. **Play Mode** chooses what the keys do: **NOTE** (play the voices), **ARP** (feed the arpeggiator, PLAY.md#arpeggiator) or **SEQ** (record and transpose sequencer patterns).

## Patterns

8 patterns, each 1–16 steps. A step is one of:

| Step | Plays |
| --- | --- |
| Notes | 1 to 4 notes at once (a single note or a chord) |
| Rest | nothing |
| Tie | holds the previous step's notes through this step |

Patterns belong to the instrument, not to a patch: changing patch keeps the pattern playing with the new sound.

## Playback

On the shared `krate::StepClock`, ticked every sample from the transport's beat position (the arpeggiator's clock; they never run at once). **Rate** is the arpeggiator's (1/8, 1/16, 1/8T, 1/16T). Each step's notes last half a step, or into the next step when it is a tie. Steps land on their exact sample.

- **Standalone**, START (or Space) runs and stops the transport. **In the rack**, the sequencer runs while the rack's transport plays.
- **Transpose**: while the sequencer runs, a key transposes the pattern by its distance from middle C (MIDI 60); the keys themselves don't sound. Stopping the transport keeps the last transposition.
- Notes go through the current Key Mode (Poly lets chords and tails overlap; Solo and Unison play one note at a time, the top note of a chord).

## Recording (step by step)

With SEQ selected and the transport stopped, the BANK and PATCH rows become step buttons 1–8 and 9–16:

| Action | How |
| --- | --- |
| Pick a step | Click its button: it blinks (the record cursor) |
| Write notes | Play a note or chord on the keyboard (on screen or MIDI): when every key is released, the notes held go into the step, and the cursor moves on |
| Rest / Tie | REST or TIE writes that step and moves on |
| Length | Shift-click a step to make it the last one |
| Pattern | The display shows `P1` … `P8`; ▲▼ choose |
| Clear | CLEAR empties the pattern (16 rests, length 16) |

Lit step buttons have notes, dim ones are ties, dark ones rests; while playing, the current step is white. Keys still sound while recording, so you hear what you write.

## Pattern generator

GENERATE writes a new pattern into the current one (which you can then edit or record over):

- **Notes** come from the keys held when GENERATE is pressed; with none held, from the notes already in the pattern; with an empty pattern, from a C minor pentatonic scale around C3. They are spread over **Range** octaves (the arpeggiator's).
- **DENSITY** (knob) is the chance that a step plays (the rest are rests); about one in eight playing steps becomes a tie when it follows a note.
- Melodies move in small steps through the note pool more often than they leap; step 1 always plays.
- Each press makes a different pattern (a new seed); the generator is deterministic for a given seed, so tests can check it.

## Storage

`~/.local/share/kalliope/sequences.json`, saved 2 s after an edit and on quit:

```json
{
  "version": 1,
  "current": 0,
  "patterns": [
    { "length": 16, "steps": [[48, 51], [], "tie", [55]] }
  ]
}
```

A step is an array of notes (empty = rest) or `"tie"`; missing steps are rests. Notes are clamped to 0–127, at most 4 per step.
