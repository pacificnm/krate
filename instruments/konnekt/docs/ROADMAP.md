# Konnekt: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done.

| # | Milestone | Done when |
| --- | --- | --- |
| N1 ✅ | **The engine** | `konnekt_dsp`: modules described by tables (jacks, knobs, help text); the patch graph (modules, cables, signal order, one block of delay round a loop); voices and the MIDI In module; Oscillator, Envelope, LFO, VCA, Mixer, Attenuverter, Output; patch changes handed to the audio thread without locks; live taps on every output; `konnekt-render` plays a patch to WAV; `konnekt_tst_engine` (pitch 1 per octave, voices, a per-voice cable into a one-voice input mixes, loops delay one block, unplugged inputs read their knobs). Result: as planned; `konnekt-render` plays a built-in patch (patch files arrive with N4); patches are built from plain data (`PatchSpec`, no Qt) for the renderer, the tests and the app; a stolen voice's gate closes for one block so envelopes start again; 8 voices of two oscillators, an LFO, a mixer, an envelope and a VCA: 3.4 % of a core; `konnekt_tst_engine` (16 tests, including every module's help text being there and every module fed by everything at once staying finite) |
| N2 | **The other modules** | Noise, the three filters, Sample & hold, Slew, Drive, Crusher, Ring mod, Clock, Sequencer, Chorus, Delay, Reverb, Scope; each tested |
| N3 | **The app** | The `konnekt` app on JACK: the rack of modules drawn from their tables, add / move / delete, click-to-wire cables, live traces on the jacks, a scope on a cable, help on hover, VOICES, MIDI Learn for knobs |
| N4 | **Patches and lessons** | Save / load by name; lesson patches with their explanations; the manual (HELP); README and the suite docs |
