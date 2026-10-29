# Krunch: DSP

Mono from the input to the speaker; the same signal on both outputs. Everything is in `dsp/`, without Qt; `krunch-render` plays a DI (a WAV, or a synthetic riff) through each channel offline.

```
input 1|2 -> front pedals -> gate -> PREAMP (playing channel) -> channel volume
          -> FX LOOP (if the channel's loop is on) -> POWER AMP
          -> CABINET (the channel's IRs A and B, or the built-in speaker) -> level -> out
```

## Channels

Four channels, each with its own parameters (`ch<N>_<name>`) and cabinet. Only the playing channel's preamp runs; switching starts the new one fresh and crossfades over 15 ms (both run during the fade, and both cabinets). The gate takes the playing channel's threshold, the loop its switch.

## Preamp

`dsp/Preamp.cpp`, per channel, by its **voice**:

| Voice | Stages | Into the first stage | Gain knob (first stage) | Tone stack |
| --- | --- | --- | --- | --- |
| Clean | 1 | high-pass 30 Hz | −6 … +18 dB | American (scooped, glassy) |
| Crunch | 2 | high-pass 60 Hz, +2 dB at 900 Hz | 0 … +30 dB | British |
| Heavy | 3 | high-pass 140 Hz, +5 dB at 850 Hz (tight) | +6 … +40 dB | British |
| Lead | 4 | high-pass 100 Hz, +6 dB at 750 Hz | +6 … +42 dB | British |

- **Stages** run at 4× the sample rate (`dsp/Oversampler.h`: two half-band 2× stages each way, about 0.5 ms of delay). Each is a gain, a biased `tanh` (asymmetric: even harmonics), a coupling-capacitor high-pass and a treble roll-off (one-pole each, per voice). Later stages have a fixed gain (+12 … +14 dB) and half the bias.
- **Tone stack** (`dsp/ToneStack.cpp`): the passive bass / mid / treble network modelled from the circuit (Yeh and Smith, DAFx 2006): third order, the controls interact as on an amp, bass on a log taper; bilinear transform. Two sets of parts: American (the mid scoop near 700 Hz is about 12 dB at noon) and British (less scooped).
- **Presence:** a high shelf at 3 kHz, −6 … +9 dB; then a low-pass at 11 kHz.
- A trim per voice evens the channels out (the defaults play within 2 dB of each other).

## Gate

`dsp/Gate.h`, before the preamp. Threshold −80 … −20 dBFS (0 = off); opens in 0.5 ms above it, holds 30 ms, closes over 80 ms once the level is 6 dB under it.

## FX loop

Between the channel volume and the power amp: the loop's pedal chain (`dsp/Pedal.h`, empty until there are pedals). The front pedals run before the gate, always.

## Power amp

`dsp/PowerAmp.cpp`, shared by the channels. **Resonance** lifts 100 Hz (0 … +10 dB) going in; **Master** drives the output stage −24 … +18 dB (it clips from about 0 dB); the stage is a symmetric `tanh` at 4×. **Sag**: a follower on the output (4 ms attack, 150 ms release) lowers the headroom, so digging in compresses.

## Cabinet

`dsp/Cabinet.cpp`. Each channel plays its IR A, its IR B, or both mixed by `ch<N>_cab_mix` (A 100 … B 100); with neither, the built-in speaker (below). A stereo IR makes the amp stereo; a mono one plays on both sides.

- **Convolution with no delay:** uniformly partitioned, 128-sample partitions. The first 128 taps run directly on every sample; the rest by FFT (overlap-save, 256 points) once per 128 samples, from the spectra of the input's past blocks. Any host block size works.
- **Instant switching:** the input's history (the direct window and the past blocks' spectra) is shared by all eight slots (four channels × A, B), so a slot that starts playing (a channel switch, a new IR) is right from its first sample.
- **Loading** (`IrKernel::build`, off the audio thread): the WAV's first two channels, resampled to the engine's rate (windowed sinc), cut at 65,536 taps (1.4 s at 48 kHz) with a 10 ms fade, normalised to unit energy and then −6.5 dB, so IRs play as loud as each other and as the built-in speaker (the 33 of one 4×12 pack land within 1.4 dB). The controller hands kernels over through atomics and frees a replaced one once the audio thread has moved two blocks on; a kernel built for another rate is ignored until rebuilt.
- **Cost:** about 0.7 % of a core for a 200 ms mono IR.

The **built-in speaker** (`SpeakerFilter`, always running so it's ready): high-pass 75 Hz, +3 dB at 110 Hz, −2.5 dB at 450 Hz, +4 dB at 2.4 kHz, two low-passes (5 and 6 kHz). After the cabinet, −4 dB of trim, so a cranked master stays near full scale, and the Level knob.

## IR library

`app/IrLibrary.cpp`: `~/.local/share/krunch/irs/`, an SQLite database (`irs.db`, table `irs`: id, pack, folder, name, channels, rate, frames, favourite, added) next to a copy of each IR named `<id>.wav`, where the id is the file's SHA-256: the same IR is never added twice, and channels, presets and songs refer to IRs by id. Imports WAV files, folders (searched with their subfolders for WAVs and zips) and zip packs; the pack is the zip's or folder's name, the folder the path inside it; macOS's `._` files and `__MACOSX` are skipped, and files we can't read are reported. `app/CabinetController.cpp` (QML `Cabinets`) keeps each channel's IR ids in `~/.config/krunch/cabinets.json` and in songs' sounds.

## Parameters

| Key | Range | Default |
| --- | --- | --- |
| `input` | input 1 or 2 | 1 |
| `channel` | 1 … 4 | 1 |
| `master` | 0 … 10 (−24 … +18 dB into the output stage) | 5 |
| `sag` | 0 … 10 | 3 |
| `resonance` | 0 … 10 (0 … +10 dB at 100 Hz) | 4 |
| `level` | −inf, −60 … +12 dB | 0 dB |
| `ch<N>_voice` | Clean, Crunch, Heavy, Lead | channel 1 Clean, 2 Crunch, 3 Heavy, 4 Lead |
| `ch<N>_gain` | 0 … 10 | 4, 5, 6.5, 7 |
| `ch<N>_bass`, `_mid`, `_treble`, `_presence` | 0 … 10 | per channel (channel 3: mids at 3.5) |
| `ch<N>_volume` | −inf, −40 … +6 dB | −7.8 dB (channel 4: −6.9 dB) |
| `ch<N>_gate` | off, −80 … −20 dBFS | off, off, −59 dB, −62 dB |
| `ch<N>_loop` | off / on | on |
| `ch<N>_cab_mix` | A 100 … B 100 (with both IRs) | A 50 / B 50 |

Learnable buttons: `channel1` … `channel4`, `loop` (the playing channel's).

Cost: 1–3 % of one core per channel (`krunch-render --bench`, the lead voice the most).
