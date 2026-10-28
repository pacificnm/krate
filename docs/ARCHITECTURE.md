# Architecture

k303 is two halves joined by lock-free primitives: a Qt GUI that never touches audio, and a realtime JACK callback that never touches Qt. The DSP core in between knows nothing about either.

## Modules

| Module | Kind | Depends on | Responsibility |
| --- | --- | --- | --- |
| `k303_dsp` | static lib, C++20 | std only | Oscillator, diode ladder, envelopes, voice, sequencer, parameter store, SPSC queue. Pure, allocation-free in `process()` |
| `k303_jack` | static lib | `k303_dsp`, libjack | JACK client: ports, process callback, MIDI event dispatch, transport |
| `k303_app` | part of executable | Qt 6 Core/Gui/Qml/Quick, `k303_jack` | QML-facing controllers, persistence, MIDI Learn UI state |
| `k303` | executable | all above | `main.cpp`, QML module `K303` |
| `k303-render` | executable | `k303_dsp` | Offline: render a pattern to WAV, no JACK |
| `k303_tests` | Qt Test | `k303_dsp`, `k303_app` | Unit tests |

```
         GUI thread (Qt event loop)                     JACK realtime thread
 ┌───────────────────────────────────────┐      ┌─────────────────────────────────┐
 │ QML panel                             │      │ process(nframes)                │
 │   │ bindings                          │      │   1. drain GUI→audio queue      │
 │ SynthController  PatternModel         │      │   2. read params (atomics)      │
 │ MidiLearnController  Persistence      │      │   3. for each MIDI event/step   │
 │   │ write              ▲ poll 60 Hz   │      │      boundary: split block,     │
 │   ▼                    │              │      │      apply event, render slice  │
 │ ParamStore (atomics) ──┼──────────────┼─────►│   4. publish playhead, CC hits  │
 │ CcMap (atomics)  ──────┼──────────────┼─────►│   5. write out_L / out_R        │
 │ SPSC GuiToAudio ───────┼──────────────┼─────►│                                 │
 │ SPSC AudioToGui ◄──────┴──────────────┼──────│◄── midi_in port (LX49)          │
 └───────────────────────────────────────┘      └─────────────────────────────────┘
```

## Threads

| Thread | Owner | May do | Must not do |
| --- | --- | --- | --- |
| GUI | Qt | Anything Qt; file I/O; allocate | Touch DSP objects directly |
| JACK process | libjack | Read/write atomics, push/pop SPSC queues, run DSP | Allocate, lock a mutex, log, call Qt, do I/O, throw |
| JACK notification | libjack | Set atomic flags (xrun count, sample-rate change, shutdown) | Anything slow |

The realtime rules are enforced by review and by a debug-build check: `RtGuard` sets a thread-local flag in `process()`, and a debug-only `operator new` override asserts if it is called while the flag is set.

Denormals: `process()` enables flush-to-zero and denormals-are-zero (`_MM_SET_FLUSH_ZERO_MODE`, `_MM_SET_DENORMALS_ZERO_MODE`) at the top of every callback.

## Cross-thread communication

Every channel is single-producer / single-consumer and fixed size. Nothing allocates after startup.

| Channel | Direction | Mechanism | Contents |
| --- | --- | --- | --- |
| Parameters | GUI → audio, audio → GUI | `ParamStore`: `std::array<std::atomic<float>, ParamCount>`, normalized 0–1 | Knob values. GUI writes on drag; audio writes when a learned CC arrives. Audio smooths values per sample (see [DSP.md](DSP.md)) |
| Param dirty bits | audio → GUI | `std::atomic<uint64_t>` bitmask | Which params the audio thread changed (from MIDI), so the GUI updates those knobs only |
| CC map | GUI → audio | `std::array<std::atomic<int16_t>, 128>`: CC number → ParamId, −1 = unmapped | MIDI Learn bindings |
| Learn arm | GUI → audio | `std::atomic<int16_t> learnTarget` (−1 = off) | Param waiting for a CC |
| Commands | GUI → audio | `SpscQueue<Command, 256>` | Step edits, pattern switch, transport play/stop, tempo, whole-pattern load |
| Events | audio → GUI | `SpscQueue<Event, 256>` | `CcLearned{cc, param}`, `NoteOn{note}` (for step record), `Xrun` |
| Playhead | audio → GUI | `std::atomic<int8_t> currentStep` (−1 = stopped) | LED playhead |

The GUI polls atomics and drains the events queue from one `QTimer` at 60 Hz. UI timers are fine for *display*; they never drive timing.

Ownership of pattern data: the audio thread owns the playing copy. `PatternModel` on the GUI side keeps a mirror that is the source of truth for editing and saving; every edit updates the mirror and pushes a `Command`. On pattern switch or load, the GUI sends the whole pattern (16 steps fit in one command payload).

## Block processing

`process(nframes)` renders in slices. A slice ends at the next of: a MIDI event's frame offset, a sequencer step/gate boundary, or the end of the block. Events take effect at their exact sample. This is what makes both the sequencer and MIDI input sample-accurate.

## C++ ↔ QML surface

Registered in the `K303` QML module with `QML_ELEMENT` / `QML_SINGLETON`:

| Type | Exposes |
| --- | --- |
| `Synth` (singleton) | One `Q_PROPERTY(qreal …)` per parameter (normalized), `playing`, `tempo`, `currentStep`, `xrunCount`, `audioConnected` |
| `Pattern` (singleton) | `QAbstractListModel` of 16 steps (roles: `note`, `octave`, `accent`, `slide`, `gate`), `length`, `index`, `selectedStep`; invokables to edit |
| `MidiLearn` (singleton) | `active`, `armedParam`, `mappingFor(paramId)`, `clear(paramId)`, `clearAll()` |

QML holds no audio state and runs no timing logic.

Singleton instances: `main()` creates each controller before the QML engine, and the class's static `create(QQmlEngine*, QJSEngine*)` hands QML that instance. The controller must **not** be default-constructible (no `parent = nullptr` default): Qt 6.8 prefers a default constructor over `create()` and would silently build a second instance, which for `Synth` means a second JACK client.

## Startup and shutdown

1. Load `midimap.json` and `patterns.json` (missing or invalid → defaults, warning logged).
2. Create JACK client `k303`, register `out_L`, `out_R`, `midi_in`, set callbacks, activate.
3. Auto-connect outputs to the first two physical playback ports (under PipeWire these are the default sink's, which it lists first); connect `midi_in` to the first capture port whose name matches the configured regex (default `Impact LX49`).
4. If JACK is unavailable, the UI still starts with an "audio offline" indicator and retries every 2 s.
5. On quit: stop transport, deactivate and close the client, save patterns and mappings.

Sample-rate change: the notification callback sets a flag; the next `process()` calls `Voice::setSampleRate()` and recomputes samples-per-step. Coefficient tables are sized at startup for up to 192 kHz × 4 oversampling, so this does not allocate.
