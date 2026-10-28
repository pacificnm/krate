# Kalliope: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. k303's and K909's reference renders and the rack tests stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| P0 | **Skeleton** | `instruments/kalliope/` (`kalliope_dsp`, `kalliope_ui` QML module `Kalliope`), `apps/kalliope/`; parameter table (49) with formats; registry entry (channel 2); silent engine with MIDI Learn; `pw-jack ./build/bin/kalliope` opens the panel frame with section headers and labels; `tst_kalliope` |
| P1 | **One voice** | `shared/dsp/` with the PolyBLEP oscillator and half-band decimator moved from k303 (k303 renders byte-identical); VCO-1/2 with all waveforms, sync, cross mod, noise; mix; HPF; ladder VCF 12/24; ENV-1/2; LFO; VCA; 2× oversampling. `kalliope-render` plays a note per patch-ish preset to WAV; `tst_voice` (pitch accuracy per range, bounded at full resonance, sync and cross mod alias little, envelopes' times) |
| P2 | **Polyphony and MIDI** | 8 voices, Poly / Unison / Solo, stealing with declick, sustain, pitch bend, mod wheel, velocity; `tst_voices` (allocation, stealing, legato, sustain); `--bench` within budget; LX49 keys play chords |
| P3 | **Panel** | `PanelSlider`, `PanelSwitch`, colored `PanelButton` in the shared `Krate` module; every control placed and wired; on-screen keyboard; screenshot matches UI.md; qmllint clean |
| P4 | **Effects** | Chorus I/II and the tempo-synced ping-pong delay; renders with each; `tst_fx` (delay time follows tempo, feedback bounded, chorus stereo) |
| P5 | **Patches** | 64 patches, factory banks 1–2, select / edit dot / write / manual; Program Change; `patches.json`; `tst_patches` |
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
