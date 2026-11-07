# Krunch: amp captures

Krunch's amps are **captures** of real amps: each channel plays one, and a channel without one is silent. A capture is a small neural network trained on recordings of that amp: you play through it and it answers as that amp did, distortion and feel included. Krunch plays the open `.nam` capture format; thousands of free ones are shared by players on community capture sites.

## Using one

1. Get `.nam` files of amps you like (community capture sites share thousands for free). **Amp-only** captures (the amp through a load box, no cabinet) take your own cabinet IRs after them; a **full-rig** capture (a miked cabinet in it) wants the channel's **CABINET** **OFF** (no IR on A or B, no speaker), or you hear two cabinets (muffled, boxy). A capture's file says which it is (its `gear_type`: `amp`, `amp_cab`...): the picker marks those with a cabinet, and choosing one sets the channel's CABINET by it, off for a cabinet in it, on otherwise (only when chosen, not when a preset or song loads; `CaptureController::hasCabinet`, `tst_captures`). One whose file doesn't say: "mic", "cab", "full rig" in the name. Most captures are made at 48 kHz, which is what the player runs at.
2. Put them in Krunch's captures folder, `~/.local/share/krunch/captures`. Use **Krate Remote** (**Files ▸ Places ▸ Amp captures (Krunch)**, upload or drop them), or **IMPORT** in the picker, or drop them on it. Files that land in the folder show up by themselves.
3. On the panel, pick the channel, then **CHOOSE AMP ▾** (in the **AMP · CHANNEL** box) opens the picker: **No amp (the channel is silent)**, then the captures. A click plays it at once (it loads in the background), so you can play and compare. The channel buttons show each channel's capture by name, or **NO AMP**.
4. Save the rig as a preset: it keeps each channel's capture with the knobs, IRs and pedals ([PRESETS.md](PRESETS.md)).

The channel's controls around the capture:

| Control | What it does |
| --- | --- |
| **GAIN** | How hard you drive the capture, −12 … +12 dB (noon: as it was captured). Like the amp's own gain, as far as the capture allows. |
| **BASS, MID, TREBLE, PRESENCE** | An EQ after the capture: shelves at 120 Hz and 2.5 kHz (±12 dB), a peak at 700 Hz (±10 dB), a shelf at 5 kHz (±8 dB). Noon is flat. |
| **VOLUME**, **GATE**, **FX LOOP**, the cabinets, **SPACE**, the pedals, **Dual Head** | As always ([DSP.md](DSP.md)). |

A channel whose capture's file has gone from the folder shows **MISSING:** and the name (its button, the amp box) and is silent until the file is back or another is chosen (`CaptureController::missing`).

Captures are played at a common loudness (−8 dB by the loudness the file records; a file that doesn't record one plays as it is), so switching between them doesn't jump. Each channel's capture is kept by name in `~/.config/krunch/captures.json`, in presets and with songs' sounds.

## How

- `dsp/Capture.cpp`: the model (built, reset to the engine's rate and block size, prewarmed), loaded off the audio thread (`app/CaptureController.cpp`, `std::async`), handed to the engine through an atomic per channel and freed once the audio thread has moved two blocks on. A model readied for another rate is ignored (the channel is silent) until reloaded (the rate changing reloads them all). A capture made at another rate than the engine's plays, with a note in the picker.
- The engine: an open-source `.nam` player library (MIT) and Eigen (MPL 2.0), fetched when Krate is built (pinned, checked by SHA-256; `third_party/nam/CMakeLists.txt`), single precision. Their licences are installed with the package (`/usr/share/doc/krate/third-party`).
- Cost: a standard-size capture about 10 % of one core at 48 kHz; lighter architectures 2 %.

Tests: `tst_krunch` capturesPlayOnTheirChannels (the capture library's own example model: on its channel only, bounded, faster than real time; a channel without one silent; a file that isn't a model refused with a reason) and gainAndEqShapeTheCapture.
