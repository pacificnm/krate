# k303

Part of **Krate**, a suite of instruments and a rack that plays them together (see [SHARED_FOUNDATIONS.MD](../../docs/SHARED_FOUNDATIONS.MD)).

A standalone acid bass synthesizer for Linux/KDE, inspired by classic 1980s analog bass synths. Qt 6 / QML hardware-style front panel, a C++ realtime DSP engine with a modeled, saturating diode-ladder filter, overdrive, delay and reverb, a built-in 16-step sequencer, and MIDI Learn for hardware controllers (developed against a Nektar Impact LX49 mk3).

Status: M0–M6 built (panel, JACK audio out, parameters, voice DSP with diode-ladder filter, sequencer with scale and play modes, pattern editing and storage, MIDI input and MIDI Learn); knob positions are saved. Next: M7, v1 polish (About box, sound tuning, accessibility, quick-start check). See [docs/ROADMAP.md](docs/ROADMAP.md).

**Manual**: [docs/MANUAL.md](docs/MANUAL.md): every control, what it does and how to use it, with sound recipes and troubleshooting. In the app, **HELP** on the panel opens it (with a search).

## Documentation

| Doc | What it covers |
| --- | --- |
| [MANUAL.md](docs/MANUAL.md) | The player's manual (also in the app: HELP) |
| [PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) | Original brief (source of intent) |
| [SHARED_FOUNDATIONS.MD](../../docs/SHARED_FOUNDATIONS.MD) | The Krate suite: shared instrument interfaces, transport, MIDI routing, the Krate rack, and the k303 refactor plan |
| [DECISIONS.md](docs/DECISIONS.md) | Decisions made since the brief, and where they differ from it |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Modules, threads, how GUI, audio and MIDI talk to each other |
| [DSP.md](docs/DSP.md) | Oscillator, diode-ladder filter, envelopes, accent, slide, the output stage (drive, delay, reverb, width), parameters |
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
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo   # from the repo root
cmake --build build
pw-jack ./build/bin/k303
```

## License

Copyright (C) 2026 Jaimie Garner

k303 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE](../../LICENSE).

It links dynamically against Qt 6 (LGPLv3) and libjack / PipeWire's JACK library (LGPL-2.1 / MIT), all of which are compatible with GPL-3.0-or-later.
