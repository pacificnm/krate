# K909: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. k303's golden renders and the rack tests must stay green throughout.

| # | Milestone | Done when |
| --- | --- | --- |
| D0 ✅ | **Skeleton** | `instruments/k909/` (`k909_dsp`, `k909_ui` QML module `K909`) and `apps/k909/`; registry entry; `pw-jack ./build/bin/k909` opens the empty panel, scaled, with the header and section labels, a JACK client `k909` with `out_L/R` and `midi_in`, silent. Result: parameter table (31), silent `Engine` with MIDI Learn CCs, `Drums` controller (Tempo and START/STOP drive the standalone transport), registry entry (channel 10, actions `patternA`–`patternH`, `startstop`), `tst_k909`; verified in the app: JACK outputs and both LX49 ports connect |
| D1 | **Voices** | All 11 voices in [DSP.md](DSP.md); `k909-render` writes a WAV of every voice in turn and a demo beat; `tst_voices`: each voice is non-silent, bounded, decays to silence, deterministic, little energy above 20 kHz; `--bench` within budget |
| D2 | **Panel and notes** | Every knob on the panel and wired (`Drums` controller on `ParamController`, `panel.json`); MIDI notes play the voices with velocity and accent; right-click on a track name auditions it; screenshot matches the layout in [UI.md](UI.md) |
| D3 | **Sequencer** | Tracks × steps playback on `StepClock` with accent, Total Accent, Shuffle, hat choke, play order and scale; step LEDs; START/STOP and Space; a golden render of a fixed pattern; `tst_sequencer` (step timing to the sample, shuffle offsets, choke, pattern change at the end of the pass) |
| D4 | **Pattern editing and storage** | Track select, step toggling, A–H, length, scale, Clear; `patterns.json` with demo patterns seeded on first run (`tst_pattern`) |
| D5 | **MIDI Learn and the rack** | Learn for all knobs, A–H and `startstop`; K909 in the Krate rack on channel 10 (Tempo and START/STOP hidden there); `krate-render` includes it; rack tests extended; on the LX49, pads play K909 on channel 10 while the keys play the focused instrument |
| D6 | **Polish** | Voices tuned by ear (DSP.md updated with final values); qmllint clean; accessibility names; README section |

## Later

| Feature | Notes |
| --- | --- |
| Kits | Saved knob sets with a KIT display and WRITE, as in the reference layout |
| Flam | Per-step flam, a fourth step state |
| Parameter locks | Per-step tune/decay values |
| Per-voice outputs | `outputChannels()` > 2; the rack gives each a port pair |
| Pattern chaining / song mode | Shared with k303 if it happens |
| Step record | Play the pads while running to write steps |
