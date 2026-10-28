# Build

Target: Debian 13 (trixie) / KDE Plasma on PipeWire, Qt 6.8, CMake ≥ 3.24, GCC 14 or Clang, C++20.

## Dependencies

Already on the dev machine: Qt 6.8.2 (`qt6-declarative-dev`, QtQuick Controls and Shapes QML modules), CMake 3.31, g++ 14, PipeWire, `libjack-jackd2-0` (runtime only).

Install the rest:

```sh
sudo apt install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-shapes \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript qml6-module-qtquick-templates \
  libjack-jackd2-dev pipewire-jack \
  alsa-utils qpwgraph
```

| Package | Why |
| --- | --- |
| `libjack-jackd2-dev` | JACK headers; we build against the JACK API |
| `pipewire-jack` | PipeWire's libjack replacement, so the JACK client runs on PipeWire |
| `alsa-utils` | `aseqdump` / `aconnect` for checking the LX49 |
| `qpwgraph` | Graph view to see and patch k303's ports |

## Repo layout

```
CMakeLists.txt
src/
  core/                # krate_core: shared Krate pieces; no Qt, no JACK, no instrument
    Instrument.h         IInstrument, AudioBlock, MidiEvent, TransportState
    ParamDescriptor.h    parameter descriptors, snapToDetent
    StandaloneTransport.h
    StepClock.{h,cpp}    step timing and order from beat position
    Midi.{h,cpp}         decode(), MidiLearnMap
    MasterStage.h        host soft clip + master meter
    SpscQueue.h, Denormals.h
  host/                # krate_host: JACK, no Qt, no instrument
    JackHost.{h,cpp}     standalone host: ports, MIDI in, transport, master stage
  dsp/                 # k303_dsp: k303's realtime core; no Qt, no JACK
    Params.{h,cpp}       parameter table, units, display text
    ParamStore.h
    Oscillator, DiodeLadder, HalfbandDecimator, Voice
    Sequencer.{h,cpp}    k303 step rules on krate::StepClock
    Engine.{h,cpp}       implements krate::IInstrument
  app/                 # Qt side
    SynthController.{h,cpp}     # QML singleton "Synth": params, standalone transport, JackHost
    PatternModel.{h,cpp}        # QML singleton "Pattern"
    MidiLearnController.{h,cpp} # QML singleton "MidiLearn" (instrument-neutral)
    Persistence.{h,cpp}         # patterns.json, midimap.json, panel.json
qml/
  Main.qml Panel.qml Knob.qml StepRow.qml PianoKeys.qml PanelButton.qml Led.qml
  LcdField.qml WaveSwitch.qml PanelLabel.qml LearnOverlay.qml Theme.qml
tools/
  render_main.cpp      # k303-render
tests/
  tst_params tst_dsp tst_sequencer tst_queue tst_pattern tst_midi, TestHost.h
docs/
```

R6 (docs/SHARED_FOUNDATIONS.MD) moves `core/` and `host/` to `shared/` and k303 to `instruments/k303/` + `apps/k303/`.

## CMake outline

```cmake
add_library(krate_core STATIC src/core/...)          # PUBLIC include dir: src
add_library(krate_host STATIC src/host/JackHost.cpp)
target_link_libraries(krate_host PUBLIC krate_core PkgConfig::JACK)

add_library(k303_dsp STATIC src/dsp/...)
target_link_libraries(k303_dsp PUBLIC krate_core)

qt_add_executable(k303 src/main.cpp)
qt_add_qml_module(k303 URI K303 VERSION 1.0 NO_IMPORT_SCAN
  SOURCES src/app/...  QML_FILES qml/...)
target_link_libraries(k303 PRIVATE k303_dsp krate_host Qt6::Quick Qt6::QuickControls2)

add_executable(k303-render tools/render_main.cpp)   # links k303_dsp only
# tests: one executable + add_test per tests/tst_*.cpp
```

Warnings: `-Wall -Wextra -Wpedantic -Werror` on `krate_core`, `krate_host`, `k303_dsp` and `k303-render`. Debug builds add `-fsanitize=address,undefined` (option `K303_SANITIZE`, default ON for Debug).

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
pw-jack ./build/k303
```

`pw-jack` makes the binary load PipeWire's libjack. To make that the system default instead, the `pipewire-jack` package ships an ld.so config snippet (find it with `dpkg -L pipewire-jack | grep ld.so.conf`); copying it to `/etc/ld.so.conf.d/` and running `sudo ldconfig` removes the need for `pw-jack`.

Latency: choose the buffer size with `PIPEWIRE_LATENCY=64/48000 pw-jack ./build/k303`. Target 64–128 frames at 48 kHz without xruns.

## Test

```sh
ctest --test-dir build --output-on-failure
./build/k303-render --bars 8 --out /tmp/k303.wav               # listen to the DSP without JACK
./build/k303-render --resonance 0.9 --envmod 0.8 --accent 1 --out /tmp/hot.wav   # any param key 0..1, --tempo BPM, --square
./build/k303-render --bench                                    # CPU cost per second of audio
cmake --build build --target k303_qmllint                     # lints against the generated module (knows Theme is a singleton)
```

Screenshot without a display: `QT_QPA_PLATFORM=offscreen K303_SCREENSHOT=/tmp/k303.png ./build/k303` renders the window, saves it and quits (after `K303_SCREENSHOT_DELAY_MS`, default 500).

Test MIDI without hardware: point `inputPortMatch` at `Midi Through` and send with `aseqsend -p 14:0 "B0 15 7F"` (CC 21 = 127). Use `XDG_CONFIG_HOME` / `XDG_DATA_HOME` to keep test runs away from your real config and patterns.

Without Ninja installed, drop `-G Ninja`; CMake's default Makefile generator works the same.

| Test | Checks |
| --- | --- |
| `tst_sequencer` | Step start positions exact over 100 000 steps at fractional samples-per-step; slide/tie/rest/accent rules table from SEQUENCER.md; tempo change mid-step; wrap-around |
| `tst_dsp` | No NaN/Inf over long renders at max resonance, all cutoffs, 44.1/48/96 kHz; output peak bounds; oscillator alias level below a threshold at high pitch |
| `tst_queue` | SPSC queue order, full/wrap, two-thread stress |
| `tst_pattern` | Pattern model edits reach the engine; pattern switch, length, clear; JSON round-trip; unknown keys ignored; bad files fall back to defaults; newer `version` not overwritten; debounced and on-quit saves; audition |
| `tst_midi` | Mapped CCs move params (any channel); learn binds the first CC and applies it; rebinding moves a CC; switch threshold at 64; keys, legato, velocity-0 note off, keys ignored while running, sample-accurate event timing; mapping file round-trip and bad entries; MidiLearn controller end to end |
| RT allocation guard | Debug build: running `Engine::process` under `RtGuard` with a full pattern and MIDI input triggers no allocation |
