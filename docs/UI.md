# UI

A fixed-size hardware-style panel drawn at **1600 × 692** design pixels and scaled uniformly to the window: brushed-silver upper deck, grey lower deck with a key bed. All branding is our own: the panel reads "k303", "Acid Bass" and "K-303 Acid Bass Synthesizer". No third-party names, logos or artwork are used.

## Scaling

`Main.qml`: a `Window` (default 1200 × 519, minimum 800 × 346) containing a `Panel` fixed at 1600 × 692. A `Scale` transform with `s = Math.min(width / 1600, height / 692)` sizes it, centered on a dark `desk` letterbox. All coordinates below are design pixels.

## Layout

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐ 0
│   /|/| ⊓⊔     TUNING  CUT OFF  RESONANCE  ENV MOD  DECAY  ACCENT                         │
│  [=WAVE=]       (◯)     (◯)       (◯)       (◯)     (◯)    (◯)        Acid Bass          │
│ ■ k303                                                                                   │ 140
│ UNMAP AUDIO ABOUT   TEMPO   SCALE  PLAY MODE   PATTERN [Pattern 1   ]▲▼  ┌K-303──────┐ VOLUME│
│ [▬][1][2][3][4]     ( ◯ )   (◯)     (◯) ▸FWD    LENGTH  [16 steps    ]▲▼  │Acid Bass…│ ( ◯ ) │
│    [5][6][7][8]                                ▬▬▬▬ level ▬▬▬▬           └──────────┘       │ 342
├──────┬─────────────┬───────────────────────────────────────────┬──────────────────┬───────┤
│ MIDI │ ┌KEYBOARD─┐ │ ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ●  step LEDs│                  │RANDOM-│
│LEARN●│ └─────────┘ │ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢  1–16     │                  │ IZE   │ 458
│      │             │ C  C#  D  D#  E   F  F#  G  G#  A  A#  B  C│ REST   NOTE  TIE │       │
│┌────┐│    TIME     │    ■●     ■●       ■●    ■●    ■●         │ACCENT SLIDE DOWN UP│┌────┐│
││RUN ││    [●]      │ ▢    ▢    ▢    ▢    ▢    ▢    ▢     ▢      │ [ ]   [ ]  [ ] [ ]│CLEAR││
│└STOP┘│             │                                           │      OCTAVE      │└────┘│
└──────┴─────────────┴───────────────────────────────────────────┴──────────────────┴───────┘ 692
```

| Control | Function | Status |
| --- | --- | --- |
| Waveform switch | Saw / square | M2 |
| Tuning, Cut Off Freq, Resonance, Env Mod, Decay, Accent | Sound knobs | M2 |
| Unmap / Audio / About | Clear all MIDI mappings / audio status LED, click to reconnect / about box | M6, M1, M7 |
| 1–8 buttons + red block | Pattern select 1–8; the red block is a beat LED | M5, M4 |
| Tempo knob | Sequencer tempo | M4 |
| Scale selector (1/16, 1/32, 1/16T) | Step resolution, drawn | Later |
| Play Mode selector (Forward, Reverse, Fwd+Rev, Invert, Random) | Step order, drawn | Later |
| Pattern / Length displays | Current pattern and its length, ▲▼ to change | M5 |
| Red bar under displays | Output level meter | M1 |
| Model plate | "K-303 / Acid Bass Synthesizer" | — |
| Volume knob | Output volume | M2 |
| MIDI Learn | Learn toggle + LED | M6 |
| Run/Stop | Start / stop the sequencer | M4 |
| Keyboard | On = note keys play the voice; off = note keys edit the selected step | M5 |
| Time | On = time mode: DOWN = Note, UP = Tie, ACCENT = Rest (labels above the buttons), the classic step-entry convention | M5 |
| 16-step LED row | Click selects the step to edit; playhead LED moves while running | M4, M5 |
| 13 note keys with LEDs | The lit key is the selected step's note | M5 |
| Accent / Slide / Down / Up + Octave | LEDs show the selected step's accent, slide and octave | M5 |
| Randomize | Randomize pattern | Later |
| Clear | Clear pattern | M5 |

Controls marked "drawn" render and move but do nothing until their milestone. Until M2/M5, panel state lives in local QML properties on `Panel.qml`; those move to the C++ singletons and QML then holds none.

## Components

| Component | Behavior |
| --- | --- |
| `Knob.qml` | Dark ribbed skirt, silver cap, black pointer. Only the rib and pointer layers rotate, so lighting stays fixed and nothing re-tessellates while turning. `minAngle`/`maxAngle` (default ±135°), `detents` for selectors. Vertical drag: 200 px = full range; Shift = fine (×0.1); wheel = 1/100 or one detent; double-click = default; Up/Down keys when focused. Learn mode (M6): click arms it, pulsing outline when armed, CC badge when mapped |
| `PanelButton.qml` | Hardware push button, light or dark, optional LED, optional red indicator bar (Run/Stop), optional face text. `checkable`; Enter activates when focused |
| `Led.qml` | Red LED with glow; any size or shape |
| `StepRow.qml` | 16 step buttons with LEDs and numbers. Selected step lit; playhead pulses (M4: `OpacityAnimator`, runs on the render thread) |
| `PianoKeys.qml` | 13 keys C..C on a key bed: white keys in the bottom row, black keys raised between them, LED per key, note labels |
| `LcdField.qml` | Recessed LCD readout with ▲▼ buttons |
| `WaveSwitch.qml` | Two-position slide switch with saw/square icons |
| `PanelLabel.qml` | Silk-screen text; `centerX`, `orange`, `inverse` |
| `Panel.qml` | Places everything at the coordinates above |
| `Main.qml` | Window and scaling only |
| `Theme.qml` | Singleton: design size and color tokens |

Base style: Qt Quick Controls **Basic** (set in `main.cpp` before the engine loads); every control is re-skinned.

## Colors and type

| Token | Value | Use |
| --- | --- | --- |
| `desk` | `#2A2B2E` | Letterbox around the panel |
| `silverLight` → `silverDark` | `#E6E7E9` → `#BDBEC1` | Upper deck gradient |
| `lowerDeck` | `#CDCED1` | Lower deck |
| `keyBed` | `#E9E9EA` | Key bed behind note keys and step function buttons |
| `band` | `#26272A` | Black label band, switch track |
| `buttonLight` / `buttonDark` | `#FBFBFA` / `#4A4B4E` (with shades) | Buttons and keys |
| `knobSkirt`, `knobCap` | `#2E2F32`, `#F7F7F7` (with shades) | Knobs |
| `ink` / `inkInverse` | `#1C1C1C` / `#F2F2F2` | Labels |
| `labelOrange` | `#E0781E` | OCTAVE label, Play Mode highlight |
| `ledOn` / `ledOff` | `#FF2A1A` / `#5A1E18` | LEDs, meter |
| `lcdBg` / `lcdInk` | `#D8DBD4` / `#2A2D2A` | Displays |
| `accentUi` | `#2A7FFF` | Focus ring, learn-mode outline and badges |

Labels: bold sans-serif uppercase, 11–15 px; displays: monospace.

## Binding rules

- Every control binds to a C++ singleton property (`Synth.cutoff`, `Pattern.selectedStep`, …) and writes back through it. No JavaScript holds synth state (from M2/M5 on).
- Values crossing into QML are normalized 0–1; display text (Hz, BPM, dB) comes from `Synth.displayText(paramId)`.
- The playhead LED reads `Synth.currentStep`, updated at 60 Hz by the poll timer.
- Knobs moved by MIDI update through the same property, so the panel always shows the real value.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| Space | Run / stop, from anywhere (focused buttons use Enter) |
| Left / Right | Select previous / next step |
| A, S | Toggle accent / slide on the selected step |
| N, T, R | Set the selected step to Note / Tie / Rest |
| L | Toggle MIDI learn |
| 1–8 | Select pattern |

## Accessibility

Each control sets `Accessible.name` and `Accessible.role`; knobs and buttons take keyboard focus (Tab) and show a blue focus ring, so the panel is usable without a mouse.
