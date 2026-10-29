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
│ ARPEGGIO · SEQUENCER                KEY MODE        PATCH          BANK · PATCH NUMBER     FX         │
│ [NOTE][ARP][SEQ] [HOLD] [START][CLEAR] [POLY][UNI][SOLO] [ 88. ]▲▼ [WRITE]  [1]…[8] banks  [CHORUS] ◯ ◯ ◯ │
│ ◯mode ◯range ◯rate ◯density [GEN][REST][TIE]  ◯vel ◯bend  name     [MANUAL] [1]…[8] patches  delay    │
│                                       (SEQ: the display shows P1–P8; the two rows are steps 1–16)        │
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
| Two- and three-position switches | `PanelSwitch` (new shared control): small vertical toggle, its position written under it | mod dest, PWM source, slope, env source, sync, env key follow (in ENV-2: it shortens both envelopes) |
| Function buttons | `PanelButton` with a `tint` (new property) and LED | NOTE / ARP / SEQ and POLY / UNISON / SOLO (one lit; each trio learns as one control), HOLD, START (standalone), CLEAR, GEN, REST, TIE, CHORUS (cycles Off, I, II), WRITE, MANUAL, banks and patches (each learnable as an action) |
| Patch display | Red seven-segment readout (QML, our own drawing) | Bank and patch, a dot while edited, the patch name under it; ▲▼ step through patches. In SEQ mode: the sequencer pattern (P1–P8), ▲▼ choose it, the cursor's step under it |
| Step buttons | In SEQ mode, the BANK and PATCH rows become steps 1–16 | Yellow = notes, amber = tie, grey = rest, dim = past the pattern's length, white = playing, blinking red = the record cursor. Click to record into a step; Shift-click to make it the last |
| Arp, delay, play and master knobs | `Knob`, value under it | Arp mode/range/rate, delay level/time/feedback, velocity, bend range, Tempo (standalone), Volume |
| UNMAP, AUDIO, LEARN | As k303 and K909 | Plus the audio status line and output meter |
| Keyboard | `Keyboard.qml`: two octaves, ◂ OCTAVE ▸ (the Octave parameter, −3 … +3: the keyboard shows C0 … C6 and MIDI keys from the LX49 shift by the same amount, on top of the LX49's own octave buttons) | Mouse: press and glide; keys held from MIDI light up where they sound. Notes go to the engine through a lock-free queue. The octave buttons learn as actions `octave_down` / `octave_up`, the display as the Octave parameter |

Keyboard shortcuts (standalone): Space run/stop (for the arpeggiator), L learn, Esc leaves learn, Z/X octave down/up.

## Rack

Loaded and scaled to fit like the other panels; Tempo hidden when `!Kalliope.ownsTransport`. The keyboard stays: it is useful with the mouse, and it shows what the LX49 is playing.

## Accessibility

As K909: every slider, selector and button named from the descriptor table, values as descriptions, the display named "Patch".
