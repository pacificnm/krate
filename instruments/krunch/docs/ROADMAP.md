# Krunch: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. The other instruments' reference renders and the rack tests stay green throughout.

The signal chain it builds toward:

```
guitar → [pedals] → PREAMP (channel) → FX SEND → [loop pedals] → FX RETURN → POWER AMP → CABINET (IR) → out
```

| # | Milestone | Done when |
| --- | --- | --- |
| G0 ✅ | **Audio in** | The rack's audio inputs `in_1` / `in_2` (`IInstrument::inputChannels()`, `AudioBlock::inputs`); **AUDIO** in the transport bar: a capture port per input (saved, reconnected when the device appears), input meters, buffer size, the delay from input to speakers; Krunch in the rack: input 1 or 2 straight through at Level, input meter with clip light, output meter, delay; `tst_krunch`, rack input tests. Result: as planned; the buffer size is asked of the server with `jack_set_buffer_size` (PipeWire honours it while Krate runs); Krunch and Klick are left out of the Song screen's instrument choices (`InstrumentEntry::playsNotes`); **AUDIO** also picks the playback device, and lists PipeWire's per-input sources (an interface's "Input 1"), which aren't marked as hardware |
| G1 ✅ | **The amp, with channels** | Four channels, each with its own voicing (clean to high gain), gain, volume, bass / mid / treble / presence, gate; channel switching with a short crossfade; preamp, FX loop (send / return), power amp (master, sag, resonance); oversampled distortion stages; empty pedal chains in front of the amp and in the loop; tests. Result: as planned ([DSP.md](DSP.md)): voices Clean / Crunch / Heavy / Lead (1–4 stages at 4×), the interacting tone stack modelled from the circuit, a fixed speaker filter until G2; channel buttons and the loop learnable; `krunch-render` (a DI WAV or a synthetic riff through each channel; `--bench` 1–3 % per channel); `tst_krunch` (24 tests) |
| G2 ✅ | **Cabinets and the IR library** | An IR library (SQLite; files copied in, duplicates skipped by content); import files or whole folders; IRs of any rate and length, mono or stereo, resampled; partitioned convolution; two IRs blended per channel; a built-in fallback cabinet. Result: as planned ([DSP.md](DSP.md#cabinet)), in `~/.local/share/krunch/irs`; zip packs import as they are; the library keeps pack, folder and name (rather than guessing cabinet, speaker and mic from names) plus favourites; convolution with no delay and a shared input history, so switching IRs or channels is instant; IRs normalised to play equally loud; the IR picker (search, favourites, click to hear) and IMPORT on the panel; `cabinets.json` and songs' sounds keep each channel's IRs; `krunch-render --ir/--ir-b`; `tst_cabinet` (14 tests), `tst_irlibrary` (8) |
| G3 | **Panel, presets, songs** | Learned knobs that follow the playing channel; Program Change for channels; presets (the whole amp, IRs included); IR library tidying (remove, rename, tags); the amp's whole state saved with songs in the Song library |
| G4 | **Tuning** | An industrial rhythm preset and others tuned by ear; a tuner |
| G5 | **Song-driven channels** | Channel changes marked on a song's bars, played with the song |
| G6 | **Guitar from MIDI** | A plucked-string voice that plays a song's guitar track into the amp, as a guide to play along with (and to mute when you play) |

## Later

| Feature | Notes |
| --- | --- |
| Pedals in front of the amp | ✅ The pedalboard (add, remove, drag to reorder, footswitches learnable, saved with songs); Compressor, Green Drive, Round Fuzz, 5- and 10-band EQs, Phaser, Flanger, Tremolo ([PEDALS.md](PEDALS.md)). Next: boost, gate, wah |
| Pedals in the loop | ✅ Each pedal FRONT or LOOP; Chorus, Delay and Reverb ([PEDALS.md](PEDALS.md)). Next: a stereo loop (stereo chorus, ping-pong delay, wide reverb), tempo-synced delay |
| Reordering pedals | Drag to reorder each chain |
| Signature pedals | ✅ Prism (the early-70s spacey lead and clean rig in one pedal), with Vibe and Drum Echo as pedals of their own; Madman (the early-80s neoclassical metal rig, for the clean channel); Pyramid (the mid-80s British twin-guitar metal rig) ([PEDALS.md](PEDALS.md#signature-pedals)). More to come, one per distinct sound |
| Footswitches | Channels and pedals from a MIDI foot controller |
