# Krate

A suite of software instruments for Linux, and a rack that plays them together from one clock, mixes them and routes one MIDI controller across them. Qt 6 / QML hardware-style panels, C++ realtime engines, JACK/PipeWire audio and MIDI.

| | Status |
| --- | --- |
| [**k303**](instruments/k303/README.md): acid bass synth with a 16-step sequencer, diode-ladder filter, MIDI Learn | Playable |
| [**Klick**](instruments/klick/README.md): metronome, clicks on the shared transport | Works in the rack core and `krate-render` |
| [**K909**](instruments/k909/README.md): drum machine, 11 synthesized voices, tracks × steps sequencer with accent and shuffle, patterns A–H | Playable, standalone and in the rack (MIDI channel 10) |
| [**Kalliope**](instruments/kalliope/README.md): 8-voice polysynth, two oscillators with sync and cross mod, 12/24 dB ladder, chorus and delay, 64 patches, arpeggiator | Playable, standalone and in the rack (MIDI channel 2) |
| [**Krimson**](instruments/krimson/README.md): 4-part multitimbral virtual-analog synth, 16 voices, sync / FM / ring oscillators, five-mode filter with drive, morph, per-slot effects and arpeggiators, 64 programs | Playable, standalone and in the rack (MIDI channels 3–6); sound tuning in progress |
| **Song player**: play along with a song: open a `.gp` tab file and/or a `.mid` file, put its tracks on Krate's instruments, leave your part out ([docs/SONG_PLAYER.md](docs/SONG_PLAYER.md)) | In progress: the Song tab plays songs through the instruments with your part's tab scrolling along; a song library (SQLite) keeps every song by artist and album with its track setup, instrument sounds and mixer; practice tools to come |
| **Krate rack**: hosts every instrument; transport bar, mixer, per-channel MIDI routing | Playable: `krate` app (k303 + K909 + Kalliope + Krimson + Klick), rack-wide MIDI Learn, sessions in `~/.config/krate/rack.json` |

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
pw-jack ./build/bin/k909                          # K909 on its own
pw-jack ./build/bin/kalliope                      # Kalliope on its own
pw-jack ./build/bin/krimson                       # Krimson on its own
pw-jack ./build/bin/krate                         # the rack: k303 + K909 + Kalliope + Krimson + Klick
./build/bin/krate-render --bars 8 --out rack.wav  # the rack, rendered offline (add --k909 0.8 --kalliope 1 --krimson 0.8 for all five)
```

More (dependencies, testing, MIDI without hardware): [instruments/k303/docs/BUILD.md](instruments/k303/docs/BUILD.md).

## License

Copyright (C) 2026 Jaimie Garner

Krate is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

It links dynamically against Qt 6 (LGPLv3) and libjack / PipeWire's JACK library (LGPL-2.1 / MIT), all of which are compatible with GPL-3.0-or-later.
