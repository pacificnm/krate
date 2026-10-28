# Kalliope: playing, arpeggiator, patches

## Key modes

| Mode | Behavior |
| --- | --- |
| Poly | Up to 8 notes; the 9th takes the oldest released voice, else the oldest held one |
| Unison | All 8 voices on the newest note, detuned; legato; releasing returns to the previous held note |
| Solo | One voice, last-note priority, legato (envelopes don't retrigger while a key is held) |

## Arpeggiator

Runs on the audio thread on the shared `krate::StepClock`, so it locks to the transport: the standalone app's (Tempo, START from the ARPEGGIO button or Space) or the rack's transport bar.

- While **Arpeggio** is on, held notes feed the arpeggiator instead of the voices. Standalone, turning Arpeggio on starts the transport (and turning it off stops it again, if Arpeggio started it); in the rack, the arpeggiator runs while the rack's transport plays.
- **Mode**: Up, Down, Up & Down (no repeated end notes), Random. Notes are ordered by pitch, over **Range** octaves.
- **Rate**: 1/8, 1/16, 1/8T, 1/16T (the clock's steps per beat). Each note is gated for half a step.
- **Hold**: released notes stay in the arpeggio; playing a new chord while none is held replaces them.
- The arpeggiator plays through the current Key Mode (Poly lets release tails overlap).

## Patches

64 patches: banks 1–8 × patches 1–8, shown on the display as `11` … `88`. A patch holds every parameter except Volume, Tempo and Hold (BRIEF.md#parameters).

| Action | How |
| --- | --- |
| Select | BANK 1–8 then PATCH 1–8 (a new bank applies with the next patch press), or Program Change, or the display's ▲▼ |
| Edit | Move any control; the display shows a dot (`1.1.`) while the patch differs from its stored version |
| Write | WRITE, then the patch button to store into (bank as selected); WRITE again cancels |
| Compare | MANUAL: the panel's live settings, ignoring the patch (as the panel reads now) |

Banks 1–2 are factory patches on first run (brass, strings, pads, bass, leads, bells, sync and cross-mod sounds, an arpeggio patch); 3–8 start as copies of an init patch. Factory patches are defined in code, so a reset can restore them.

### Storage

`~/.local/share/kalliope/patches.json`, through `krate::JsonFile`, saved when a patch is written (and the current selection 2 s after it changes):

```json
{
  "version": 1,
  "current": 0,
  "patches": [
    { "name": "Brass 1", "params": { "cutoff": 0.42, "res": 0.1, "env1_a": 0.08 } }
  ]
}
```

Parameters are stored by key, normalized; missing keys take the init patch's value, unknown keys are ignored (so the format survives new parameters). The panel's live state is in `~/.config/kalliope/panel.json` as for every Krate instrument.
