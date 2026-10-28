# K909: panel

A fixed-size hardware-style panel drawn at **1600 × 940** design pixels and scaled uniformly to the window (the same `Scale` approach as k303's `Main.qml`). Light warm-grey chassis, dark grey section headers with orange labels, black knobs with an orange pointer, cream step buttons with a dark inset, orange pattern buttons. Built from the shared `Krate` QML module (`Knob`, `PanelButton`, `Led`, `LcdField`, `PanelLabel`, `LearnOverlay`, `Theme`).

Branding: "Krate" wordmark top left, **K909** large, **Drum Machine** on the right. No third-party names, logos or artwork.

## Layout

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│  Krate                                                                                          │
│  K909                                                                    DRUM MACHINE           │
├─────────┬───────────┬───────────┬────────┬────────┬────────┬──────┬──────┬──────────┬──────────┤
│ TOTAL   │ BASS DRUM │ SNARE DRUM│LOW TOM │MID TOM │ HI TOM │ RIM  │ CLAP │  HI HAT  │  CYMBAL  │
│ ACCENT  │ TUNE LEVEL│ TUNE LEVEL│TUNE LVL│TUNE LVL│TUNE LVL│LEVEL │LEVEL │CH LV OH LV│CR LV RD LV│
│  (◯)    │ (◯)  (◯)  │ (◯)  (◯)  │(◯) (◯) │(◯) (◯) │(◯) (◯) │ (◯)  │ (◯)  │(◯)   (◯) │(◯)   (◯) │
│         │ ATK  DECAY│ TONE SNAPY│ DECAY  │ DECAY  │ DECAY  │      │      │CH DC OH DC│CR TN RD TN│
│         │ (◯)  (◯)  │ (◯)  (◯)  │  (◯)   │  (◯)   │  (◯)   │      │      │(◯)   (◯) │(◯)   (◯) │
├─────────┴───────────┴───────────┴────────┴────────┴────────┴──────┴──────┴──────────┴──────────┤
│ [A][B][C][D]   ┌──────────────────────────────┐   TEMPO    VOLUME   SHUFFLE  UNMAP AUDIO LEARN ABOUT│
│ [E][F][G][H]   │ PATTERN [Pattern A      ]▲▼  │   (◯)      ( ◯ )     (◯)      [ ]   [●]   [●]   [ ] │
│                │ LENGTH  [16 steps       ]▲▼  │                                                   │
│                │ SCALE   [1/16           ]▲▼  │   ▬▬▬▬▬▬ level meter ▬▬▬▬▬▬                       │
│                └──────────────────────────────┘                                                   │
├────────┬────────────────────────────────────────────────────────────────────────────────────────┤
│        │  ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ●   step LEDs: playhead, and the selected track's steps   │
│ START  │ [1][2][3][4] [5][6][7][8] [9][10][11][12] [13][14][15][16]                              │
│ STOP   │ ▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬     │
│ CLEAR  │ BASS DRUM │SNARE DRUM│ LOW TOM │ MID TOM │ HI TOM │ RIM  CLAP │ CLOSED OPEN │CRASH RIDE │
└────────┴────────────────────────────────────────────────────────────────────────────────────────┘
```

The reference layout's timing-scale strip above the step buttons (note symbols) is replaced by the LED row, which shows both the playhead and the selected track's steps.

## Controls

| Control | Function |
| --- | --- |
| Voice knobs | The parameters in [BRIEF.md](BRIEF.md#parameters); each knob is MIDI-learnable and right-click resets it to its default |
| A–H | Select pattern (learnable as actions `patternA`…`patternH`) |
| PATTERN / LENGTH / SCALE displays | Current pattern name, its length 1–16, steps per beat; ▲▼ to change |
| TEMPO | Standalone only; hidden in the rack (the transport bar owns tempo) |
| VOLUME, SHUFFLE | Output level; swing amount |
| UNMAP, AUDIO, LEARN, ABOUT | As k303: clear all mappings, audio status (click to reconnect), MIDI Learn mode, about box |
| START / STOP | Run and stop the transport (standalone; hidden in the rack). Learnable as `startstop`. Space also toggles |
| CLEAR | Clear the selected track; Shift+click clears the whole pattern |
| Step buttons 1–16 | Toggle the selected track's step: off → on → accent. Lit steps show on, bright steps show accent |
| Track strip | Click a voice name to select its track (it lights orange). Each name also auditions the voice when clicked with the right button |

Keyboard (standalone window): Space run/stop; A–H select a pattern; ↑ ↓ change the selected track; L toggles learn; Esc leaves learn. In the rack, Space, L and Esc belong to the rack window, so only ↑ ↓ and A–H apply.

## Rack

In the rack, the panel is loaded by `Main.qml`'s Loader like the others and scaled to fit (K909's panel is taller than k303's, so it scales down further). Its own Tempo and START/STOP are hidden when `!Drums.ownsTransport`, the same rule as k303.
