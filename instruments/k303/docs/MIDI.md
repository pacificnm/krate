# MIDI

MIDI arrives on a JACK MIDI input port (`k303:midi_in`). Under PipeWire, USB MIDI devices such as the LX49 appear as JACK MIDI ports automatically. Events are read in the process callback with their frame offsets and applied at that exact sample. There is no separate MIDI thread.

## Handled messages (v1)

| Message | Effect |
| --- | --- |
| Control Change (any channel) | If MIDI Learn is armed: bind this CC to the armed parameter. Else if the CC is mapped: set that parameter to `value / 127` |
| Note On (velocity > 0) | Plays the voice directly (monophonic, last-note priority) while the sequencer is stopped. Pressing a key while another is held slides to it. **Velocity ≥ 100 plays the note accented.** Always pushed to the GUI as `NoteOn` (for step record, later) |
| Note Off / Note On velocity 0 | Releases the voice if it is the sounding note; falls back to the previously held key if one is still down (slides back to it) |
| Everything else | Ignored in v1. MIDI clock (0xF8, 0xFA, 0xFB, 0xFC) is handled in a later milestone |

Channel: v1 listens on all channels (omni). A channel filter is a later setting. Keys are ignored while the sequencer runs; starting the sequencer forgets held keys.

Auto-connect: at startup, and again whenever JACK ports appear (e.g. the controller is plugged in later), `midi_in` connects to **every** MIDI source whose name matches `inputPortMatch` (case-insensitive regex). The LX49 mk3 has two USB MIDI ports, `Impact LX49mk3 MIDI1` (keys, knobs, faders, pads) and `Impact LX49mk3 MIDI2` (control and transport buttons); both match the default "Impact LX49". The status line under the pattern buttons shows "MIDI in" or "no MIDI".

## MIDI Learn

```
 [Learn] toggle on ─► click a knob ─► knob armed (pulsing outline)
        ─► turn any hardware knob ─► audio thread sees CC
        ─► ccMap[cc] = param, learnTarget = −1, push CcLearned
        ─► GUI shows the binding, saves midimap.json
```

- Learn mode is a panel toggle (MIDI LEARN button, or L; Esc leaves it). In learn mode a click on any knob or switch arms it instead of changing it; clicking the armed control again disarms it. Learnable: the six sound knobs, Tempo, Scale, Play Mode, Volume and the Waveform switch. Learn mode stays on after a binding, so you can map several controls in a row.
- Implementation: `MidiLearn` QML singleton (`MidiLearnController`) plus a `LearnOverlay` inside each learnable control. The overlay shows a blue outline (pulsing while armed) and a "CC n" badge, and takes the clicks only while learn mode is on.
- The first CC that arrives while a parameter is armed binds to it. The binding is done in the audio thread so the next CC of that same turn already moves the knob.
- One CC drives one parameter. Binding a CC that was already mapped elsewhere moves it (the old binding is removed). A parameter may have several CCs.
- In learn mode each mapped control shows its CC number as a small badge.
- Right-click in learn mode clears that control's mappings. The UNMAP button (upper left) clears all of them.
- Switches (`waveform`) map CC values < 64 → first position, ≥ 64 → second.
- Knob pickup: v1 jumps (the parameter takes the CC value immediately). Soft takeover is a later option.

## Mapping file

`~/.config/k303/midimap.json`, written whenever a binding changes (GUI thread, temp file + rename).

```json
{
  "version": 1,
  "inputPortMatch": "Impact LX49",
  "mappings": [
    { "cc": 21, "param": "cutoff" },
    { "cc": 22, "param": "resonance" }
  ]
}
```

- `param` is the stable string id from [DSP.md](DSP.md#parameters). Unknown ids and CC numbers outside 0–127 are skipped with a warning.
- `inputPortMatch` is a regular expression matched against JACK port names to auto-connect `midi_in` at startup. Empty = no auto-connect.
- The CC numbers above are examples, not the LX49's actual defaults.

## Nektar Impact LX49 mk3 setup

The LX49 mk3 has a DAW integration mode that can send proprietary messages instead of plain CCs. For k303, use the controller's standalone/preset mode so the knobs send plain Control Change messages.

Check before debugging the app:

```sh
aconnect -l                 # find the LX49 client number
aseqdump -p <client>        # turn knobs: expect "Control change" lines
pw-jack jack_lsp -t | grep -i midi   # LX49 should be listed as a JACK MIDI port
```

If `aseqdump` shows SysEx instead of Control Change lines, the controller is still in DAW mode. Record the actual mode name and the default CC numbers here once checked on the hardware (open question in [DECISIONS.md](DECISIONS.md)).

## Later

- External MIDI clock sync (see [SEQUENCER.md](SEQUENCER.md#transport)).
- Step record: select a step, play a key, the step takes that pitch and the cursor advances.
- Live transpose of the playing pattern from the keyboard.
- Channel filter and soft takeover.
