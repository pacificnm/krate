# Krate: Shared Foundations for Instruments, Rack, MIDI and Audio

**Krate** is the suite: a set of standalone instruments (three to start, more later) and the **Krate rack**, an app that hosts them all, plays them from one clock, mixes them and routes one controller to them. Each instrument ships as its own app first. This doc sets the shared structure every instrument follows from its first line of code, so that building the rack means wrapping instruments, not rewriting them.

| Phase | Deliverable |
| --- | --- |
| 1 | **k303**: acid bass synth with step sequencer (exists; refactor to this doc, see [Refactor plan](#refactor-plan-for-k303)) |
| 2 | **K909**: multi-voice drum synth with a track × step sequencer |
| 3 | **Kalliope**: polyphonic analog-style synth, keyboard-played, optional arpeggiator |
| 4 | **Krate rack**: one app hosting every instrument: transport bar, mixer, per-channel MIDI routing, one session file |
| 5+ | More instruments, each added as a library the rack picks up (see [Adding instruments](#adding-instruments)) |

Names: the suite and rack are **Krate**; the instruments are **k303**, **K909** and **Kalliope**. Docs, code and commits never name the original hardware that inspired an instrument, or its maker.

## Principles

1. **The realtime instrument is pure.** Each instrument's DSP core implements `IInstrument`: a `process()` that reads only its inputs (audio buffers, MIDI events, transport state, its own parameters and command queue) and writes only its outputs. No Qt, no JACK, no globals, no allocation, locks or I/O. The same object runs under a standalone host, a rack, an offline renderer or (later) a plugin wrapper.
2. **Saved state lives outside the realtime path.** Each instrument has a non-realtime controller layer (Qt) that owns the editable state (patterns, MIDI map, knob positions) and does all serialization. It sends changes to the core as commands. The core never touches files. *(Changed from an earlier draft, which put `serialize()` on the engine: file and JSON work cannot run on the audio thread.)*
3. **Time comes from a transport, not from each instrument.** Tempo, play/stop and song position are supplied to `process()` every block. A standalone app creates its own transport; the rack supplies one shared transport to all devices. Sequencers derive their steps from it.
4. **MIDI is delivered with the audio block, timestamped.** Hosts read MIDI in the audio callback and pass each block's events, with frame offsets, to `process()`. Events apply at their exact sample. No MIDI thread, no queue between MIDI and audio. *(Changed from an earlier draft, which had a MIDI thread feeding a lock-free queue: that adds latency and loses sample accuracy; see k303 DECISIONS D1.)*
5. **One audio client per process.** Standalone, each instrument app owns its JACK/PipeWire client. In the rack, the rack owns the single client and calls each device's `process()` with its own output buffers, and exposes a stereo output pair per device plus the master pair. Because devices never talk to JACK themselves, this is a host swap, not a device change.
6. **Shared code is shared, not copied.** Everything instrument-neutral (interfaces, queues, MIDI, learn, parameter system, host, UI components) lives in one shared library that all apps link.

## Core interfaces (shared, no Qt)

### Parameters

Every instrument describes its parameters in a table. Hosts, MIDI Learn, generic panels and session files work from the table without knowing the instrument.

```cpp
struct ParamDescriptor {
    std::string_view key;     // stable id: saved in session and MIDI-map files, never renamed
    std::string_view label;   // panel label
    float defaultValue;       // normalized 0..1
    int detents;              // 0 = continuous; N = selector or switch with N positions
    std::string (*format)(float normalized);  // "1.25 kHz", "Fwd+Rev": GUI thread only
};

class ParamStore;  // lock-free normalized values + "changed by audio" bits (as in k303 today)
```

Values crossing any boundary are normalized 0..1. Conversion to engine units stays inside each instrument.

### Transport

```cpp
struct TransportState {
    bool playing;
    bool restarted;          // play pressed this block: sequencers reset to their first step
    double bpm;              // constant for the block
    double beatAtBlockStart; // quarter notes since play was pressed
};
```

Within a block, beat position advances by `bpm / 60 / sampleRate` per sample. A sequencer's step index is `floor(beat × stepsPerBeat)`; it fires a step when that index changes. Tempo changes stay seamless, all devices stay locked, and an external MIDI clock later is just another transport source.

### Audio block and instrument

```cpp
struct MidiEvent { std::uint32_t frame; std::uint8_t size; std::uint8_t data[3]; };

struct AudioBlock {
    float *const *outputs;           // outputChannels() buffers, each `frames` long; instrument overwrites them
    std::uint32_t frames;
    const MidiEvent *midi;           // sorted by frame
    std::uint32_t midiCount;
    const TransportState *transport;
};

class IInstrument {
public:
    virtual ~IInstrument() = default;

    // Non-realtime (before processing, or while stopped).
    virtual void prepare(double sampleRate, std::uint32_t maxFrames) = 0;
    virtual std::span<const ParamDescriptor> parameters() const = 0;
    virtual int outputChannels() const { return 2; }  // stereo; a drum machine may expose more

    // Any thread.
    virtual ParamStore &params() = 0;

    // Realtime.
    virtual void process(const AudioBlock &block) noexcept = 0;
};
```

Instrument-specific data (k303 patterns, K909 drum tracks, Kalliope programs) does not go through this interface. Each instrument has its own command queue (GUI → audio) and event queue (audio → GUI) between its controller and its core, as k303 already does with `Engine::post()` and `popEvent()`.

Output level: each instrument keeps its output bounded (no hard overs) but does not heavy-limit. The rack's mixer owns the master limiter. In standalone mode, the host adds the same master limiter.

### MIDI and MIDI Learn

| Piece | Shared? | Notes |
| --- | --- | --- |
| `MidiEvent`, status decoding (note on/off, CC, velocity-0 as note off, channel) | Shared | Plain functions; no queue |
| `MidiLearnMap`: CC → parameter table, learn target, "CC learned" event | Shared, realtime-safe | Extracted from k303's `Engine::handleMidi`; each instrument owns one instance |
| What notes do (mono with legato, drum triggers, poly voice allocation) | Per instrument | Inside each `process()` |
| `MidiLearnController` (QML singleton), `LearnOverlay` (QML) | Shared, generic over `parameters()` | Today k303-specific only through `ParamId`; generalize to descriptor keys |
| Mapping file | Per instrument | `~/.config/<instrument>/midimap.json`; in the rack, stored per device in the session |

## Threads: standalone and rack

```
 Standalone (one instrument)                    Rack (all three)

 GUI thread                                     GUI thread
   Instrument controller(s) + QML panel           Rack controller + device controllers + panels
   StandaloneTransport (tempo, run)               Transport (tempo, run, position)
          │ commands / atomics                           │ commands / atomics
          ▼                                              ▼
 JACK thread                                    JACK thread
   JackHost::callback                             RackHost::callback
     read MIDI                                      read MIDI, route by channel / focus
     advance transport                              advance transport (one clock)
     instrument.process(block)                      for each device: device.process(its block)
     master limiter → out_L/out_R                   per-device out_L/out_R (each device's own pair)
                                                    mixer: gain/pan/mute/solo → master limiter → master out
```

The only difference between the two columns is the host. Instrument cores and controllers are identical.

## Shared code layout

A monorepo, so the shared library and all instruments change together and build as one CMake project:

```
shared/
  core/      IInstrument, AudioBlock, TransportState, ParamDescriptor, ParamStore,
             SpscQueue, MidiEvent + decoding, MidiLearnMap, Denormals, HalfbandDecimator,
             offline render harness (WAV writer, bench)              -- no Qt
  host/      JackHost (standalone), later RackHost                   -- JACK, no Qt
  app/       Persistence helpers (atomic JSON save, versioned load), MidiLearnController,
             generic panel/param controller, screenshot hook          -- Qt
  qml/       Knob, PanelButton, Led, LcdField, PanelLabel, LearnOverlay, Theme,
             scale-to-window Main pattern                            -- QML module
instruments/
  k303/      core/ (Voice, DiodeLadder, Sequencer, Engine : IInstrument), app/, qml/, tests/
  k909/      ...
  kalliope/  ...
apps/
  k303/      main.cpp: JackHost + k303 module
  k909/, kalliope/
  krate/     main.cpp: RackHost + all instrument modules + rack UI (binary: krate)
docs/        suite-level docs (this file); each instrument keeps its own docs/
```

Each instrument builds as **libraries plus a QML module** (static), and each app is a thin `main.cpp`. The rack links all three instruments' libraries and shows their panels inside its own window.

QML singletons: k303 uses singletons (`Synth`, `Pattern`, `MidiLearn`). That works in the rack because each instrument has its own QML module, giving one instance per instrument type. **Decided: the rack holds at most one of each instrument**, so singletons stay. (Allowing two copies of one instrument later would mean replacing the singletons with per-panel controller objects.)

## The rack layer

| Component | Responsibility |
| --- | --- |
| `RackHost` | Owns the one JACK client; per block: read MIDI, advance the transport, route MIDI, call each device, write each device's stereo pair, mix, limit, write the master pair |
| `Transport` | Shared tempo, play/stop, beat position; later MIDI clock in/out and swing. Shown as the **transport bar** |
| `Mixer` | Per device: level, pan, mute, solo; master level and limiter |
| MIDI routing | Per-channel routing plus focus, below |
| Session file | `rack.json`: transport, mixer, routing, rack-level MIDI map, focus, and each device's saved state produced by that device's controller (its patterns and knob positions) |
| Rack UI | Transport bar on top; device panels in a stack or tabs with a focus indicator; mixer strip |

### Audio outputs

JACK ports in the rack: `<device>:out_L` / `<device>:out_R` for every device (pre-mixer, post-device), plus `master_L` / `master_R` (post-mixer, post-limiter). Auto-connect sends only the master pair to the speakers; the per-device pairs are there for recording or mixing in a DAW.

### MIDI routing

Decided: **per-channel routing plus a focus**, so one controller can play one instrument while its knobs are spread across several.

| Message | Goes to |
| --- | --- |
| Notes on a channel assigned to a device | That device (for example ch 1 → k303, ch 10 → K909, ch 2 → Kalliope) |
| Notes on an unassigned channel | The focused device |
| CC with a **fixed** mapping | The mapped device and parameter, whatever has focus |
| CC with a **follow-focus** mapping | That parameter key on whichever device has focus (when it has one with that key) |
| Focus-switch messages | Change the focused device (below) |

Rack-level MIDI map (in `rack.json`): entries keyed by **(channel, CC)** → (device, parameter key) for fixed mappings, or → parameter key for follow-focus mappings; channel can be "any". Learning in the rack: arm a control on any device panel, move a hardware knob; the rack records the channel too, so the same CC number on different channels can drive different instruments.

Focus switching from the controller: a rack action that is MIDI-learnable like a parameter. Learn "focus k303", "focus K909", "focus next" etc. to a button's CC or note, or use Program Change n → device n. The Nektar's buttons and its preset/program features can send these; the exact messages are checked on the hardware when the rack is built.

Standalone apps keep their own `midimap.json` as today (one device, so no channel or focus logic).

Standalone apps keep working unchanged. The rack is one more app over the same libraries.

### Tempo

Decided: in the rack, tempo and play/stop live on the **transport bar**; instrument panels hide their own Tempo and RUN/STOP controls there. Standalone apps keep the Tempo knob and RUN/STOP, which drive their standalone transport.

## Adding instruments

The suite will grow beyond three, so the rack must not hard-code its devices.

- Each instrument library exports one registration entry: id (`k303`), display name, a factory for its core and controller, its QML module and panel component, its default MIDI channel, and the size of its panel.
- `apps/krate` lists the instruments it links in one place (a compile-time registry). Adding an instrument = adding its library and one registry line; no rack code changes.
- Everything the rack does (routing, mixer strips, learn, session save, outputs) works from the registry and each device's `parameters()`, never from instrument-specific types.

## Instrument notes

| | k303 | K909 | Kalliope |
| --- | --- | --- | --- |
| Voices | 1, mono, legato/slide | One per drum sound, fixed | N, voice allocation and stealing |
| Sequencer | Step: note/octave/accent/slide/gate | Tracks × steps: trigger/accent (flam, per-step params later) | None; optional arpeggiator driven by the transport |
| Shared timing | `StepClock`: steps from transport beat position, scale (steps per beat), play order (Forward/Reverse/Fwd+Rev/Random) | Same `StepClock` | Arpeggiator uses `StepClock` |
| Outputs | Stereo | Stereo; per-voice outputs optional later | Stereo |
| Keys | Play the voice (stopped) | Trigger sounds (mapped notes) | Play polyphonically |

`StepClock` is the timing and step-order part of k303's `Sequencer`, extracted and shared; each instrument keeps its own step data and step rules.

## Refactor plan for k303

Each step keeps every test green and k303 behaving the same; commit after each.

| # | Step | Done when |
| --- | --- | --- |
| R1 | **Parameter descriptors.** Replace the `ParamId`-only table with `ParamDescriptor` (add detents and `format`); move display text from `SynthController` into the core | `Synth.displayText` reads `format`; tests cover every parameter's text |
| R2 | **`IInstrument` and `AudioBlock`.** `Engine` implements it; `k303-render` and tests drive it through the interface | Renderer output identical before and after (sample compare) |
| R3 | **Transport.** Add `TransportState` and a `StandaloneTransport`; Tempo and run/stop move out of the instrument into the transport; extract `StepClock` from `Sequencer` and drive it from beat position | Existing timing tests pass against the transport; the Tempo knob still works (it drives the standalone transport) |
| R4 | **Generic MIDI.** Extract `MidiEvent` decoding and `MidiLearnMap` into shared core; make `MidiLearnController` and `LearnOverlay` work from descriptor keys | `tst_midi` passes unchanged plus tests for the shared pieces |
| R5 | **Host decoupling.** `JackHost` takes an `IInstrument&` and a transport and owns the master limiter; no k303 types in the host | Host compiles without any k303 header |
| R6 | **Repo layout.** Move to `shared/`, `instruments/k303/`, `apps/k303/`; shared QML components become their own module | Clean build from scratch; the k303 app and all tests run as before |

After R6, starting K909 means adding `instruments/k909/` and `apps/k909/` and nothing in `shared/` should need to change except genuinely new shared pieces.

## Decisions and open questions

| # | Question | Decision |
| --- | --- | --- |
| 1 | Suite and repo name | **Krate** (suite and rack app). Rename the GitHub repo `k303` → `krate` before R6 |
| 2 | Instrument names | k303, K909, Kalliope |
| 3 | More than one copy of an instrument in the rack | No: one of each; QML singletons stay |
| 4 | Rack MIDI routing | Per-channel routing plus focus; fixed and follow-focus CC mappings; focus switching learnable from the controller |
| 5 | Rack audio outputs | Stereo pair per device plus master |
| 6 | Tempo in the rack | Transport bar; panels hide their own Tempo and RUN/STOP in the rack |
| 7 | More instruments later | Yes: compile-time instrument registry; nothing in the rack is instrument-specific |
