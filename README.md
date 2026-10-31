# Krate

A suite of software instruments for Linux, and a rack that plays them together from one clock, mixes them and routes one MIDI controller across them. Qt 6 / QML hardware-style panels, C++ realtime engines, JACK/PipeWire audio and MIDI.

| | Status |
| --- | --- |
| [**Korrode**](instruments/korrode/README.md): acid bass synth with a 16-step sequencer, saturating diode-ladder filter, overdrive, delay and reverb, MIDI Learn | Playable |
| [**Klick**](instruments/klick/README.md): metronome, clicks on the shared transport | Works in the rack core and `krate-render` |
| [**Kessel**](instruments/kessel/README.md): drum machine, 15 synthesized voices (four industrial) and a sample voice, per-voice mixer with drive, gated reverb, delay, compressor and crusher, 16 kits, tracks × steps sequencer with accent and shuffle, patterns A–P | Playable, standalone and in the rack (MIDI channel 10) |
| [**Kalliope**](instruments/kalliope/README.md): 16-voice polysynth, two oscillators with sync and cross mod, analog drift, a driven 12/24 dB ladder, chorus, delay and reverb, 64 patches, arpeggiator | Playable, standalone and in the rack (MIDI channel 2) |
| [**Krimson**](instruments/krimson/README.md): 4-part multitimbral virtual-analog synth, 16 voices, sync / FM / ring oscillators, five-mode filter with drive, morph, per-slot effects and arpeggiators, 64 programs | Playable, standalone and in the rack (MIDI channels 3–6); sound tuning in progress |
| [**Kobalt**](instruments/kobalt/README.md): monophonic synth after the classic one-oscillator monosynths: saw, pulse with width modulation, sub-oscillator and noise into a self-oscillating four-pole filter, one envelope with Gate + Trig / Gate / LFO triggers, portamento Off / Auto / On, bender and lever, analog drift, arpeggiator and a 64-step sequencer with legato, delay and reverb, 64 patches | Playable, standalone and in the rack (MIDI channel 7) |
| **Song player**: play along with a song: open a `.gp` tab file and/or a `.mid` file, put its tracks on Krate's instruments, leave your part out ([docs/SONG_PLAYER.md](docs/SONG_PLAYER.md)) | In progress: the Song tab plays songs through the instruments with your part's tab scrolling along; a song library (SQLite) keeps every song by artist and album with its track setup, instrument sounds and mixer; practice tools to come |
| **Sequencer**: an 8-track pattern sequencer for the whole rack: 16 patterns of up to 4 bars, each track playing one instrument, notes with length and velocity, chords ([docs/SEQUENCER.md](docs/SEQUENCER.md)) | Working: the engine, the Seq screen (piano roll, drum lanes for Kessel, velocity lane), copying bars from a song on the Song screen, pattern chains, and patterns switched live from a controller |
| [**Klang**](instruments/klang/README.md): sampler, 16 pads playing your WAV and MP3 files as one-shots, held notes or tempo-locked loops, each with filter, drive, bitcrusher, ring modulator, trance gate, choke groups, delay and reverb sends; slicing; 16 patterns of up to 64 steps with accent, rolls, reverse and pitch per step | Playable, standalone and in the rack (MIDI channel 11), with a starter kit of its own sounds |
| [**Konnekt**](instruments/konnekt/README.md): a modular synthesizer, its own app: 23 modules from Krate's synths (oscillators, three filters, envelopes, LFOs, VCAs, utilities, effects, clock, sequencer) wired with virtual cables, up to 8 voices; every jack shows its signal live, everything explains itself on hover and in full on a right-click, ten lessons build a synth up a module at a time, 17 presets to take apart | Playable (`konnekt`), from any MIDI keyboard |
| [**Krunch**](instruments/krunch/README.md): guitar amp, played from your audio interface's input | G0: the input passes straight through, with meters and the delay shown ([roadmap](instruments/krunch/docs/ROADMAP.md)) |
| **Krate rack**: hosts every instrument; transport bar, mixer, per-channel MIDI routing | Playable: `krate` app (Korrode + Kessel + Kalliope + Krimson + Kobalt + Klang + Krunch + Klick), rack-wide MIDI Learn, sessions in `~/.config/krate/rack.json` |

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
pw-jack ./build/bin/korrode                          # Korrode on its own
pw-jack ./build/bin/kessel                          # Kessel on its own
pw-jack ./build/bin/kalliope                      # Kalliope on its own
pw-jack ./build/bin/krimson                       # Krimson on its own
pw-jack ./build/bin/kobalt                        # Kobalt on its own
pw-jack ./build/bin/klang                         # Klang on its own
pw-jack ./build/bin/konnekt                       # Konnekt, the modular (its own app)
pw-jack ./build/bin/krate                         # the rack: Korrode + Kessel + Kalliope + Krimson + Kobalt + Klang + Krunch + Klick
./build/bin/krate-render --bars 8 --out rack.wav  # the rack, rendered offline (add --kessel 0.8 --kalliope 1 --krimson 0.8 --kobalt 0.9 --klang 1 for all seven)
```

More (dependencies, testing, MIDI without hardware): [instruments/korrode/docs/BUILD.md](instruments/korrode/docs/BUILD.md).

## License

Copyright (C) 2026 Jaimie Garner

Krate is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See [LICENSE](LICENSE).

It links dynamically against Qt 6 (LGPLv3) and libjack / PipeWire's JACK library (LGPL-2.1 / MIT), all of which are compatible with GPL-3.0-or-later.

Third-party code: `third_party/dr_libs/dr_mp3.h` (MP3 decoding for backing tracks), public domain or MIT No Attribution; see its README.
