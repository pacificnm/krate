# Roadmap

Each milestone ends with something you can run and check. Do them in order; each builds on the last.

| # | Milestone | Done when |
| --- | --- | --- |
| M0 | **Skeleton** | `cmake --build` succeeds with zero warnings; `k303` opens a window showing the empty cream panel scaled with correct aspect at any window size; `ctest` runs (one placeholder test) |
| M1 | **JACK audio out** | `pw-jack k303` shows a `k303` client with `out_L`/`out_R` in qpwgraph, auto-connected; a 220 Hz test saw plays; 10 minutes at 64 frames / 48 kHz with no xruns; quitting cleanly removes the client; UI shows "audio offline" when JACK is unavailable and recovers when it returns |
| M2 | **Params end to end** | `Knob.qml` complete; all 9 knobs/switches on the panel; moving a knob changes `ParamStore` and the test tone's level/pitch without zipper noise |
| M3 | **Voice DSP** | PolyBLEP osc, 4x-oversampled diode ladder, MEG/VEG, accent sweep, slide; `k303-render` writes a WAV of a hard-coded riff that sounds like classic acid bass; `tst_dsp` passes; `--bench` under 5% of a core |
| M4 | **Sequencer** | Done: sequencer core with M3; playhead LED flashes from `Synth.currentStep`; beat LED; Space = run/stop; lock-free command queue (`tst_queue`, clean under ThreadSanitizer). Open: 133 BPM pattern stays in time with a click track for 10 minutes (listening check) |
| M5 | **Pattern editing and storage** | Done: select step, set note/octave/accent/slide/gate from the panel and keyboard shortcuts; edits sent live to the audio thread; 8 patterns, length 1–16; Clear; Keyboard mode auditions notes; `patterns.json` saved 2 s after edits and on quit (`tst_pattern`) |
| M6 | **MIDI in and Learn** | Done: `midi_in` auto-connects by name, also on hot-plug; keys play the voice when stopped (legato, velocity ≥ 100 = accent); learn mode binds knobs and the waveform switch; bindings saved to `midimap.json`; `tst_midi` passes; verified end to end via Midi Through. Open: confirm on the LX49 hardware |
| M7 | **v1 polish** | Sound tuned by ear against reference recordings (update starting values in DSP.md); qmllint clean; accessibility names set; README quick start verified on a clean machine |

| M8 ✅ | **A fuller sound** | More of the bass kept under resonance (makeup (1 + k)^0.8); saturation where the ladder's feedback meets the signal; a bent saw and a square shaped from it; SWEEP (accent sweep depth) and accents driving the filter harder; DRIVE and TONE; DELAY, REVERB, WIDTH (the dry bass stays centred). Result: at the default resonance a held low note's fundamental is 1.9 dB under no resonance (was 8.3); output trimmed −4 dB so the demo is 1.8 dB louder than before; bench 1.4 % (1.7 % with drive and both effects); `tst_character`; [MANUAL.md](MANUAL.md) and HELP |

## Later (post v1)

| Feature | Notes |
| --- | --- |
| External MIDI clock | 24 PPQN, start/stop/continue; `ClockSource` seam from M4 |
| Step record | Uses the `NoteOn` event already pushed to the GUI in M6 |
| Live transpose | From the keyboard while the pattern plays |
| Soft takeover, MIDI channel filter | MIDI Learn refinements |
| LV2 plugin | Wraps `k303_dsp` (no Qt/JACK in it by design); LV2 UI can reuse the QML panel |
| Pattern chaining / song mode | Not in the brief; only if wanted |
