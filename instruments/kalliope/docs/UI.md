# Kalliope: panel

A fixed-size panel drawn at **1600 × 1100** design pixels, scaled uniformly to the window (as k303 and K909). Near-black chassis, orange section headers with black lettering, white silkscreen, vertical sliders with a 0–10 scale, black rotary selectors with white pointers, square colored function buttons with LEDs, a red seven-segment patch display, and a two-octave keyboard.

Branding: "Krate" wordmark top left, **KALLIOPE** large top right, "Polyphonic Synthesizer" under it. No third-party names, logos or artwork.

## Layout

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ Krate                                                                   KALLIOPE              │
│                                                                POLYPHONIC SYNTHESIZER         │
├─────┬────────────┬────────┬────────┬────┬───┬────────────────────┬──────┬───────────┬────────────┤
│ LFO │VCO MODULATOR│ VCO-1  │ VCO-2  │MIX │HPF│        VCF         │ VCA  │   ENV-1   │   ENV-2    │
│ ┃ ┃ ◯│ ┃  ┃  ▭  ┃  ▭ │ ┃  ◯    │ ◯   ◯  │ ◯  │ ┃ │ ┃ ┃ ▭ ┃ ▭ ┃ ┃      │ ┃  ┃  │ ┃ ┃ ┃ ┃   │ ┃ ┃ ┃ ┃    │
│     │             │    ◯   │ ◯   ▭  │    │   │                    │      │           │            │
├─────┴────────────┴────────┴────────┴────┴───┴────────────────────┴──────┴───────────┴────────────┤
│ ARPEGGIO           KEY MODE           PATCH        BANK · PATCH NUMBER          FX                │
│ [ARP][HOLD] ◯ ◯ ◯  [POLY][UNI][SOLO]  [ 88. ]      (P5)                        [CHORUS] ◯ ◯ ◯     │
│                     ◯ velocity ◯ bend                                            delay time fb   │
├──────────────────────────────────────────────────────────────────────────────────────────────┤
│ VOLUME ◯  TEMPO ◯  ┌────────────── two-octave keyboard ──────────────┐   UNMAP AUDIO LEARN       │
│ OCTAVE ◂ C3 ▸      └──────────────────────────────────────────────────┘   status · meter          │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

Upper deck (sound) 250–540, function deck 560–760, keyboard deck 780–1076. The sound sections are drawn from one model in `Panel.qml` (key, label, kind, column, row), so moving a control is a one-line change.

## Controls

| Control | Kind | Notes |
| --- | --- | --- |
| Continuous parameters | `PanelSlider` (new shared control): vertical, 0–10 scale, drag or wheel, double-click resets; learnable | LFO, modulator amounts, cross mod, HPF, VCF, VCA, envelopes, fine tune, mix |
| Range and waveform selectors | `Knob` with detents and the positions written around it (from the parameter's own display text) | VCO ranges and waveforms; the LFO waveform shows its position under the knob (the names don't fit around it); Mix and Fine Tune are continuous knobs |
| Two- and three-position switches | `PanelSwitch` (new shared control): small vertical toggle, its position written under it | mod dest, PWM source, slope, env source, sync (Env Key Follow is on the parameter list and learnable; it gets a panel switch with the patches in P5) |
| Function buttons | `PanelButton` with a `tint` (new property) and LED | Arp and Hold (on/off), Poly / Unison / Solo (one lit; they learn as one control), Chorus (cycles Off, I, II); banks, patches, Manual, Write in P5 |
| Patch display | Red seven-segment readout (QML, our own drawing) | Bank and patch, a dot while edited; ▲▼ step through patches (P5; shows `--` until then) |
| Arp, delay, play and master knobs | `Knob`, value under it | Arp mode/range/rate, delay level/time/feedback, velocity, bend range, Tempo (standalone), Volume |
| UNMAP, AUDIO, LEARN | As k303 and K909 | Plus the audio status line and output meter |
| Keyboard | `Keyboard.qml`: two octaves, ◂ OCTAVE ▸ (lowest C from C1 to C6) | Mouse: press and glide; keys held from MIDI light up too. Notes go to the engine through a lock-free queue |

Keyboard shortcuts (standalone): Space run/stop (for the arpeggiator), L learn, Esc leaves learn, Z/X octave down/up.

## Rack

Loaded and scaled to fit like the other panels; Tempo hidden when `!Kalliope.ownsTransport`. The keyboard stays: it is useful with the mouse, and it shows what the LX49 is playing.

## Accessibility

As K909: every slider, selector and button named from the descriptor table, values as descriptions, the display named "Patch".
