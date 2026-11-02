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

`dsp/Preamp.cpp`, per channel: two to four **triode stages** (`dsp/Tube.cpp`) at 4× the sample rate, in volts (a guitar at full scale is 1 V at the first grid), wired per voice as its kind of amp is, then the tone stack and presence.

| Voice | Stages (cathode Ω, bypassed above Hz, coupling Hz, roll-off Hz) | Between | Tone stack |
| --- | --- | --- | --- |
| Clean | 1.5 k/4/25/14 k, 1.5 k/4/20/11 k | Gain pot (−32 dB … 0) and the stack's loss (×0.12) | American |
| Crunch | 2.7 k/40/8/12 k, **10 k cold**/1.5 k/20/6 k, 1.5 k/50/25/3.5 k | Gain pot (−30 … 0), ×0.45 | British |
| Heavy | 1.5 k/60/15/12 k, **10 k cold**/1.5 k/40/6 k, 1.5 k/60/30/3.5 k, 1.5 k/60/30/3 k | Gain pot (−40 … 0), ×0.22, ×0.25 | British |
| Lead | 1.5 k/40/10/11 k, 2.7 k/100/30/6 k, 1.5 k/70/30/3.8 k, 1.5 k/100/40/3.2 k | Gain pot (−38 … 0), ×0.25, ×0.2 | British |

- **A triode stage** (`TriodeStage`): Koren's triode equations (1996) solved once per tube type against a 100 kΩ plate load from 250 V and kept as a table (plate voltage against grid-to-cathode voltage). The cathode resistor's voltage follows the stage's current through its bypass capacitor, so a stage pushed hard biases itself colder (compression that recovers), and below the bypass frequency the stage has less gain (less bass into the clipping: tight). The coupling capacitor in front is a high-pass that the grid charges when driven past the cathode (grid current: a little blocking). Then the plate swing, inverted, and the Miller roll-off. A 10 kΩ stage idles near cutoff: the cold clipper, asymmetric. The resting point is solved by bisection (silence in is silence out from the first sample).
- **Preamp tubes** (`ch<N>_tube`, all of the channel's stages): **High gain** (mu 100), **Smooth** (mu 70, softer), **Medium** (mu 60, more headroom), **Low gain** (mu 21.5: barely clips).
- The last stage's coupling capacitor (30 Hz) out; the **tone stack** (`dsp/ToneStack.cpp`, Yeh and Smith, DAFx 2006: American or British parts); **presence** −6 … +9 dB above 3 kHz, then a low-pass at 11 kHz; a trim per voice evens the channels' peaks out (the clean channel +20 dB).
- Played 12 dB softer, Crunch comes out about 3 dB quieter and cleaner; Heavy and Lead hold their level (saturated, as such amps are) but clean up and never choke (`tst_krunch` touchResponse). The stages before them (a fixed `tanh` per stage) came out within half a dB.

## Gate

`dsp/Gate.h`, before the preamp. Threshold −80 … −20 dBFS (0 = off); opens in 0.5 ms above it, holds 30 ms, closes over 80 ms once the level is 6 dB under it.

## FX loop

Between the channel volume and the power amp: the loop's pedal chain (`dsp/Pedal.h`, empty for now). The pedalboard ([PEDALS.md](PEDALS.md)) runs before the gate, always.

## Power amp

`dsp/PowerAmp.cpp`, shared by the channels. **Resonance** lifts 100 Hz (0 … +10 dB) going in, then the power tubes' own voicing; **Master** drives the output pair −30 … +12 dB. The pair (`pushPull`, at 4×): each tube's current a 3/2 law from its class AB idle under a soft ceiling, the two subtracted (symmetric, a little crossover when biased cold), unity gain when small. **Sag**: a follower on the output (20 ms attack, 200 ms release) lowers the ceiling, by the tubes' own droop times the knob, so digging in compresses.

| Power tubes (`power_tube`) | Headroom | Knee | Idle | Sag | Voicing |
| --- | --- | --- | --- | --- | --- |
| British pentode | 1.0 | 2.5 | 0.35 | 1.0 | +1.5 dB at 800 Hz |
| American beam | 1.25 | 2.0 | 0.45 | 0.7 | −1.5 dB at 600 Hz |
| Small pentode | 0.6 | 1.6 | 0.6 | 1.6 | +2 dB at 2 kHz |
| Big beam | 1.6 | 3.5 | 0.4 | 0.4 | +1.5 dB at 120 Hz |

The output is made up by the square root of the headroom, so changing tubes changes the feel more than the level (within about 4 dB).

## Cabinet

`dsp/Cabinet.cpp`. Each channel plays its IR A, its IR B, or both mixed by `ch<N>_cab_mix` (A 100 … B 100); with neither, the built-in speaker (below). A stereo IR makes the amp stereo; a mono one plays on both sides.

- **Convolution with no delay:** uniformly partitioned, 128-sample partitions. The first 128 taps run directly on every sample; the rest by FFT (overlap-save, 256 points) once per 128 samples, from the spectra of the input's past blocks. Any host block size works.
- **Instant switching:** the input's history (the direct window and the past blocks' spectra) is shared by all eight slots (four channels × A, B), so a slot that starts playing (a channel switch, a new IR) is right from its first sample.
- **Loading** (`IrKernel::build`, off the audio thread): the WAV's first two channels, resampled to the engine's rate (windowed sinc), cut at 65,536 taps (1.4 s at 48 kHz) with a 10 ms fade, normalised to unit energy and then −6.5 dB, so IRs play as loud as each other and as the built-in speaker (the 33 of one 4×12 pack land within 1.4 dB). The controller hands kernels over through atomics and frees a replaced one once the audio thread has moved two blocks on; a kernel built for another rate is ignored until rebuilt.
- **Cost:** about 0.7 % of a core for a 200 ms mono IR.

**SPACE** (`ch<N>_space`): with both IRs loaded, A moves to the left and B to the right as it turns up (at 10, hard left and right; CAB MIX still balances them). A slot's place is a gain per side: both at the centre, the far side fading out. With one IR or none it does nothing.

The **built-in speaker** (`SpeakerFilter`, always running so it's ready): high-pass 75 Hz, +3 dB at 110 Hz, −2.5 dB at 450 Hz, +4 dB at 2.4 kHz, two low-passes (5 and 6 kHz). After the cabinet, −0.5 dB of trim, so a cranked master stays near full scale, and the Level knob.

## IR library

`app/IrLibrary.cpp`: `~/.local/share/krunch/irs/`, an SQLite database (`irs.db`, table `irs`: id, pack, folder, name, channels, rate, frames, favourite, added) next to a copy of each IR named `<id>.wav`, where the id is the file's SHA-256: the same IR is never added twice, and channels, presets and songs refer to IRs by id. Imports WAV files, folders (searched with their subfolders for WAVs and zips) and zip packs; the pack is the zip's or folder's name, the folder the path inside it; macOS's `._` files and `__MACOSX` are skipped, and files we can't read are reported. `app/CabinetController.cpp` (QML `Cabinets`) keeps each channel's IR ids in `~/.config/krunch/cabinets.json` and in songs' sounds.

## Parameters

| Key | Range | Default |
| --- | --- | --- |
| `input` | input 1 or 2 | 1 |
| `channel` | 1 … 4 | 1 |
| `master` | 0 … 10 (−30 … +12 dB into the output pair) | 5 |
| `sag` | 0 … 10 | 3 |
| `resonance` | 0 … 10 (0 … +10 dB at 100 Hz) | 4 |
| `level` | −inf, −60 … +12 dB | 0 dB |
| `power_tube` | British pentode, American beam, Small pentode, Big beam | British pentode |
| `ch<N>_voice` | Clean, Crunch, Heavy, Lead | channel 1 Clean, 2 Crunch, 3 Heavy, 4 Lead |
| `ch<N>_gain` | 0 … 10 | 4, 5, 6.5, 7 |
| `ch<N>_bass`, `_mid`, `_treble`, `_presence` | 0 … 10 | per channel (channel 3: mids at 3.5) |
| `ch<N>_volume` | −inf, −40 … +6 dB | −7.8 dB (channel 4: −6.9 dB) |
| `ch<N>_gate` | off, −80 … −20 dBFS | off, off, −59 dB, −62 dB |
| `ch<N>_loop` | off / on | on |
| `ch<N>_cab_mix` | A 100 … B 100 (with both IRs) | A 50 / B 50 |
| `ch<N>_tube` | High gain, Smooth, Medium, Low gain | High gain |
| `ch<N>_space` | 0 … 10 (A left, B right) | 0 |

Learnable buttons: `channel1` … `channel4`, `loop` (the playing channel's).

Cost: 2.5–3 % of one core per channel (`krunch-render --bench`, Heavy and Lead the most).
