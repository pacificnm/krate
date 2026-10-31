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
| `qpwgraph` | Graph view to see and patch Korrode's ports |

## Repo layout

Korrode lives inside the Krate monorepo (../../../docs/SHARED_FOUNDATIONS.MD#shared-code-layout). Build from the repo root.

```
CMakeLists.txt                 # Krate project: adds shared/, instruments/korrode/, apps/korrode/
shared/                        # instrument-neutral
  core/      krate_core        IInstrument, AudioBlock, TransportState, ParamDescriptor,
                               Transport, StepClock, Midi (decode, MidiLearnMap),
                               MasterStage, SpscQueue, Denormals            -- no Qt, no JACK
  host/      krate_host        JackHost                                     -- JACK, no Qt
  app/       krate_app         JsonFile, MidiMapFile, PanelFile, ScreenshotHook   -- Qt
             krate_qml         MidiLearnController (QML singleton MidiLearn)
  qml/       krate_qml         QML module "Krate": Knob, PanelButton, Led, LcdField,
                               PanelLabel, LearnOverlay, Theme
  tests/     tst_queue, TestHost.h
instruments/korrode/
  dsp/       korrode_dsp          Params, ParamStore, Oscillator, DiodeLadder, HalfbandDecimator,
                               Voice, Sequencer, Engine (implements krate::IInstrument)
  app/       korrode_ui           SynthController ("Synth"), PatternModel ("Pattern"), PatternFile
  qml/       korrode_ui           QML module "Korrode": Main, Panel, StepRow, PianoKeys, WaveSwitch
  tools/     korrode-render
  tests/     tst_params, tst_dsp, tst_sequencer, tst_pattern, tst_midi
  docs/      this folder
apps/korrode/   main.cpp          the korrode binary: links korrode_ui + krate_qml (static QML plugins)
docs/        SHARED_FOUNDATIONS.MD (suite level)
```

## CMake outline

Each folder has its own `CMakeLists.txt`. Libraries: `krate_core`, `krate_host`, `krate_app`, `krate_qml` (module `Krate`), `korrode_dsp`, `korrode_ui` (module `Korrode`). Both QML modules are static libraries; `apps/korrode` links their plugins and imports them with `Q_IMPORT_QML_PLUGIN`. All binaries land in `build/bin/`; all QML modules in `build/qml/`.

Warnings: `-Wall -Wextra -Wpedantic`, plus `-Werror` on the non-Qt libraries and `korrode-render`. Debug builds add `-fsanitize=address,undefined` (option `KRATE_SANITIZE`, default ON for Debug).

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
pw-jack ./build/bin/korrode
```

`pw-jack` makes the binary load PipeWire's libjack. To make that the system default instead, the `pipewire-jack` package ships an ld.so config snippet (find it with `dpkg -L pipewire-jack | grep ld.so.conf`); copying it to `/etc/ld.so.conf.d/` and running `sudo ldconfig` removes the need for `pw-jack`.

Latency: choose the buffer size with `PIPEWIRE_LATENCY=64/48000 pw-jack ./build/bin/korrode`. Target 64–128 frames at 48 kHz without xruns.

## Test

```sh
ctest --test-dir build --output-on-failure
./build/bin/korrode-render --bars 8 --out /tmp/korrode.wav               # listen to the DSP without JACK
./build/bin/korrode-render --resonance 0.9 --envmod 0.8 --accent 1 --out /tmp/hot.wav   # any param key 0..1, --tempo BPM, --square
./build/bin/korrode-render --bench                                    # CPU cost per second of audio
cmake --build build --target all_qmllint                      # lints both QML modules (Krate and Korrode)
```

Screenshot without a display: `QT_QPA_PLATFORM=offscreen KRATE_SCREENSHOT=/tmp/korrode.png ./build/bin/korrode` renders the window, saves it and quits (after `KRATE_SCREENSHOT_DELAY_MS`, default 500).

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
