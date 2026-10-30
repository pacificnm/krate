# K909: kits

A kit is a saved set of every sound and mix knob: the voices' own knobs, each voice's pan, drive and sends, and the effects (REV SIZE, GATE, DLY TIME, FEEDBACK, COMP, CRUSH). Volume, Tempo and Shuffle stay where they are. 16 kits; the first six are factory kits, the rest start as Init (every default).

## Using kits

| Action | How |
| --- | --- |
| Load | The KIT display's ▲▼: the next or previous kit loads at once. A dot after the name means you have changed a knob since |
| Write | WRITE (it blinks), ▲▼ to choose the slot (the display shows "WRITE to …"), WRITE again to store. Esc cancels. An Init slot gets a name like `Kit 8`; a named one keeps its name |

Kits and patterns are separate: load any kit under any pattern.

A kit you WRITE also remembers the SAMPLE voice's file; loading it brings that file back. The factory kits have none and leave the sample as it is.

## Samples

The SAMPLE voice plays a WAV file of your own: a hit, a vocal stab, a found sound, a loop.

| Action | How |
| --- | --- |
| Load | **LOAD** (beside the SAMPLE display) opens a file dialog; choose one or more WAV files. Or drop WAV files anywhere on the panel. The last one loaded plays |
| Choose | The SAMPLE display's ▲▼ step through the library |
| Play | Track **SAMPLE** in the sequencer, MIDI notes 60 and 61, or right-click its name in the track strip |

Loading **copies** the file into K909's sample library, `~/.local/share/k909/samples/` (a different file with the same name gets a number), so kits keep working if the original moves or is deleted. To remove a sample, delete it from that folder. Any WAV works (16, 24, 32-bit or float, any rate, mono or stereo: stereo is mixed to mono, the bass drum's way); files longer than 30 s are cut; every sample is normalized to the same peak. The sample playing is kept in `samples.json`. Files that aren't WAV (or are silent) are refused, with a message in the SAMPLE display.

## Factory kits

| # | Kit | Sound |
| --- | --- | --- |
| 1 | Classic | Every default: the original machine, exactly |
| 2 | Industrial | A low, driven, squarer kick with more click; a cracking, driven snare through the gated reverb; driven closed hats; metal (pipe), hard clank, gritty noise stabs, a driven boom; the compressor at half |
| 3 | EBM | Tight and driving: a short punchy kick, a snappy snare with a little reverb, short hats, noise stabs on a 1/8 delay, the compressor at a third |
| 4 | Gated 80s | The big chopped snare and toms: snare, toms and clap sent hard to the gated reverb |
| 5 | Crushed | Driven voices into the crusher and a pumping compressor: lo-fi and squashed |
| 6 | Machine Shop | Metal percussion: struck plates on reverb and delay, very hard clank, sweeping noise, a long boom, a cracking snare, a 1/8T delay |

Every factory kit on every factory pattern peaks under 0 dBFS before the host's soft clip (measured, 4 bars at 128 BPM).

## Storage

`~/.local/share/k909/kits.json`, through `krate::JsonFile`: saved on every write and 2 s after a kit is chosen.

```json
{
  "version": 1,
  "current": 1,
  "kits": [
    { "name": "Classic", "params": { "bd_tune": 0.5, "bd_drive": 0.0 } },
    { "name": "Kit 8", "params": { "sm_tune": 0.5 }, "sample": "kick.wav" }
  ]
}
```

Parameters are stored by key, normalized; missing keys take the default, unknown keys are ignored and values are clamped (so the format survives new parameters); names are cut to 16 characters. The factory kits are defined in code (`app/KitFile.cpp`).
