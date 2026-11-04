# Krunch: DSP

Krunch is an amp capture player: each channel plays a `.nam` capture of a real amp, with Krunch's gate, EQ, effects loop, cabinets and pedals around it. Mono up to the cabinet; a stereo IR, SPACE or the Dual Head pedal makes it stereo (otherwise the same signal on both outputs). Everything is in `dsp/`, without Qt; `krunch-render` plays a DI (a WAV, or a synthetic riff) through each channel offline (`--capture amp.nam` for the amp: without one it's silent).

```
input 1|2 -> front pedals -> gate -> AMP CAPTURE (playing channel: GAIN drive, the capture, then its EQ)
          -> channel volume -> FX LOOP (if the channel's loop is on)
          -> CABINET (the channel's IRs A and B, CAB MIX and SPACE; or the built-in speaker; or off)
          -> Dual Head split (if one is on) -> level -> out
```

## Channels

Four channels, each with its own parameters (`ch<N>_<name>`), capture and cabinet. Only the playing channel's capture runs; switching starts the new one fresh and crossfades over 15 ms (both run during the fade, and both cabinets). The gate takes the playing channel's threshold, the loop its switch.

## The amp: a capture

`Krunch::runChannel`, per channel: the channel's capture (a `.nam` model of a real amp, [CAPTURES.md](CAPTURES.md)) is the amp. A channel without one (or with one readied for another rate) is **silent**.

- **GAIN** (`ch<N>_gain`): a gain in front of the capture, −12 … +12 dB; noon (0 dB) plays it as it was captured.
- The capture, played at a common loudness: −8 dB by the loudness the file records (files that don't record one play as they are).
- **The EQ after it** (biquads, noon flat): **BASS** a low shelf at 120 Hz (±12 dB), **MID** a peak at 700 Hz (Q 0.7, ±10 dB), **TREBLE** a high shelf at 2.5 kHz (±12 dB), **PRESENCE** a high shelf at 5 kHz (±8 dB). Recomputed only when a knob moves.
- **VOLUME** (`ch<N>_volume`), −40 … +6 dB (the bottom: silence), ramped across the block.

## Gate

`dsp/Gate.h`, after the front pedals and before the capture. Threshold −80 … −20 dBFS (0 = off); opens in 0.5 ms above it, holds 30 ms, closes over 80 ms once the level is 6 dB under it.

## FX loop

Between the channel volume and the cabinet: the loop's pedal chain (`dsp/Pedal.h`), played while the playing channel's loop is on. The front chain runs before the gate, always ([PEDALS.md](PEDALS.md)).

## Cabinet

`dsp/Cabinet.cpp`. Each channel plays its IR A, its IR B, or both mixed by `ch<N>_cab_mix` (A 100 … B 100); with neither, the built-in speaker (below). A stereo IR makes the amp stereo; a mono one plays on both sides.

- **Convolution with no delay:** uniformly partitioned, 128-sample partitions. The first 128 taps run directly on every sample; the rest by FFT (overlap-save, 256 points) once per 128 samples, from the spectra of the input's past blocks. Any host block size works.
- **Instant switching:** the input's history (the direct window and the past blocks' spectra) is shared by all eight slots (four channels × A, B), so a slot that starts playing (a channel switch, a new IR) is right from its first sample.
- **Loading** (`IrKernel::build`, off the audio thread): the WAV's first two channels, resampled to the engine's rate (windowed sinc), cut at 65,536 taps (1.4 s at 48 kHz) with a 10 ms fade, normalised to unit energy and then −6.5 dB, so IRs play as loud as each other and as the built-in speaker (the 33 of one 4×12 pack land within 1.4 dB). The controller hands kernels over through atomics and frees a replaced one once the audio thread has moved two blocks on; a kernel built for another rate is ignored until rebuilt.
- **Cost:** about 0.7 % of a core for a 200 ms mono IR.

**CABINET** on/off (`ch<N>_cabinet`, learnable as `cabinet`: the playing channel's): off, the channel plays no IR and no speaker, the amp straight out (for a capture with its cabinet in it), at about the level a cabinet plays at.

**SPACE** (`ch<N>_space`): with both IRs loaded, A moves to the left and B to the right as it turns up (at 10, hard left and right; CAB MIX still balances them). A slot's place is a gain per side: both at the centre, the far side fading out. With one IR or none it does nothing.

The **built-in speaker** (`dsp/Speaker.cpp`, `SpeakerFilter`, always running so it's ready), for a channel with its cabinet on and no IR: high-pass 75 Hz, +3 dB at 110 Hz, −2.5 dB at 450 Hz, +4 dB at 2.4 kHz, two low-passes (5 and 6 kHz).

## Output

After the cabinet, the **Dual Head** pedal's split (`dsp/DualHead.cpp`, when one is on in front, or in the loop with the playing channel's loop on: [PEDALS.md](PEDALS.md#dual-head)), then −0.5 dB of trim and the **Level** knob (−60 … +12 dB, the bottom silence, ramped).

## IR library

`app/IrLibrary.cpp`: `~/.local/share/krunch/irs/`, an SQLite database (`irs.db`, table `irs`: id, pack, folder, name, channels, rate, frames, favourite, added) next to a copy of each IR named `<id>.wav`, where the id is the file's SHA-256: the same IR is never added twice, and channels, presets and songs refer to IRs by id. Imports WAV files, folders (searched with their subfolders for WAVs and zips) and zip packs; the pack is the zip's or folder's name, the folder the path inside it; archivers' `._` files and `__MACOSX` folders are skipped, and files we can't read are reported. `app/CabinetController.cpp` (QML `Cabinets`) keeps each channel's IR ids in `~/.config/krunch/cabinets.json` and in songs' sounds.

## Parameters

| Key | Range | Default |
| --- | --- | --- |
| `input` | input 1 or 2 | 1 |
| `channel` | 1 … 4 | 1 |
| `level` | −inf, −60 … +12 dB | 0 dB |
| `ch<N>_gain` | −12 … +12 dB into the capture | 0 dB (as captured) |
| `ch<N>_bass`, `_treble` | ±12 dB (shelves at 120 Hz, 2.5 kHz) | 0 dB |
| `ch<N>_mid` | ±10 dB (a peak at 700 Hz) | 0 dB |
| `ch<N>_presence` | ±8 dB (a shelf at 5 kHz) | 0 dB |
| `ch<N>_volume` | −inf, −40 … +6 dB | −7.8 dB |
| `ch<N>_gate` | off, −80 … −20 dBFS | off, off, −59 dB, −62 dB |
| `ch<N>_loop` | off / on | on |
| `ch<N>_cab_mix` | A 100 … B 100 (with both IRs) | A 50 / B 50 |
| `ch<N>_space` | 0 … 10 (A left, B right) | 0 |
| `ch<N>_cabinet` | off / on (off: no IR, no speaker) | on |

Each channel's capture is not a parameter: it's chosen by name (`app/CaptureController.cpp`, QML `Captures`), kept in `~/.config/krunch/captures.json`, in presets and in songs' sounds.

Learnable buttons: `channel1` … `channel4`, `loop` and `cabinet` (the playing channel's).

Cost: mostly the capture's (a standard-size capture about 10 % of one core at 48 kHz, lighter architectures about 2 %), plus the cabinet's convolution (`krunch-render --bench`).
