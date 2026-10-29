# Kalliope: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. k303's and K909's reference renders and the rack tests stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| P0 ✅ | **Skeleton** | `instruments/kalliope/` (`kalliope_dsp`, `kalliope_ui` QML module `Kalliope`), `apps/kalliope/`; parameter table (49) with formats; registry entry (channel 2); silent engine with MIDI Learn; `pw-jack ./build/bin/kalliope` opens the panel frame with section headers and labels; `tst_kalliope`. Result: as planned; includes are `kalliope/dsp/...`, `kalliope/app/...` from the start; `Kalliope` QML singleton (Tempo and run/stop drive the standalone transport); every detented default sits on a detent and every value formats (tested); the panel's section model (key, label, kind, column) is what P3 renders controls from |
| P1 ✅ | **One voice** | `shared/dsp/` with the PolyBLEP oscillator and half-band decimator moved from k303 (k303 renders byte-identical); VCO-1/2 with all waveforms, sync, cross mod, noise; mix; HPF; ladder VCF 12/24; ENV-1/2; LFO; VCA; 2× oversampling. `kalliope-render` writes notes and sweeps for a few hard-coded sounds to WAV; `tst_voice` (pitch accuracy per range, bounded at full resonance, sync and cross mod alias little, envelopes' times). Result: `shared/dsp` holds the decimator and `polyBlep` (k303 and K909 renders byte-identical); Kalliope's own `Oscillator` corrects every jump, sync resets included, with a band-limited step; one voice played by MIDI with last-note priority, pitch bend, mod wheel vibrato; `kalliope-render` (brass, PWM pad, sync lead, cross-mod bell, resonant sweep; `--bench` 0.55% per voice); `kalliope_tst_voice` (15 tests; sync leaves 0.02% of its energy above 20 kHz) |
| P2 ✅ | **Polyphony and MIDI** | 8 voices, Poly / Unison / Solo, stealing with declick, sustain, pitch bend, mod wheel, velocity; `tst_voices` (allocation, stealing, legato, sustain); `--bench` within budget; LX49 keys play chords. Result: as planned, plus all-notes-off (CC 120/123), a fresh start phase per Poly note, DC-free pulse, −3 dB output trim; `--bench` 3.8% for 8 heavy voices; `kalliope_tst_voices` (15 tests). LX49 chords: to be confirmed on the hardware |
| P3 ✅ | **Panel** | `PanelSlider`, `PanelSwitch`, colored `PanelButton` in the shared `Krate` module; every control placed and wired; on-screen keyboard; screenshot matches UI.md; qmllint clean. Result: as planned; `ParamController::detents()`; selectors label their positions from the parameter's display text; the keyboard plays through `Engine::postNote` and lights keys held from MIDI (`noteHeld`, published every block). The patch display shows `--` until P5 |
| P4 ✅ | **Effects** | Chorus I/II and the tempo-synced ping-pong delay; renders with each; `tst_fx` (delay time follows tempo, feedback bounded, chorus stereo). Result: as planned; Kalliope is stereo now; the delay buffer is allocated at engine construction (no allocation in `prepare` or `process`); voice tests run with the chorus off; `--bench` 4.0% with both effects |
| P5 ✅ | **Patches** | 64 patches, factory banks 1–2, select / edit dot / write / manual; Program Change; `patches.json`; `tst_patches`. Result: as planned; `Patches` QML singleton, 16 factory sounds, lit bank and patch buttons, ▲▼ on the display, ARP / HOLD / WRITE / BANK / PATCH actions learnable; first run starts in Manual so an existing panel is kept; `ParamController::setValues()` loads a patch with one change notification; `kalliope_tst_patches` |
| P6 | **Arpeggiator and the rack** | Arp on `StepClock` (modes, range, rate, hold) locked to the transport; Kalliope in the Krate rack on channel 2; `krate-render --kalliope`; learn for every control and the actions; `tst_arp` |
| P7 | **Polish** | Factory patches tuned by ear; accessibility names; docs final; README quick start |

## Later

| Feature | Notes |
| --- | --- |
| Dual and Split | Two patches at once (layer, or split at a key), 4 voices each |
| Reverb | Third send effect |
| Portamento | Glide time, Solo and Unison first |
| Aftertouch | Destination: vibrato or cutoff |
| Per-voice outputs, more voices | If the CPU budget allows |
| Arp patterns from k303/K909 style steps | Only if wanted |
