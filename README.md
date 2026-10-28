# k303

A standalone acid bass synthesizer for Linux/KDE, inspired by classic 1980s analog bass synths. Qt 6 / QML hardware-style front panel, a C++ realtime DSP engine with a modeled diode-ladder filter, a built-in 16-step sequencer, and MIDI Learn for hardware controllers (developed against a Nektar Impact LX49 mk3).

Status: M0–M2 built (panel, JACK audio out, parameters end to end); M1/M2 listening checks pending. Next: M3, voice DSP. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Documentation

| Doc | What it covers |
| --- | --- |
| [PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) | Original brief (source of intent) |
| [DECISIONS.md](docs/DECISIONS.md) | Decisions made since the brief, and where they differ from it |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Modules, threads, how GUI, audio and MIDI talk to each other |
| [DSP.md](docs/DSP.md) | Oscillator, diode-ladder filter, envelopes, accent, slide, parameters |
| [SEQUENCER.md](docs/SEQUENCER.md) | Step data, sample-accurate timing, slide/tie/accent rules, pattern storage |
| [MIDI.md](docs/MIDI.md) | JACK MIDI input, MIDI Learn, mapping file, LX49 setup |
| [UI.md](docs/UI.md) | Panel layout, QML components, interaction, colors |
| [BUILD.md](docs/BUILD.md) | Dependencies, repo layout, CMake, running, testing |
| [ROADMAP.md](docs/ROADMAP.md) | Milestones with acceptance criteria |

## Quick start (once code exists)

```sh
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-shapes \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libjack-jackd2-dev pipewire-jack
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
pw-jack ./build/k303
```
