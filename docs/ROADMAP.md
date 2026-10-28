# Roadmap

Each milestone ends with something you can run and check. Do them in order; each builds on the last.

| # | Milestone | Done when |
| --- | --- | --- |
| M0 | **Skeleton** | `cmake --build` succeeds with zero warnings; `k303` opens a window showing the empty cream panel scaled with correct aspect at any window size; `ctest` runs (one placeholder test) |
| M1 | **JACK audio out** | `pw-jack k303` shows a `k303` client with `out_L`/`out_R` in qpwgraph, auto-connected; a 220 Hz test saw plays; 10 minutes at 64 frames / 48 kHz with no xruns; quitting cleanly removes the client; UI shows "audio offline" when JACK is unavailable and recovers when it returns |
| M2 | **Params end to end** | `Knob.qml` complete; all 9 knobs/switches on the panel; moving a knob changes `ParamStore` and the test tone's level/pitch without zipper noise |
| M3 | **Voice DSP** | PolyBLEP osc, 4x-oversampled diode ladder, MEG/VEG, accent sweep, slide; `k303-render` writes a WAV of a hard-coded riff that sounds like classic acid bass; `tst_dsp` passes; `--bench` under 5% of a core |
| M4 | **Sequencer** | Done: sequencer core with M3; playhead LED flashes from `Synth.currentStep`; beat LED; Space = run/stop; lock-free command queue (`tst_queue`, clean under ThreadSanitizer). Open: 133 BPM pattern stays in time with a click track for 10 minutes (listening check) |
| M5 | **Pattern editing and storage** | Select step, set note/octave/accent/slide/gate from the panel and keyboard shortcuts; edits audible on the next pass while playing; 8 patterns, length 1–16; `patterns.json` persists across restarts |
| M6 | **MIDI in and Learn** | LX49 auto-connects; keys play the voice when stopped (legato slides between held keys); learn mode binds knobs; bindings survive restart; `tst_ccmap` and `tst_persistence` pass |
| M7 | **v1 polish** | Sound tuned by ear against reference recordings (update starting values in DSP.md); qmllint clean; accessibility names set; README quick start verified on a clean machine |

## Later (post v1)

| Feature | Notes |
| --- | --- |
| External MIDI clock | 24 PPQN, start/stop/continue; `ClockSource` seam from M4 |
| Step record | Uses the `NoteOn` event already pushed to the GUI in M6 |
| Live transpose | From the keyboard while the pattern plays |
| Square-wave shaper | Saw → transistor-style shaper, closer to the hardware square |
| Soft takeover, MIDI channel filter | MIDI Learn refinements |
| LV2 plugin | Wraps `k303_dsp` (no Qt/JACK in it by design); LV2 UI can reuse the QML panel |
| Pattern chaining / song mode | Not in the brief; only if wanted |
