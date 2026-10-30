# Kobalt: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. The other instruments' tests and the rack's stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| B1 ✅ | **The voice** | `kobalt_dsp`: the parameter table (42) with formats; the oscillator with saw, pulse, sub and noise; the source mixer; the four-pole filter; the envelope with its three triggers; the LFO; VCA Env / Gate; portamento Off / Auto / On; last-note priority; bender, lever, sustain, Octave; analog drift. `kobalt-render` writes a few sounds to WAV and benchmarks; `kobalt_tst_voice` (pitch per range, the subs' pitch, the filter bounded at full resonance, the glides, the triggers, Gate on the VCA, note priority). Result: as planned, and the delay and reverb came early (B2 had them); 42 parameters; one voice at 1.1 % of a core with every source up, full resonance, Analog, delay and reverb; single notes peak around −10 … −20 dBFS (no headroom kept for chords); `kobalt_tst_voice` (27 tests: the filter self-oscillates at the cutoff, the LFO trigger fires the envelope once a cycle, Gate holds the VCA open) |
| B2 ✅ | **Panel, patches, effects, the rack** | The `Kobalt` QML module: the panel (sliders, switches, bender section, patches, keyboard deck), 64 patches with 16 factory sounds, delay and reverb; the `kobalt` standalone app; Kobalt in the Krate rack on channel 7; `tst_kobalt`, `kobalt_tst_patches`. Result: as planned (the delay and reverb were already in from B1); the patch controller is `KobaltPatchController` (Kalliope's `PatchController` is a global class too: the rack links both); 16 factory sounds (Sub Bass, Glide Lead, PWM Lead, Squelch Bass, Pluck, LFO Gate, Noise Hat, Zap Tom, Wobble Bass, Siren, Hollow Square, Arp Blip, Random Bleeps, Gate Organ, Brass Lead, Bend Lead); Kobalt sits between Krimson and Krunch in the rack; `kobalt_tst_kobalt` (6 tests), `kobalt_tst_patches` (4 tests). The HELP button arrives with the manual (B4) |
| B3 | **Arpeggiator and step sequencer** | Play Mode NOTE / ARP / SEQ; the arpeggiator (Up, Up & Down, Down, 1–3 octaves, Hold); 8 patterns of up to 64 steps loaded from the keys (rests, ties, legato from overlapping keys), transposed from the keys while running; `sequences.json`; `kobalt_tst_arp`, `kobalt_tst_sequencer` |
| B4 | **Manual and docs** | `docs/MANUAL.md` (HELP on the panel); README; the suite docs; `krate-render --kobalt`; copying a song's bars to Kobalt keeps one note at a time |

## Later

| Feature | Notes |
| --- | --- |
| Aftertouch | To the cutoff or vibrato, as Kalliope |
| External clock for the arpeggiator | Only if wanted |
