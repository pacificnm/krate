# Krunch: roadmap

Each milestone ends with something you can run and check, and is committed and pushed when done. The other instruments' reference renders and the rack tests stay green throughout.

The signal chain it builds toward:

```
guitar → [pedals] → PREAMP (channel) → FX SEND → [loop pedals] → FX RETURN → POWER AMP → CABINET (IR) → out
```

| # | Milestone | Done when |
| --- | --- | --- |
| G0 ✅ | **Audio in** | The rack's audio inputs `in_1` / `in_2` (`IInstrument::inputChannels()`, `AudioBlock::inputs`); **AUDIO IN** in the transport bar: a capture port per input (saved, reconnected when the device appears), input meters, buffer size, the delay from input to speakers; Krunch in the rack: input 1 or 2 straight through at Level, input meter with clip light, output meter, delay; `tst_krunch`, rack input tests. Result: as planned; the buffer size is asked of the server with `jack_set_buffer_size` (PipeWire honours it while Krate runs); Krunch and Klick are left out of the Song screen's instrument choices (`InstrumentEntry::playsNotes`) |
| G1 | **The amp, with channels** | Four channels, each with its own voicing (clean to high gain), gain, volume, bass / mid / treble / presence, gate; channel switching with a short crossfade; preamp, FX loop (send / return), power amp (master, sag, resonance); oversampled distortion stages; empty pedal chains in front of the amp and in the loop; tests |
| G2 | **Cabinets and the IR library** | An IR library (`~/.local/share/krate/irs`, SQLite: cabinet, speaker, mic, position, tags; files copied in, duplicates skipped by content); import files or whole folders; IRs of any rate and length, mono or stereo, resampled; partitioned convolution; two IRs blended per channel; a built-in fallback cabinet |
| G3 | **Panel, presets, songs** | The amp panel with channel buttons; MIDI Learn (channels as buttons, Program Change); presets; the IR browser (filter, search, favourites, audition); the amp's whole state saved with songs in the Song library |
| G4 | **Tuning** | An industrial rhythm preset and others tuned by ear; a tuner |
| G5 | **Song-driven channels** | Channel changes marked on a song's bars, played with the song |
| G6 | **Guitar from MIDI** | A plucked-string voice that plays a song's guitar track into the amp, as a guide to play along with (and to mute when you play) |

## Later

| Feature | Notes |
| --- | --- |
| Pedals in front of the amp | Boost, overdrive, gate, wah |
| Pedals in the loop | Delay, chorus, reverb |
| Reordering pedals | Drag to reorder each chain |
| Footswitches | Channels and pedals from a MIDI foot controller |
