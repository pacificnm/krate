# Krunch: amp captures

A channel can play a **capture** of a real amp instead of Krunch's own preamp and power amp. A capture is a small neural network trained on recordings of that amp: you play through it and it answers as that amp did, distortion and feel included. Krunch plays the open **Neural Amp Modeler** format (`.nam` files); thousands of free ones are shared by players.

## Using one

1. Get `.nam` files of amps you like. Use **amp-only** captures (the amp through a load box, no cabinet), so your own cabinet IRs follow them. Most captures are made at 48 kHz, which is what the player runs at.
2. Put them in Krunch's captures folder, `~/.local/share/krunch/captures`. Use **Krate Remote** (**Files ▸ Places ▸ Amp captures (Krunch)**, upload or drop them), or **IMPORT** in the picker, or drop them on it. Files that land in the folder show up by themselves.
3. On the channel, **AMP** (top right of the channel) opens the list: pick a capture, or **Krunch's own amp (its tubes)**. A click plays it at once (it loads in the background), so you can play and compare.

With a capture on a channel:

| Control | What it does |
| --- | --- |
| **GAIN** | How hard you drive the capture, −12 … +12 dB (noon: as it was captured). Like the amp's own gain, as far as the capture allows. |
| **BASS, MID, TREBLE, PRESENCE** | An EQ after the capture: shelves at 120 Hz and 2.5 kHz (±12 dB), a peak at 700 Hz (±10 dB), a shelf at 5 kHz (±8 dB). Noon is flat. |
| **VOLUME**, **GATE**, **FX LOOP**, the cabinets, **SPACE**, the pedals, **Dual Head** | As always. |
| **VOICE**, **PREAMP TUBES**, the power amp (**MASTER**, **SAG**, **RESONANCE**, **TUBES**) | Not used: the capture is the amp, its power stage included. Krunch's power amp fades out of the path while a channel with a capture plays. |

Captures are played at a common loudness (−18 dB, from the level the file records), so switching between them doesn't jump. Each channel's capture is kept by name in `~/.config/krunch/captures.json`, in presets and with songs' sounds.

## How

- `dsp/Capture.cpp`: the model (`nam::get_dsp`, `Reset` to the engine's rate and block size, prewarmed), loaded off the audio thread (`app/CaptureController.cpp`, `std::async`), handed to the engine through an atomic per channel and freed once the audio thread has moved two blocks on. A model readied for another rate is ignored until reloaded (the rate changing reloads them all). A capture made at another rate than the engine's plays, with a note in the picker.
- The engine: [Neural Amp Modeler's core](https://github.com/sdatkinson/NeuralAmpModelerCore) v0.6.0 (MIT) and Eigen (MPL 2.0), fetched when Krate is built (pinned, checked by SHA-256; `third_party/nam/CMakeLists.txt`), single precision. Their licences are installed with the package (`/usr/share/doc/krate/third-party`).
- Cost: a standard WaveNet capture about 10 % of one core at 48 kHz; lighter architectures 2 %.

Tests: `tst_krunch` capturePlaysInPlaceOfTheAmp (NAM's own example model: on its channel only, bounded, faster than real time; a file that isn't a model refused with a reason).
