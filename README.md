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

## License

Copyright (C) 2026 Jaimie Garner

k303 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

It links dynamically against Qt 6 (LGPLv3) and libjack / PipeWire's JACK library (LGPL-2.1 / MIT), all of which are compatible with GPL-3.0-or-later.
