# Krimson: panel

A fixed-size panel drawn at **1600 × 900** design pixels, scaled to the window. A red body ("Krimson", "Virtual Analog Synthesizer") with a black control panel: grey section frames with white silkscreen, black knobs with LED rings (value and morph range), small square buttons with red LEDs, red seven-segment displays, and a keyboard along the bottom. All names and artwork are our own.

## Layout (left to right, as the reference)

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Krate                                                                   Krimson  VIRTUAL ANALOG SYNTHESIZER│
│┌────────────────┬──────────┬───────────┬────────┬─────────────┬─────────────┬────────┬──────┬───────┐     │
││MASTER TEMPO OCT│ARP·VOICE │LFO1 · LFO2│MOD ENV │ OSCILLATORS │   FILTER    │AMP ENV │  FX  │OUTPUT │     │
││WHEEL  PROGRAM  │ARP HOLD  │◯rate ◯rate│ ◯att   │OSC1 ▪ OSC2 ▪│TYPE ▪ ◯FREQ │◯A ◯D   │CHORUS│◯level │     │
││[A][B][C][D]    │◯mode..   │◯amt  ◯amt │ ◯dec   │◯pw  ◯semi   │◯res ◯drive  │◯S ◯R   │◯dly  │◯pan   │     │
││SLOT SETUP      │VOICE ▪   │WAVE ▪ DEST│ ◯amt   │◯mix ◯fine   │◯env ◯vel    │◯vel    │◯time │       │     │
││CH ZONE SPLIT ON│◯glide◯det│DEST ▪     │ DEST ▪ │◯fm  [KBD]   │KBD ▪ fenv   │◯gain   │◯rev  │       │     │
││                │◯bend     │           │        │[SYNC][RING] │ ◯A◯D◯S◯R    │        │      │       │     │
│└────────────────┴──────────┴───────────┴────────┴─────────────┴─────────────┴────────┴──────┴───────┘     │
│ PITCH MOD │                     keyboard (mouse-playable, lights held keys)                               │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

▪ = a list of choices with an LED each, ◯ = a ring knob, [ ] = a button.

## Controls

| Control | Kind |
| --- | --- |
| Knobs | `RingKnob` (new, shared in the `Krate` module): a black knob inside an LED ring showing its value (bipolar knobs light from the top), with a second, blue arc for the morph range (from the value to the target); while MORPH is lit, the ring turns blue and the knob shows and sets its target; label above, value under |
| Choices | Waves, filter type, key tracking, LFO waves and destinations, voice mode, wheel, chorus: a list with an LED per choice; click a choice to pick it; learnable as one control |
| Switches | Sync, Ring, OSC 2 KBD, ARP, HOLD, slot ON: small buttons, red when on |
| Slot buttons A–D | Click: play and edit that slot; Shift-click: layer; LED = lit (the keyboard plays it); highlighted = the slot being edited; learnable as actions `slotA`…`slotD` |
| Slot setup | The edited slot's MIDI channel (or none), zone (whole, lower, upper), on/off, and the split key (or off) |
| MORPH, CLEAR | MORPH (blue while lit) toggles morph editing; CLEAR clears the edited slot's targets |
| Program display | The edited slot's program (`11`–`88`, a dot while edited), its name, ▲▼; STORE (red while armed: ▲▼ pick the blinking destination, STORE writes) |
| Performances | Under the wheels: PERF ◂ n ▸, LOAD, SAVE |
| Wheels | Pitch (springs back) and mod, mouse-draggable; they act on the lit slots |
| Keyboard | `PianoKeyboard` (shared with Kalliope, moved to the `Krate` module): two octaves from C3 shifted by Octave, mouse-playable, lights keys held in any slot |
| Master level, Tempo (standalone), Octave | As Kalliope's |

Every control edits the edited slot and its value comes back when you switch slots. Keyboard shortcuts (standalone): Space run/stop, L learn, Esc leaves learn.

Rack: Tempo hidden in the rack, the rest as standalone.
