# Krimson: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. k303's, K909's and Kalliope's reference renders and the rack tests stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| C0 ✅ | **Skeleton** | `instruments/krimson/` (`krimson_dsp`, `krimson_ui` QML module `Krimson`), `apps/krimson/`; the slot parameter table (≤ 64) with formats; the engine holding four slots, the table showing the edited one; registry entry; red panel frame with sections and labels; `tst_krimson`. Result: 59 parameters (56 per slot, Volume / Tempo / Octave for the instrument); the engine keeps four slot stores, the table reads and writes the edited one and reports everything changed on a switch so the panel follows; learned CCs edit the edited slot; the `Krimson` singleton has `editedSlot`; slot buttons A-D on the frame already work |
| C1 ✅ | **One voice** | Kalliope's oscillator, envelope, LFO and effects moved to `shared/dsp/` (Kalliope byte-identical); OSC 1/2 with all waves, sync, FM, ring mod; multi-mode filter with drive; filter, amp and mod envelopes; LFO 1 and 2; `krimson-render` sounds; `tst_voice`. Result: oscillator, envelope and LFO in `shared/dsp/` (the oscillator as `dsp/BlepOscillator.h`, so it never shadows k303's `dsp/Oscillator.h`; the LFO gained triangle and soft random); `MultiFilter`; one voice playing the edited slot (last-note priority), pitch bend, wheel vibrato; `krimson-render` (sync lead, FM bell, ring clang, 12 dB acid bass, driven saw lead, notch pad; `--bench` 0.71 % per voice); `krimson_tst_voice` (15 tests) |
| C2 ✅ | **Polyphony** | 16 voices, Poly / Mono / Legato / Unison, glide, velocity, bend, wheel, sustain; `--bench` within budget. Result: as planned, plus all-notes-off; voices render in stereo (unison spread now, slot pan in C3) with a 2 ms fade when stolen; `--bench` 9.9 % for 16 heavy voices; `krimson_tst_voices` (15 tests) |
| C3 | **Slots** | Four slots with channels, layering (lit slots), split, level / pan / on; voices shared across slots; `slots.json`; `tst_slots` |
| C4 | **Panel** | Shared `RingKnob` and stepping selector buttons; every control placed and wired to the edited slot; slot buttons; wheels; keyboard; screenshot matches UI.md |
| C5 | **Morph** | Morph values per parameter and slot; wheel or velocity as source; MORPH editing on the panel with the range shown on the rings; `tst_morph` |
| C6 | **Effects** | Chorus / Ensemble, delay, reverb per slot; `tst_fx` |
| C7 | **Programs and performances** | 64 programs (factory banks 1–2), 8 performances; STORE, PERF; Program Change per slot channel; files; `tst_programs` |
| C8 | **Arpeggiator and the rack** | Per-slot arpeggiator on the transport; Krimson in the rack with channels 3–6 (the registry and the rack learn "extra channels" per device); learn; `krate-render --krimson` |
| C9 | **Sound and polish** | Factory programs tuned by ear toward the target character (sync and FM leads, ring bells, 12 dB basses, unison leads, morph sweeps); accessibility; docs final |

## Later

| Feature | Notes |
| --- | --- |
| More oscillator shapes | Wave tables and shape control, as on the newer model |
| More effects | Phaser, flanger, comb, ring, drive as an effect slot |
| Mutate / randomize | A sound variation button |
| Per-slot outputs | `outputChannels()` > 2 in the rack |
