# Kalliope: playing, arpeggiator, patches

## Key modes

| Mode | Behavior |
| --- | --- |
| Poly | Up to 16 notes; the 17th takes the oldest released voice, else the oldest held one |
| Unison | 8 voices on the newest note, detuned by DETUNE (±0–48 cents); legato; releasing returns to the previous held note |
| Solo | One voice, last-note priority, legato (envelopes don't retrigger while a key is held) |

## Arpeggiator

Runs on the audio thread on the shared `krate::StepClock`, ticked every sample from the transport's beat position, so it locks to the transport: the standalone app's (Tempo; Space) or the rack's transport bar. Steps and gate ends land on their exact sample.

- **Play Mode** (NOTE / ARP / SEQ) chooses what the keys do. In NOTE, keys play voices directly. In ARP, keys only feed the arpeggiator, which plays the voices. (SEQ: the step sequencer, [SEQUENCER.md](SEQUENCER.md).) Choosing ARP releases the keys' chord; going back to NOTE plays the keys still down as a chord again.
- **Standalone**, choosing ARP starts the transport if it is stopped, and leaving ARP stops it again if ARP was what started it (a transport you started by hand, with START or Space, keeps running). **In the rack**, the arpeggio runs while the rack's transport plays.
- **Mode**: Up, Down, Up & Down (the top and bottom notes are not repeated), Random. The held notes are ordered low to high, without repeats, and extended over **Range** octaves (1–4).
- **Rate**: 1/8, 1/16, 1/8T, 1/16T (2, 4, 3 or 6 steps per beat), shared with the sequencer. Each note lasts half a step.
- **Hold**: released keys stay in the arpeggio; the first key of a new chord (with no keys down) replaces them. Turning Hold on latches what is held at that moment.
- Velocity is the last key's. The arpeggio plays through the current Key Mode: Poly lets release tails overlap; Unison and Solo retrigger every note.

## Patches

64 patches: banks 1–8 × patches 1–8, shown on the display as `11` … `88` with the patch's name under it. A patch holds every parameter except Volume, Tempo and Hold (BRIEF.md#parameters), by key. The `Patches` QML singleton (`app/PatchController`) runs them.

| Action | How |
| --- | --- |
| Select | PATCH 1–8 in the current bank; or BANK n (it blinks) then PATCH m for patch nm; or the display's ▲▼ (wrapping 88 → 11); or Program Change n (patch n + 1 in order: PC 0 = 11, PC 8 = 21, …) |
| Edit | Move any control: the display shows a dot (`14.`) while the panel differs from the stored patch, and loses it if you move everything back |
| Write | WRITE (it blinks), optionally a BANK, then the PATCH number to store into; WRITE again cancels. An `Init` slot gets a name like `Patch 35`; a named one keeps its name |
| Manual | MANUAL: the panel as it is, not tied to any patch (display `--`, no dot) |

The current bank and patch buttons are lit. The panel's live state is kept in `panel.json` as for every Krate instrument; selecting a patch writes the patch's values into it.

**First run.** With no `patches.json` yet, Kalliope starts in Manual, so the panel you had stays as it was; pick a patch to load it.

**Factory patches** (banks 1–2, defined in code in `app/PatchFile.cpp`, so a reset can restore them): 11 Brass, 12 Strings, 13 Soft Pad, 14 Bass, 15 Sync Lead, 16 Bells, 17 Unison Lead, 18 Sweep Bass; 21 Organ, 22 Pluck, 23 Tremolo Keys, 24 Noise Sweep, 25 Arp Pluck (arpeggio on), 26 Detuned Saws, 27 Sub Bass, 28 Low Wobble. Banks 3–8 start as `Init` (every parameter at its default). Tuning them by ear is part of P7.

### Storage

`~/.local/share/kalliope/patches.json`, through `krate::JsonFile`: saved on every write, and 2 s after the selection changes.

```json
{
  "version": 1,
  "current": 0,
  "manual": false,
  "patches": [
    { "name": "Brass", "params": { "cutoff": 0.45, "res": 0.15, "env1_a": 0.35 } }
  ]
}
```

Parameters are stored by key, normalized; missing keys take the init patch's value, unknown keys are ignored and values are clamped (so the format survives new parameters); names are cut to 16 characters.
