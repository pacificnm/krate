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
    const float *const *inputs;      // inputChannels() buffers (audio in: a guitar), or nullptr = silence
};

class IInstrument {
public:
    virtual ~IInstrument() = default;

    // From the processing thread, between blocks, never concurrently with process();
    // must not allocate (size buffers for maxFrames in the constructor).
    virtual void prepare(double sampleRate, std::uint32_t maxFrames) noexcept = 0;
    virtual std::span<const ParamDescriptor> parameters() const noexcept = 0;
    virtual int outputChannels() const noexcept { return 2; }  // a drum machine may expose more
    virtual int inputChannels() const noexcept { return 0; }   // audio inputs it plays (Krunch, the guitar amp)

    // Any thread: parameters by index into parameters(), normalized 0..1.
    virtual float param(int index) const noexcept = 0;
    virtual void setParam(int index, float normalized) noexcept = 0;
    virtual void setParamFromHost(int index, float normalized) noexcept = 0;  // audio thread; flags the change (K1)
    virtual std::uint64_t takeParamsChangedByAudio() noexcept = 0;  // bit = index

    // The instrument's CC map and MIDI Learn state (added in R4); nullptr if none.
    virtual MidiLearnMap *midiLearnMap() noexcept { return nullptr; }

    // Realtime.
    virtual void process(const AudioBlock &block) noexcept = 0;
};
```

As built in R2 (`shared/core/Instrument.h`): parameters are reached by index rather than through a shared `ParamStore` type, so the interface does not depend on any instrument's parameter enum; `ParamStore` stays an implementation detail. A `processStereo()` helper wraps two buffers into an `AudioBlock` for stereo hosts and tests.

Instrument-specific data (k303 patterns, K909 drum tracks, Kalliope programs) does not go through this interface. Each instrument has its own command queue (GUI → audio) and event queue (audio → GUI) between its controller and its core, as k303 already does with `Engine::post()` and `popEvent()`.

Output level: each instrument keeps its output bounded (no hard overs) but does not heavy-limit. The rack's mixer owns the master limiter. In standalone mode, the host adds the same master limiter.

### MIDI and MIDI Learn

| Piece | Shared? | Notes |
| --- | --- | --- |
| `MidiEvent`, status decoding (note on/off, CC, velocity-0 as note off, channel) | Shared | `krate::decode()` in `shared/core/Midi.h`; no queue |
| `MidiLearnMap`: CC → target table (a parameter, or an app-defined **action** such as "select pattern 1" that fires on a press), learn target, detent snapping, "CC learned" and "action pressed" queues to the GUI | Shared, realtime-safe | `krate::MidiLearnMap` in `shared/core/Midi.h`; each instrument owns one and returns it from `IInstrument::midiLearnMap()` |
| What notes do (mono with legato, drum triggers, poly voice allocation) | Per instrument | Inside each `process()` |
| `MidiLearnController` (QML singleton), `LearnOverlay` (QML) | Shared, generic over `parameters()` | Constructed from any `IInstrument&`; works by parameter key; mapping file resolved through the descriptor table (R4) |
| Mapping file | Per instrument | `~/.config/<instrument>/midimap.json`; in the rack, stored per device in the session |

## Threads: standalone and rack

```
 Standalone (one instrument)                    Rack (all three)

 GUI thread                                     GUI thread
   Instrument controller(s) + QML panel           Rack controller + device controllers + panels
   Transport (tempo, run)               Transport (tempo, run, position)
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

A monorepo, so the shared library and all instruments change together and build as one CMake project. As built in R6:

```
CMakeLists.txt         Krate project; one add_subdirectory per folder below
shared/
  core/      krate_core   IInstrument, AudioBlock, TransportState, ParamDescriptor, Transport,
                          StepClock, Midi (decode, MidiLearnMap), MasterStage, SpscQueue, Denormals  -- no Qt
  dsp/       krate_dsp    HalfbandDecimator, polyBlep, Oscillator (BlepOscillator.h), Envelope, Lfo,
                          Chorus and Delay (Effects.h), Reverb                                    -- no Qt
  host/      krate_host   JackHost (standalone); later RackHost                                   -- JACK, no Qt
  app/       krate_app    JsonFile, MidiMapFile, PanelFile, ScreenshotHook                        -- Qt
             krate_qml    MidiLearnController (QML singleton MidiLearn)
  qml/       krate_qml    QML module "Krate": Knob, RingKnob, PanelSlider, PanelSwitch, PanelButton (tint),
                          PianoKeyboard, Led, LcdField, PanelLabel, LearnOverlay, Theme
  tests/     tst_queue; TestHost.h (drives any IInstrument with a Transport)
instruments/
  k303/      dsp/ (k303_dsp: Engine : IInstrument), app/ + qml/ (k303_ui: QML module "K303"),
             tools/ (k303-render), tests/, docs/, README.md
  klick/     dsp/ (klick_dsp: metronome, IInstrument), tests/, README.md
  k909/      dsp/ (k909_dsp), app/ + qml/ (k909_ui: QML module "K909"), tools/ (k909-render), tests/, docs/
  kalliope/  dsp/ (kalliope_dsp), app/ + qml/ (kalliope_ui: QML module "Kalliope"), tools/ (kalliope-render), tests/, docs/
  krimson/   dsp/ (krimson_dsp), app/ + qml/ (krimson_ui: QML module "Krimson"), tools/ (krimson-render), tests/, docs/
             (K909, Kalliope and Krimson are included as "k909/...", "kalliope/...", "krimson/...": instruments share file names)
  kalliope/  ...
apps/
  k303/      main.cpp: JackHost + the K303 and Krate modules
  k909/, kalliope/
  krate/     main.cpp: RackHost + all instrument modules + rack UI (binary: krate)
  krate-render/  main.cpp: the rack (k303 + Klick, K909 with --k909, Kalliope with --kalliope) rendered offline to WAV
docs/        suite-level docs (this file); each instrument keeps its own docs/
```

Each instrument builds as **libraries plus a static QML module**, and each app is a thin `main.cpp` that links the modules' plugins (`Q_IMPORT_QML_PLUGIN`). The rack will link every instrument's libraries and show their panels inside its own window. Binaries land in `build/bin/`, QML modules in `build/qml/`.

QML singletons: k303 uses singletons (`Synth`, `Pattern`, `MidiLearn`). That works in the rack because each instrument has its own QML module, giving one instance per instrument type. **Decided: the rack holds at most one of each instrument**, so singletons stay. (Allowing two copies of one instrument later would mean replacing the singletons with per-panel controller objects.)

## The rack layer

| Component | Responsibility |
| --- | --- |
| `RackHost` | Owns the one JACK client; per block: read MIDI, advance the transport, route MIDI, call each device, write each device's stereo pair, mix, limit, write the master pair |
| `Transport` | Shared tempo, play/stop, beat position; later MIDI clock in/out and swing. Shown as the **transport bar** |
| `Mixer` | Per device: level, pan, mute, solo; master level and limiter |
| MIDI routing | Per-channel routing plus focus, below |
| Session file | `rack.json`: transport, mixer, routing, rack-level MIDI map, focus, and each device's saved state produced by that device's controller (its patterns and knob positions) |
| Rack UI | Transport bar on top; instrument tabs with a focus indicator, then **Mixer**, **Song** and **Seq** tabs (the Song screen: [SONG_PLAYER.md](SONG_PLAYER.md); **S** or the learnable `rack:song`; the rack sequencer: [SEQUENCER.md](SEQUENCER.md); **Q** or the learnable `rack:seq`). The tabs fit the window: the screens' tabs keep their width, the instruments' share the rest (150 px at most). Below them, one screen at a time: the focused instrument's panel, scaled to the whole window, or the mixer screen (tall strips: pan, mute, solo, fader, meter, MIDI channel; master). Picking an instrument (tab, Tab key, a controller's focus button) shows its panel; the Mixer tab, **M** or the learnable `rack:mixer` toggle the mixer. `rack.json` remembers which screen was showing (`"view"`). Keys: Space play/stop, Tab next instrument, M mixer, S song, Q sequencer, L learn, Esc leaves learn. **Mixer MIDI Learn**: with LEARN on, every strip's fader, pan, M and S and the master fader can be learned from a controller (faders and knobs from CCs; M and S from CCs or notes, a press toggles). Keys `rack:mix:<n>:fader|pan|mute|solo`, `rack:mix:master:fader`; the rack core passes them to the GUI's mixer as `Rack::MixerMove`s (`Mapping::Kind::Mixer`), so the fader on screen, the session and the sound stay in step. Saved in `rack.json` as `"kind": "mixer"` |

### Audio outputs

JACK ports in the rack: `<device>:out_L` / `<device>:out_R` for every device (pre-mixer, post-device), plus `master_L` / `master_R` (post-mixer, post-limiter). Auto-connect sends only the master pair to the speakers (the device chosen under **AUDIO**, or the first hardware outputs); the per-device pairs are there for recording or mixing in a DAW.

### Mixer EQ

Every instrument's strip has an EQ above its pan knob (`krate::ChannelEq`, `shared/rack/ChannelEq.h`), applied after the instrument and before the fader (the device's own JACK output stays before it):

| Band | Knobs | Range |
| --- | --- | --- |
| **HI** (high shelf) | gain, HI FREQ | ±15 dB; 1.5–16 kHz (default 8 kHz) |
| **MID** (peak) | gain, MID FREQ, MID Q | ±15 dB; 100 Hz–10 kHz (default 1 kHz); Q 0.3–8 (default 0.7: wide; higher is narrower) |
| **LO** (low shelf) | gain, LO FREQ | ±15 dB; 30–600 Hz (default 100 Hz) |

- Gains at 0 dB (the knob's middle, which snaps there) make the strip **flat**: the EQ is skipped entirely, bit for bit. The EQ box is outlined in blue and says "on" while it does something; double-click **EQ** to make it flat again; double-click a knob for its default.
- The filters are the shared `krate::Biquad` (`shared/dsp/Biquad.h`, cookbook shelves and peak; Krunch's tone stacks use it too). Retuned between 64-frame chunks with their state kept, so turning a knob doesn't click.
- Saved with the mixer: in `rack.json` (each device's `"eq"`: the 7 knob positions), and in a song's or a saved sequence's mixer. Older files without it load flat.
- MIDI Learn: every EQ knob, keys `rack:mix:<n>:lowgain|lowfreq|midgain|midfreq|midq|highgain|highfreq`.
- Measured (`tst_rack`): each band on a sine within 0.2 dB of the filter's response.

### Master stage

`krate::MasterStage` (`shared/core/MasterStage.h`) is the last stage before the outputs, in the rack and in every standalone host and render tool: a soft clip plus the master meter. The instruments don't limit; this stage makes sure nothing ever leaves as a hard over. The curve is linear up to the knee at 0.5 (−6 dBFS), untouched, then `0.5 + 0.5·tanh((|x| − 0.5) / 0.5)` above it: the same slope where they meet (no corner), approaching 1.0.

It was a plain `tanh` until it was measured on a live take: `tanh` bends everything, and a song whose mix peaked at 0.85 (a sustained low bass line with drums) picked up about 6 % distortion (23 dB below the music) through whole sections, heard as clipping on the bass. With the knee, the same take measures 46 dB or more below the music; mixes that stay under −6 dBFS pass bit-exact.

### Audio inputs

The rack has two audio inputs, `in_1` and `in_2`: an audio interface's two inputs, for a guitar. Each is fed by one capture port chosen under **AUDIO** in the transport bar (saved in `rack.json` as `"audioInputs"`, reconnected whenever that device appears). Every device with `inputChannels() > 0` gets both, re-based per chunk like its MIDI; inputs it asks for beyond the rack's are silent, and hosts without inputs (`krate-render`, tests) give it none (`inputs == nullptr`). The same popup picks the playback device the master goes to (`"outputDevice"`; by default the first hardware outputs), sets the buffer size asked of the server (`"bufferSize"` in `rack.json`; `jack_set_buffer_size`, which PipeWire honours while Krate runs) and shows the delay from input to speakers, the capture plus playback latency the server reports.

### MIDI routing

Decided: **per-channel routing plus a focus**, so one controller can play one instrument while its knobs are spread across several.

| Message | Goes to |
| --- | --- |
| Notes on a channel assigned to a device | That device (defaults: ch 10 → K909, ch 2 → Kalliope, ch 3–6 → Krimson's slots A–D; k303 and Klick have none) |
| Notes on an unassigned channel | The focused device |
| CC with a **fixed** mapping | The mapped device and parameter, whatever has focus |
| CC with a **follow-focus** mapping | That parameter key on whichever device has focus (when it has one with that key) |
| Focus-switch messages | Change the focused device (below) |

Rack-level MIDI map (in `rack.json`): entries keyed by **(channel, CC)** → (device, parameter key) for fixed mappings, or → parameter key for follow-focus mappings; channel can be "any". Learning in the rack: arm a control on any device panel, move a hardware knob; the rack records the channel too, so the same CC number on different channels can drive different instruments.

Focus switching from the controller: a rack action that is MIDI-learnable like a parameter. Learn "focus k303", "focus K909", "focus next" etc. to a button's CC or note, or use Program Change n → device n. The Nektar's buttons and its preset/program features can send these; the exact messages are checked on the hardware when the rack is built.

Standalone apps keep their own `midimap.json` as today (one device, so no channel or focus logic).

#### One controller

Decided: the target setup is **one Nektar Impact LX49 mk3** driving every instrument.

- A button (or several) on the LX49 switches focus: "focus k303", "focus K909", "focus Kalliope", "focus next". Whichever message the button sends (CC, note or Program Change) works, because focus actions are learned like parameters. The panel shows which instrument has focus.
- Keys and follow-focus knobs drive the focused instrument; fixed knobs (for example K909's level, or tempo) ignore focus.
- If the LX49's pads can send on their own channel, per-channel routing lets pads play K909 (e.g. channel 10) while the keys play the focused instrument. To be checked on the hardware.
- Each mixer strip has a **MIDI channel selector** (FOCUS or CH 1–16; click for the next, right-click or wheel for the previous), saved in `rack.json` as a list per device. A device can hold several channels: Krimson's registry entry adds `extraMidiChannels` (4–6) to its default 3, and its strip reads **CH 3+3**; picking a channel leaves it just that one, double-click restores the defaults. A channel belongs to one device: giving it to another takes it. With one keyboard, leave the instruments the keys should play on FOCUS: an instrument holding the keyboard's channel gets the keys whatever has focus. (Found on the hardware: k303 used to default to channel 1, the LX49's, so the keys never reached Kalliope; k303 now defaults to FOCUS.)
- Hardware notes (2026-10-28): the LX49 mk3 is class-compliant on Linux (USB ID `2467:201a`) and has **two MIDI ports**: `MIDI1` (keys, knobs, faders, pads) and `MIDI2` (control and transport buttons). Hosts connect every port matching the device name, so both arrive. The control buttons send CCs on channel 16 via MIDI2 (one sends CC 47, value 127 on press and 0 on release); everything the user wanted (focus switching, pattern buttons, play/stop, knobs) maps through rack MIDI Learn with no code changes (K6).

#### Who resolves CCs

Decided: **in the rack, Krate resolves mapped CCs itself** and sets the target instrument's parameter directly. Instruments receive notes and any CCs the rack has not mapped. Each instrument's own `MidiLearnMap` is used standalone only. This is what lets one knob drive K909 while another drives k303, whatever has focus.

#### Rack additions to the shared interfaces

Both are added when the rack is built (K1 below); standalone apps are unaffected.

1. **`IInstrument::setParamFromHost(int index, float normalized)`.** Sets a parameter on behalf of the host *and* raises its "changed by audio" bit, so the instrument's panel moves the knob. Called from the audio thread by the rack when a mapped CC arrives. (`setParam()` stays the GUI's call and raises no bit.) If sample-accurate parameter changes are ever needed, this becomes parameter events carried in `AudioBlock`.
2. **The MIDI device choice belongs to the host.** In the rack, which controller to connect (`inputPortMatch`, default "Impact LX49") is stored in `rack.json`. Standalone apps keep it in their own `midimap.json` as today.

Standalone apps keep working unchanged. The rack is one more app over the same libraries.

### Tempo

Decided: in the rack, tempo and play/stop live on the **transport bar**; instrument panels hide their own Tempo and RUN/STOP controls there. Standalone apps keep the Tempo knob and RUN/STOP, which drive their standalone transport.

## Manuals

An instrument's manual is a Markdown file in its QML module (`RESOURCES docs/MANUAL.md` in `qt_add_qml_module`), opened on its panel with the shared `ManualView` (contents on the left from its `##` headings, the section on the right, a search; sized to the window). `ManualReader` (QML `Manual`) splits and searches it. `tst_manual` checks that every `label:` on Krimson's panel appears in its manual, so a new control can't go undocumented.

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
| R1 ✅ | **Parameter descriptors.** Replace the `ParamId`-only table with `ParamDescriptor` (add detents and `format`); move display text from `SynthController` into the core | `Synth.displayText` reads `format`; tests cover every parameter's text |
| R2 ✅ | **`IInstrument` and `AudioBlock`.** `Engine` implements it; `k303-render` and tests drive it through the interface | Renderer output identical before and after (sample compare) |
| R3 ✅ | **Transport.** Add `TransportState` and a `Transport`; Tempo and run/stop move out of the instrument into the transport; extract `StepClock` from `Sequencer` and drive it from beat position | Existing timing tests pass against the transport; the Tempo knob still works (it drives the standalone transport). Result: renders byte-identical; Tempo stays in k303's parameter table so the knob, `panel.json` and MIDI Learn keep working, but the engine ignores it and the standalone app forwards it to its transport |
| R4 ✅ | **Generic MIDI.** Extract `MidiEvent` decoding and `MidiLearnMap` into shared core; make `MidiLearnController` and `LearnOverlay` work from descriptor keys | `tst_midi` passes unchanged plus tests for the shared pieces |
| R5 ✅ | **Host decoupling.** `JackHost` takes an `IInstrument&` and a transport, and owns the master stage; no k303 types in the host | Host compiles without any k303 header. Result: `krate::JackHost` + `krate::MasterStage` (soft clip + meter, [Master stage](#master-stage); the engine no longer clips); `krate_core` and `krate_host` libraries; verified by compiling `core/` + `host/` alone; renders byte-identical |
| R6 ✅ | **Repo layout.** Move to `shared/`, `instruments/k303/`, `apps/k303/`; shared QML components become their own module | Clean build from scratch; the k303 app and all tests run as before |

**The refactor is complete.** Starting K909 means adding `instruments/k909/` and `apps/k909/` and nothing in `shared/` should need to change except genuinely new shared pieces.

## Rack build plan

Each step ends with something runnable and tested; commit after each. Instruments keep working standalone throughout.

| # | Step | Done when |
| --- | --- | --- |
| K1 ✅ | **Rack core** (`shared/rack`, no Qt, no JACK): `krate::Rack` owns the device list, per-device buffers, the shared transport, MIDI routing (channel map + focus), the rack CC map (fixed and follow-focus, resolved via `setParamFromHost`), focus actions, and the mixer (level, pan, mute, solo) into the master stage. Adds `setParamFromHost` to `IInstrument` | Offline tests with fake instruments: routing by channel and focus, focus switching from a learned message, fixed vs follow-focus CCs, mixer math, two devices locked to one transport. Result: `shared/rack` (`krate::Rack`, library `krate_rack`), `tst_rack` (19 tests); a learned button doesn't fire on the press that teaches it; Program Change *n* focuses device *n* (can be turned off); blocks longer than the rack's buffers are split with MIDI and beat re-based |
| K2 ✅ | **Klick**: a tiny metronome instrument in `instruments/klick` (beat clicks, accented bar, level) | Second real device in the rack; locks to the transport; also serves as the click track for k303's timing check. Result: `instruments/klick` (`tst_klick`), shared `krate::ParamValues<N>` and `tools/WavFile.h`, and `apps/krate-render` (k303 + Klick through a real `krate::Rack` to WAV). Verified: with Klick muted the rack render is byte-identical to standalone k303; clicks land on k303's beats to the sample. The mixer now starts at its fader settings instead of ramping from unity |
| K3 ✅ | **Controller split and registry.** Instrument controllers stop owning a host or transport: k303's `Synth` gets its transport and audio status from whoever hosts it. Each instrument library exports a registry entry (id, name, factory, panel, default MIDI channel) | Standalone k303 unchanged: renders byte-identical, app behaves the same. Result: `krate::Transport` (renamed from `StandaloneTransport`; the rack shares one), `krate::AudioHost` interface (JackHost implements it), QML singleton **`Host`** (`krate::HostController`: audio status, reconnect, MIDI auto-connect, master meter), `krate::InstrumentEntry` / `InstrumentInstance` / `InstrumentContext` (`shared/app/InstrumentRegistry.h`, with `triggerAction` for learnable buttons); k303's `Synth` keeps only k303 concerns and is given a transport (none in the rack: it hides Tempo and RUN/STOP); the standalone app builds k303 through `k303::registryEntry()`; `tst_instance` |
| K4 ✅ | **RackHost and the `krate` app.** One JACK client: a stereo pair per device plus master, MIDI in. Window: transport bar, device panels (tabs or stack, focus indicator), mixer strip. Panels hide their own Tempo and RUN/STOP | `./build/bin/krate` plays k303 (and Klick) in time from the transport bar; mixer and per-device ports work. Result: `JackHost` now runs a `JackProcessor` (`InstrumentProcessor` for standalone apps, `RackProcessor` for the rack; standalone renders unchanged); ports `krate:master_L/R` (to the speakers), `krate:<id>_L/R` per device, `krate:midi_in`; shared `krate::ParamController` (QML base for instrument knobs; Klick's panel uses it); Klick's QML module and registry entry; `apps/krate` (`Rack` singleton: devices, focus, transport, mixer, meters; window: transport bar, tabs = focus, panel scaled to fit, mixer strips). Interim until K5: instruments use their standalone files; MIDI Learn on k303's own map. Verified in the app: Program Change 1 focuses Klick; a channel-1 note plays k303 regardless of focus |
| K5 ✅ | **Rack MIDI learn and session.** Learn fixed and follow-focus knobs and focus actions; `rack.json` saves transport, mixer, routing, MIDI map, focus, device choice and each device's state | A restart restores everything. Result: rack mappings can also be **actions** (rack play/stop, any instrument's buttons) from CCs or notes; `MidiLearn` is now a base with `InstrumentMidiLearn` (standalone, `midimap.json`) and `RackMidiLearn` (rack: panel keys mean the focused instrument; fixed or follow-focus via FOLLOW; rack keys `rack:playstop`, `rack:focus:<n>`, `rack:next`, `rack:previous` (later `rack:mixer`); badges like "CC 47 ch16"); `rack.json` (`~/.config/krate/`) saves tempo, focus, MIDI device, mixer, channel routing and mappings by instrument id and key. Instrument knobs and patterns stay in each instrument's own folder (shared with its standalone app). `tst_racklearn`; verified in the app with a saved session driving focus and pattern buttons |
| K6 ✅ | **Hardware check with the LX49**: focus buttons, pads channel, knobs | Notes recorded here; any message types the LX49 sends that we don't handle are fixed. Result: on real hardware, every LX49 control the user wanted mapped (focus switching, k303 pattern buttons, play/stop, knobs) learned and worked through rack MIDI Learn; no unhandled message types turned up, so no code changes were needed |

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
| 8 | Controllers | One LX49; focus switched from a button on it (any learnable CC, note or Program Change); pads on their own channel if the hardware allows |
| 9 | Who maps CCs in the rack | Krate resolves mapped CCs and calls `setParamFromHost`; instruments get notes and unmapped CCs; instrument learn maps are standalone-only |
| 10 | MIDI device setting | Host-owned: `rack.json` in the rack, `midimap.json` standalone |
| 11 | Next build | The Krate rack (K1–K6), before K909 |
