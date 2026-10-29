# Krunch: pedals

The pedalboard sits under the amp. Up to eight pedals, left to right, each placed **FRONT** (in front of the amp, before the gate: `frontPedals()`) or **LOOP** (in the amp's effects loop, between the preamp and the power amp: `loopPedals()`, played while the channel's LOOP is on). Each group plays in the board's order, so the guitar goes: front pedals → preamp → loop pedals → power amp. Drives belong in front; time-based effects (chorus, delay, reverb) in the loop, after the distortion, so the echoes and the room stay clean. A new pedal goes where its kind belongs, and the switch on the pedal moves it.

- **Add:** the **+** tile at the end of the board, then the pedal from the menu.
- **Reorder:** drag a pedal by its body; it drops into the nearest place.
- **Remove:** the **×** at a pedal's top right (it switches off and fades out first).
- **On / off:** the footswitch; the light shows it's on. Switching crossfades over 10 ms, so it never clicks; an added pedal fades in.
- **MIDI:** the footswitches are learnable by their place on the board, `pedal1` … `pedal8` (MIDI Learn, then press a button or a pad on the controller).

The board is kept in `~/.config/krunch/pedals.json` (each pedal's type, place, footswitch and knobs, in order) and, like the rest of the amp, in a song's sounds when you press SAVE on the Song screen.

## How it's built

`dsp/Pedal.h`: a `Pedal` has its kind (`PedalInfo`: type, name, colour, knobs), its knobs and footswitch as atomics, and the bypass crossfade; each kind of pedal only writes its effect. `PedalChain` is what the audio thread plays: a list of pedals handed over through an atomic pointer (one chain in front, one in the loop). `Pedalboard` owns the pedals on the GUI side: every add, remove, move or change of place builds new lists for both chains and hands them over, and a replaced list or a removed pedal is freed once the audio thread has moved two blocks on (`Krunch::blocksProcessed()`), so changing the board never stops or glitches the sound. `app/PedalController.cpp` (QML `Pedals`) is the panel's view of it.

## Green Drive

The classic green mid-boost overdrive, modelled on its circuit in broad strokes (`dsp/GreenDrive.cpp`):

- **The clipping stage:** an op-amp whose gain is 1 + (51 k + Drive) / 4.7 k, where the 4.7 k leg has a capacitor that leaves frequencies below about 720 Hz at unity: only the mids and highs get the gain. That extra part is clipped softly by the diodes in the feedback loop and added to the clean signal, so the bass stays tight and the mids push. The feedback capacitor rolls the treble off, lower as Drive goes up (from about 60 kHz down to about 5.6 kHz). Drive is an audio-taper 500 k pot. The stage runs at 4×.
- **Tone:** a fixed 723 Hz low-pass, with the Tone control bringing back what it took: dark at 0, about −6 dB of treble at noon, a little bright at 10.
- **Level:** −24 … +12 dB.
- Small-signal response at noon: about +3 dB at 80 Hz, +17 dB at 1 kHz, +16 dB at 5 kHz (the hump); at full drive the mids get about 30 dB.

| Knob | Range | Default |
| --- | --- | --- |
| `drive` | 0 … 10 | 5 |
| `tone` | 0 … 10 | 5 |
| `level` | −24 … +12 dB | −6 dB |

## Phaser

The classic one-knob four-stage phaser (`dsp/ModPedals.cpp`): four first-order all-pass stages swept together (their corner from about 140 Hz to 1.6 kHz, the LFO bent to linger low as a FET's sweep does), mixed half and half with the guitar: two notches moving through the low mids. **MODE**: Smooth (no feedback: notches about 60 dB deep, flat peaks), or Deep, which feeds the last stage back to the first (0.6) as the later version of the pedal does: resonant peaks about 3 dB up, notches about 16 dB deep, trimmed so it isn't louder overall.

| Knob | Range | Default |
| --- | --- | --- |
| `speed` | 0.1 … 8 Hz | about 0.5 Hz |
| `mode` | Smooth / Deep (a switch) | Smooth |

## Flanger

The classic analog bucket-brigade flanger in broad strokes (`dsp/ModPedals.cpp`): the guitar mixed half and half with a copy through a short delay, swept by a slow triangle evenly in pitch (exponentially) from 0.25 ms up to the Range setting (1 … 10 ms); **COLOR** feeds the copy back (up to 0.85) for the resonant, metallic sweep; the copy loses its top above about 8 kHz, as through the chip; the delay is read between samples with a 4-point (Hermite) curve so the fast-moving read stays clean. **MATRIX** (Filter Matrix) stops the sweep: Range then sets the delay by hand over the whole 0.25 … 10 ms, for a fixed comb.

| Knob | Range | Default |
| --- | --- | --- |
| `rate` | 0.05 … 3 Hz | about 0.2 Hz |
| `range` | top of the sweep 1 … 10 ms (Matrix: the delay) | about 4 ms |
| `color` | 0 … 10 (feedback 0 … 0.85) | 5.5 |
| `matrix` | Sweep / Matrix (a switch) | Sweep |

Both start in FRONT, where these usually go (in front of the drive or between it and the amp); either works in the loop too.

## Chorus

The guitar mixed with a copy through a short delay (7 ms) swept by a sine LFO (`dsp/TimePedals.cpp`).

| Knob | Range | Default |
| --- | --- | --- |
| `rate` | 0.1 … 8 Hz | about 0.7 Hz |
| `depth` | 0 … ±4.5 ms of sweep | 5 |
| `mix` | 0 … 100 % (at 100 %, half dry and half wet) | 70 % |

## Delay

Repeats that darken and soften as they go: in the feedback, a low-pass (Tone), a high-pass at 90 Hz and a gentle saturation, so even full Repeats never runs away. Turning Time glides the delay over about 150 ms (the pitch bends, as on a tape or bucket-brigade delay). The echo is added to the dry signal.

| Knob | Range | Default |
| --- | --- | --- |
| `time` | 40 ms … 1.5 s | about 380 ms |
| `feedback` (REPEATS) | 0 … 95 % | 35 % |
| `tone` | 1.2 … 12 kHz | about 3.4 kHz |
| `mix` | 0 … 100 % echo | 35 % |

## Reverb

Krimson's reverb (an eight-line feedback-delay network, `shared/dsp/Reverb.cpp`), fed the guitar; its tail goes through Tone and is added to the dry signal.

| Knob | Range | Default |
| --- | --- | --- |
| `decay` | 0.4 … 8 s | about 1.1 s |
| `tone` | 1.2 … 12 kHz | about 4.3 kHz |
| `mix` | 0 … 100 % | 30 % |

The amp is mono up to its cabinet, so these are mono for now; stereo chorus, ping-pong delay and a wide reverb come with a stereo loop.

Tests: `tests/tst_pedals.cpp` (bypass untouched, no clicks, the mid hump, drive, tone, level, bounded clipping, aliasing; front and loop; switches snap; the flanger's comb where its delay puts it, Color's resonance, its sweep, full Color bounded; the phaser's moving notches and Deep's peaks; the delay's time and repeats, never running away; the chorus sweep; the reverb's tail; the board's order).
