# Krimson: panel

A fixed-size panel drawn at **1600 × 900** design pixels, scaled to the window. A red body ("Krimson", "Virtual Analog Synthesizer") with a black control panel: grey section frames with white silkscreen, black knobs with LED rings (value and morph range), small square buttons with red LEDs, red seven-segment displays, and a keyboard along the bottom. All names and artwork are our own.

## Layout (left to right, as the reference)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ MASTER ◯   ┌ARPEGGIATOR┐┌VIBRATO┐┌MOD ENV┐┌──OSCILLATORS──┐┌──FILTER──┐┌AMP ENV┐┌DELAY·FX┐┌OUTPUT┐  Krimson │
│ MORPH [W][V]│RUN ◯rate  ││LFO 1  ││ ◯A    ││◯LFO1  [wave]  ││◯LFO amt  ││ ◯A    ││◯tempo  ││◯level│  VIRTUAL │
│ PROGRAM [11]│range hold ││◯rate  ││ ◯D    ││◯mod ENV [88]  ││◯env amt  ││ ◯D    ││◯fb     ││◯pan  │  ANALOG  │
│ [A][B][C][D]│VOICE MODE ││◯amt   ││ ◯amt  ││◯OSC2  ◯mix    ││◯freq ◯res││ ◯S ◯R ││CHORUS  ││REVERB│          │
│ OCTAVE ◂ ▸  │◯glide     ││LFO 2  ││ dest  ││ sync fm ring  ││type drive││ vel   ││◯rev    ││      │          │
├─────────────┴───────────┴┴───────┴┴───────┴┴───────────────┴┴──────────┴┴───────┴┴────────┴┴──────┴──────────┤
│ wheels  │                         keyboard (mouse-playable, lights MIDI notes)                                │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

Exact coordinates are settled in C4 against screenshots. A section model in `Panel.qml` (key, label, kind, position) drives the controls, as in Kalliope.

## Controls

| Control | Kind |
| --- | --- |
| Knobs | A new shared `RingKnob`: a black knob with an LED ring for its value, plus a second, dimmer arc showing the morph range when one is set |
| Selectors | Buttons that step through a list, with LEDs beside the choices (wave, filter type, LFO wave and destination), as on the reference |
| Displays | Program (bank 1–8 and program 1–8, a dot while edited), OSC 2 semitone, the edited slot |
| Slot buttons A–D | Lit = edited and playing; Shift-click to layer |
| Wheels | Pitch bend and mod wheel shown (from MIDI), mouse-draggable |
| Keyboard | As Kalliope's, with the octave buttons |

Rack: Tempo hidden in the rack, the rest as standalone.
