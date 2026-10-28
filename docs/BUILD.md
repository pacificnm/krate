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
  main.cpp
  dsp/                 # k303_dsp: no Qt, no JACK
    Params.h           # ParamId enum, ranges, string ids, normalized→engine maps
    ParamStore.h
    SpscQueue.h
    Oscillator.{h,cpp}
    DiodeLadder.{h,cpp}
    Oversampler.{h,cpp}
    Envelope.{h,cpp}
    Voice.{h,cpp}
    Sequencer.{h,cpp}
    Engine.{h,cpp}     # owns Voice + Sequencer, process(in events, out buffer, n)
  jack/                # k303_jack
    JackClient.{h,cpp}
  app/                 # Qt side
    SynthController.{h,cpp}     # QML singleton "Synth"
    PatternModel.{h,cpp}        # QML singleton "Pattern"
    MidiLearnController.{h,cpp} # QML singleton "MidiLearn"
    Persistence.{h,cpp}         # midimap.json, patterns.json
qml/
  Main.qml Panel.qml Knob.qml StepLed.qml PanelSwitch.qml NoteKey.qml Theme.qml
tools/
  render_main.cpp      # k303-render
tests/
  tst_sequencer.cpp tst_dsp.cpp tst_persistence.cpp tst_ccmap.cpp
docs/
```

## CMake outline

```cmake
cmake_minimum_required(VERSION 3.24)
project(k303 VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
qt_standard_project_setup(REQUIRES 6.8)

add_library(k303_dsp STATIC src/dsp/...)
target_include_directories(k303_dsp PUBLIC src)

add_library(k303_jack STATIC src/jack/JackClient.cpp)
target_link_libraries(k303_jack PUBLIC k303_dsp PkgConfig::JACK)

qt_add_executable(k303 src/main.cpp src/app/...)
qt_add_qml_module(k303 URI K303 VERSION 1.0
  QML_FILES qml/Main.qml qml/Panel.qml qml/Knob.qml ...)
target_link_libraries(k303 PRIVATE k303_jack Qt6::Quick Qt6::QuickControls2)

add_executable(k303-render tools/render_main.cpp)
target_link_libraries(k303-render PRIVATE k303_dsp)

enable_testing()
# one qt_add_executable + add_test per tests/tst_*.cpp, linking Qt6::Test
```

Warnings: `-Wall -Wextra -Wpedantic -Werror` on `k303_dsp` and `k303_jack`. Debug builds add `-fsanitize=address,undefined` (option `K303_SANITIZE`, default ON for Debug).

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
./build/k303-render --pattern 0 --bars 8 --out /tmp/k303.wav   # listen to the DSP without JACK
./build/k303-render --bench                                    # CPU cost per second of audio
cmake --build build --target k303_qmllint                     # lints against the generated module (knows Theme is a singleton)
```

Screenshot without a display: `QT_QPA_PLATFORM=offscreen K303_SCREENSHOT=/tmp/k303.png ./build/k303` renders the window, saves it and quits.

Without Ninja installed, drop `-G Ninja`; CMake's default Makefile generator works the same.

| Test | Checks |
| --- | --- |
| `tst_sequencer` | Step start positions exact over 100 000 steps at fractional samples-per-step; slide/tie/rest/accent rules table from SEQUENCER.md; tempo change mid-step; wrap-around |
| `tst_dsp` | No NaN/Inf over long renders at max resonance, all cutoffs, 44.1/48/96 kHz; output peak bounds; oscillator alias level below a threshold at high pitch |
| `tst_persistence` | JSON round-trip for patterns and mappings; unknown keys ignored; bad files fall back to defaults; newer `version` not overwritten |
| `tst_ccmap` | Learn binds the first CC; rebinding moves a CC; switch threshold at 64 |
| RT allocation guard | Debug build: running `Engine::process` under `RtGuard` with a full pattern and MIDI input triggers no allocation |
