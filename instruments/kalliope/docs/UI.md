# Kalliope: panel

A fixed-size panel drawn at **1600 × 1100** design pixels, scaled uniformly to the window (as k303 and K909). Near-black chassis, orange section headers with black lettering, white silkscreen, vertical sliders with a 0–10 scale, black rotary selectors with white pointers, square colored function buttons with LEDs, a red seven-segment patch display, and a two-octave keyboard.

Branding: "Krate" wordmark top left, **KALLIOPE** large top right, "Polyphonic Synthesizer" under it. No third-party names, logos or artwork.

## Layout

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ Krate                                                                KALLIOPE                 │
│                                                                      POLYPHONIC SYNTHESIZER   │
├──────┬───────────────┬───────────┬──────────┬─────┬──────┬─────────────────────┬──────┬──────────────┤
│ LFO  │ VCO MODULATOR │  VCO-1    │  VCO-2   │ MIX │ HPF  │        VCF          │ VCA  │     ENV      │
│RATE  │LFO ENV  PW    │CROSS RANGE│RANGE FINE│     │      │CUT RES SLP ENV LFO KEY│LVL LFO│ 1: A D S R  │
│DELAY │MOD MOD  MOD   │MOD  (◯)   │(◯)   (◯) │ (◯) │ ┃    │ ┃   ┃  ▭   ┃   ┃   ┃ │ ┃   ┃ │ 2: A D S R  │
│ ┃ ┃  │ ┃   ┃   ┃ ▭  │ ┃  WAVE   │ WAVE SYNC│     │      │         ▭ env src   │      │ ┃┃┃┃ ┃┃┃┃ KEY▭│
│ WAVE │ dest▭    src▭│    (◯)    │ (◯)  ▭   │     │      │                     │      │              │
│ (◯)  │               │           │          │     │      │                     │      │              │
├──────┴──────┬────────┴──┬────────┴──────────┴─────┴──────┴──┬──────────────────┴──────┴──────────────┤
│ ARP  HOLD   │ [88.]     │ KEY MODE        BANK                 │ PATCH                    FX            │
│ [■]  [■]    │  ▲▼  WRITE│ POLY UNI SOLO   [1][2][3][4][5][6][7][8]  [1]…[8]  MANUAL  CHORUS DELAY       │
│ MODE RANGE RATE  TEMPO VOLUME  UNMAP AUDIO LEARN   arp knobs, delay knobs (level, time, feedback)       │
├──────────────────────────────────────────────────────────────────────────────────────────────┤
│  ◂ OCT ▸   ┃ two-octave keyboard (mouse-playable), pitch bend / mod indicators               │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

The exact coordinates are settled in P3 against screenshots, like K909's panel. Upper deck (sound) about 0–560, function deck 560–760, keyboard 760–1100.

## Controls

| Control | Kind | Notes |
| --- | --- | --- |
| Continuous parameters | `PanelSlider` (new shared control): vertical, 0–10 scale, drag or wheel, double-click resets; learnable | LFO, modulator amounts, cross mod, HPF, VCF, VCA, envelopes, fine tune, mix |
| Range and waveform selectors | `Knob` with detents and labels around it | VCO ranges and waveforms, LFO waveform, mix (continuous knob) |
| Two- and three-position switches | `PanelSwitch` (new shared control): small vertical toggle | mod dest, PWM source, slope, env source, env key follow, sync |
| Function buttons | `PanelButton` with a `color` (new property) and LED | Arpeggio, Hold, key modes, banks, patches, Manual, Write, chorus, delay |
| Patch display | Red seven-segment readout (QML, our own drawing) | Bank and patch, a dot while edited; ▲▼ step through patches |
| Arp, delay and master knobs | `Knob` | Arp mode/range/rate, delay level/time/feedback, Tempo (standalone), Volume |
| UNMAP, AUDIO, LEARN | As k303 and K909 | Plus the audio status line and output meter |
| Keyboard | Two octaves, ◂ OCT ▸ (C1–C7 reach) | Mouse: press and glide; shows notes from MIDI too |

Keyboard shortcuts (standalone): Space run/stop (for the arpeggiator), L learn, Esc leaves learn, Z/X octave down/up.

## Rack

Loaded and scaled to fit like the other panels; Tempo hidden when `!Kalliope.ownsTransport`. The keyboard stays: it is useful with the mouse, and it shows what the LX49 is playing.

## Accessibility

As K909: every slider, selector and button named from the descriptor table, values as descriptions, the display named "Patch".
