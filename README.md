# Krate

A suite of software instruments for Linux, and (coming) a rack that plays them together from one clock, mixes them and routes one MIDI controller across them. Qt 6 / QML hardware-style panels, C++ realtime engines, JACK/PipeWire audio and MIDI.

| | Status |
| --- | --- |
| [**k303**](instruments/k303/README.md): acid bass synth with a 16-step sequencer, diode-ladder filter, MIDI Learn | Playable |
| [**Klick**](instruments/klick/README.md): metronome, clicks on the shared transport | Works in the rack core and `krate-render` |
| **K909**: drum machine | Planned |
| **Kalliope**: polysynth | Planned |
| **Krate rack**: hosts every instrument; transport bar, mixer, per-channel MIDI routing | Playable: `krate` app (k303 + Klick), rack-wide MIDI Learn, sessions in `~/.config/krate/rack.json` |

Design: [docs/SHARED_FOUNDATIONS.MD](docs/SHARED_FOUNDATIONS.MD) (shared interfaces, transport, MIDI, the rack, repo layout).

## Layout

```
shared/            instrument-neutral code: core (no Qt), JACK host, Qt helpers, QML controls
instruments/<name> one folder per instrument: DSP, controllers, panel, tools, tests, docs
apps/<name>        thin executables
docs/              suite-level docs
```

## Build

Debian 13 / KDE Plasma on PipeWire; Qt 6.8, CMake ≥ 3.24, C++20.

```sh
sudo apt install build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-shapes \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript qml6-module-qtquick-templates \
  libjack-jackd2-dev pipewire-jack
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build
pw-jack ./build/bin/k303                          # k303 on its own
pw-jack ./build/bin/krate                         # the rack: k303 + Klick
./build/bin/krate-render --bars 8 --out rack.wav  # the rack, rendered offline
```

More (dependencies, testing, MIDI without hardware): [instruments/k303/docs/BUILD.md](instruments/k303/docs/BUILD.md).

## License

Copyright (C) 2026 Jaimie Garner

Krate is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

It links dynamically against Qt 6 (LGPLv3) and libjack / PipeWire's JACK library (LGPL-2.1 / MIT), all of which are compatible with GPL-3.0-or-later.
