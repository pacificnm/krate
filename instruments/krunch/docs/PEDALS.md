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

## Signature pedals

One pedal for one well-known sound: its whole chain inside (real pedals from this board, their knobs set from the signature pedal's), each part brought in by one knob, 0 taking it out (`dsp/SignaturePedals.cpp`).

### Prism

The early-70s spacey, singing lead and clean sound. The rig behind it: a single-coil guitar into very loud, very clean amps, a silicon round fuzz for the solos, a photocell vibe for the slow swirls, a multi-head drum echo and plenty of reverb. Prism is that chain, in that order: Round Fuzz (silicon) → Vibe (chorus) → Drum Echo → Reverb, then a tilt and the level. Use it on a clean channel with headroom (Clean voice, Master up); start in FRONT.

| Knob | What it does | Default |
| --- | --- | --- |
| `fuzz` | 0 = clean; up: the round fuzz from a fat lead (its Fuzz at 4.5) to full | off |
| `vibe` | 0 = off; up: the vibe's speed (0.5 … 8 Hz), at intensity 7.5 | about 1 Hz |
| `echo` | how much drum echo: mix 15 … 60 %, swell 15 … 70 %; heads 1+3, then every head from halfway (the rhythmic swirl) | 4.5 |
| `time` | the first head (60 … 450 ms); the others repeat at 2×, 3×, 4× | about 200 ms |
| `space` | how much reverb: decay and mix together | 4.5 |
| `tone` | a tilt around 800 Hz: dark … bright (±6 dB) | slightly bright |
| `level` | −12 … +12 dB | 0 dB |

With every part at 0, Tone centred and Level at 0 dB it passes the guitar untouched.

### Madman

The early-80s neoclassical metal lead and rhythm sound, made for the **clean channel**: the pedal brings the whole rig. That rig was a classic op-amp distortion pedal (op-amp gain, germanium diodes clipping to ground) into a 10-band EQ pushing the mids and trimming the lows, slamming a cranked, master-less British amp; the leads were double-tracked. Madman is that chain, in that order:

- **The distortion** (GAIN), at 4×: op-amp gain 1 + 1 M / (4.7 k + the 1 M pot, audio taper), 2 … ~210, less of it below 720 Hz (the gain leg's capacitor); the op-amp's own ceiling; germanium diodes to ground (a soft knee at about ±0.35); the output capacitor rolling off above 6 kHz.
- **The EQ**: lows trimmed (−5 dB below 160 Hz), mids pushed at 1 kHz by MIDS (0 … +12 dB).
- **The amp**: the pedal's level turned up into Krunch's own British two-stage preamp (the Crunch voice), driven by AMP, its tone stack's BASS and TREBLE (mids up, presence 6).
- **The doubler** (DOUBLE): two "other takes" 17 and 26 ms late, each drifting a little (0.3 and 0.47 Hz), mixed in and levelled as separate performances, for the double-tracked width.
- **LEVEL**: at the defaults it comes in about 5 dB over bypass and is saturated (20 dB more pick is under 4 dB louder).

| Knob | Range | Default |
| --- | --- | --- |
| `gain` | 0 … 10 (op-amp gain 2 … ~210) | 7.5 |
| `amp` | 0 … 10 (the preamp's gain 4.5 … 10) | 6.5 |
| `bass` | 0 … 10 | 4.5 |
| `mids` | +0 … +12 dB at 1 kHz | +7.2 dB |
| `treble` | 0 … 10 | 6 |
| `double` | off … 100 % | 30 % |
| `level` | −12 … +12 dB | 0 dB |

A black case with white polka dots. Set Krunch's channel to the Clean voice with its gain low (the pedal is the drive), pick a 4×12 IR, and start in FRONT; a Delay or Reverb in the LOOP finishes the lead sound.

## Vibe

The classic photocell vibe, in broad strokes (`dsp/ClassicPedals.cpp`): four all-pass stages whose capacitors are far apart (15 nF, 220 nF, 470 pF, 4.7 nF), so the notches are spread unevenly rather than in a neat comb, swept together by a lamp shining on photocells (150 k dark down to 15 k lit, by Intensity). The cells answer brightening in about 10 ms and dimming in about 60 ms, which gives the lopsided, throbbing sweep. **MODE**: Chorus (half and half with the guitar) or Vibrato (the swept signal alone: a pitch wobble, its level steady).

| Knob | Range | Default |
| --- | --- | --- |
| `speed` | 0.5 … 8 Hz | about 1 Hz |
| `intensity` | 0 … 10 | 7 |
| `mode` | Chorus / Vibrato (a switch) | Chorus |

## Drum Echo

The multi-head magnetic drum echo, in broad strokes (`dsp/ClassicPedals.cpp`): four playback heads at one, two, three and four times the first head's time. **HEADS** picks which play (1, 1+2, 1+3, all; with more heads each is a little quieter); **SWELL** feeds them back, darkened by Tone, thinned below 120 Hz and gently saturated, so the repeats build into a rhythmic swell without running away; the drum wobbles slightly (0.15 % at 0.6 Hz) and changes speed over about 200 ms. Starts in the LOOP.

| Knob | Range | Default |
| --- | --- | --- |
| `time` | the first head, 60 … 450 ms | about 200 ms |
| `heads` | 1, 1+2, 1+3, All (a switch) | All |
| `swell` | 0 … 100 % | 35 % |
| `tone` | 1.5 … 9 kHz | about 3.1 kHz |
| `mix` | 0 … 100 % | 35 % |

## Round Fuzz

The classic two-transistor round fuzz, in broad strokes (`dsp/Fuzz.cpp`): two transistor stages, each a gain into an asymmetric clipper (saturating softly one way up to 1, cutting off sooner the other way at −0.45, the same slope at rest), at 4×, with the coupling capacitors between and a treble roll-off. **FUZZ** sets the first stage's drive, so it cleans up as you pick lighter or roll your guitar's volume back (under 10 % distortion when quiet). The asymmetry gives the strong second harmonic. **BIAS** moves both transistors' operating point (centre is the classic setting); off centre, quiet notes get stuck and sputter, the gated "dying battery" sound. **TYPE**: Germanium (a softer knee, less gain, rolled off at 4.5 kHz) or Silicon (tanh, more gain, brighter at 7.5 kHz), levelled to each other. At the default Volume it comes in about 3 dB over bypass.

| Knob | Range | Default |
| --- | --- | --- |
| `fuzz` | 0 … 10 | 7 |
| `volume` | −24 … +12 dB (before the type's trim) | 5.5 |
| `bias` | −5 … +5 around centre | centre |
| `type` | Germanium / Silicon (a switch) | Germanium |

## Compressor

A pedal compressor for sustain and evening out (`dsp/StudioPedals.cpp`): a peak detector (Attack in, 200 ms out) drives a soft-kneed (8 dB) gain computer. **SUSTAIN** lowers the threshold (−12 … −48 dBFS) and raises the ratio (2:1 … 12:1) together, with make-up gain following (half of what a full-scale signal loses), so more sustain isn't quieter; at full, 30 dB of playing dynamics come out as under 10. **BLEND** mixes the dry signal back in, keeping the pick's attack (parallel compression). Best first on the board.

| Knob | Range | Default |
| --- | --- | --- |
| `sustain` | 0 … 10 | 5 |
| `attack` | 0.5 … 50 ms | about 3.5 ms |
| `level` | −12 … +12 dB | 0 dB |
| `blend` | 0 … 100 % compressed | 100 % |

## Graphic EQs

Peaking filters at fixed frequencies, each a slider (flat in the middle), and an output **LEVEL** (±12 dB, the orange slider). A flat band costs nothing (it's skipped). In front of the amp they shape what drives it; in the loop, the amp's finished tone.

| Pedal | Bands | Range | Width |
| --- | --- | --- | --- |
| **5-Band EQ** | 120, 360, 800 Hz, 2, 4.5 kHz (guitar-centred) | ±15 dB, Q 0.9 | wider than a standard pedal |
| **10-Band EQ** | 31, 62, 125, 250, 500 Hz, 1, 2, 4, 8, 16 kHz (octaves) | ±12 dB, an octave wide | a double-wide case |

A band at full lifts its own frequency by its range (within 1 dB) and leaves frequencies three octaves away within 1 dB.

The board scrolls sideways when its pedals are wider than it is.

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

## Tremolo

The volume pulsing with an LFO, as an amp's tremolo does (`dsp/ModPedals.cpp`). **SHAPE** turns a smooth sine into a choppy square: the sine is pushed through a saturating curve (harder as Shape goes up), and the gain is smoothed over 1.5 ms, so even the square's edges never click. **DEPTH** goes from a gentle pulse to full chop (silence in the dips). Starts in the LOOP, where an amp's own tremolo sits; it works in front too.

| Knob | Range | Default |
| --- | --- | --- |
| `rate` | 0.5 … 15 Hz | about 2.3 Hz |
| `depth` | 0 … 10 | 6 |
| `shape` | Sine … Square | Sine |
| `level` | −12 … +12 dB | +1.2 dB (making up for the dips) |

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

Tests: `tests/tst_pedals.cpp` (Madman: the distortion's gain range, saturated at its defaults, Gain adding distortion, the mid push, the doubler adding takes at an even level, bounded; Prism: all out is dry, its fuzz, an echo on every head; the drum echo's heads, its swell bounded; the vibe's throb and its steady vibrato; the fuzz: more fuzz more distortion, cleaning up, even harmonics, the bias gate, silicon brighter, bounded with little aliasing; the compressor's curve, evening loud and quiet out, attack, blend; the EQs flat and transparent, each band at its frequency and not three octaves away, level; bypass untouched, no clicks, the mid hump, drive, tone, level, bounded clipping, aliasing; front and loop; switches snap; the flanger's comb where its delay puts it, Color's resonance, its sweep, full Color bounded; the phaser's moving notches and Deep's peaks; the tremolo's curve, its rate, its square without clicks; the delay's time and repeats, never running away; the chorus sweep; the reverb's tail; the board's order).
