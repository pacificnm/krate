# K909: voices

Every voice is synthesized at the host sample rate inside `process()`; no samples, no allocation on the audio thread. Each voice is a small struct with `trigger(velocity, accent)` and `render(out, frames)`; the engine sums them into a stereo pair, applies Volume and a tanh soft clip (as k303's master stage).

Starting values below are what D1 ships (`dsp/Voices.cpp`); they are tuned by ear in D6 and this page is updated with the final numbers.

## Building blocks (`dsp/`)

| Block | What it is |
| --- | --- |
| `Env` | Exponential decay (`level *= k` per sample), set by its time to fall 60 dB |
| `Sine` | Phase accumulator and `std::sin`; frequency can change every sample (pitch sweeps). Cheap enough: the whole kit benches at about 1% of a core |
| `Noise` | xorshift32 white noise, one generator per voice so voices are deterministic and independent |
| `Svf` | Topology-preserving state-variable filter (lowpass, bandpass, highpass outputs), used for every noise and metal filter |
| `MetalBank` | Six square oscillators at inharmonic ratios, PolyBLEP-corrected, summed; the source for hats and cymbals |

The voice set is deterministic for a given sample rate and trigger sequence, so golden renders can be compared byte for byte (as k303's are).

## Voices

| Voice | Synthesis | Knobs |
| --- | --- | --- |
| **Bass Drum** | Sine with a pitch envelope: starts about 2.5× the base pitch and falls to it with a ~30 ms time constant; soft tanh drive for body. Attack: a short (≈3 ms) filtered noise burst plus a 1 ms pulse, mixed in by `bd_attack` | Tune: base 40–70 Hz. Decay: amplitude 80–900 ms |
| **Snare Drum** | Two sines at a 1 : 1.65 ratio (body) with a short pitch dip, plus noise through a highpass then a lowpass. Snappy sets the noise level and its decay (120–300 ms); Tone moves the noise lowpass (3–12 kHz) and the body/noise balance | Tune: body 150–260 Hz |
| **Toms (Low, Mid, Hi)** | Sine with a ~15% downward pitch sweep, a touch of noise on the attack | Tune: Low 70–130 Hz, Mid 110–190 Hz, Hi 160–280 Hz. Decay 100–900 ms |
| **Rim Shot** | An impulse into two resonant bandpasses (≈ 480 Hz and 1.7 kHz), highpassed, 90 ms decay (to −60 dB) | Level |
| **Hand Clap** | Noise through a 1.2 kHz bandpass; envelope of three 10 ms bursts 11 ms apart, then a 200 ms tail | Level |
| **Closed / Open Hat** | `MetalBank` (base ≈ 330 Hz, ratios 1, 1.48, 1.80, 2.55, 2.63, 3.90) plus a little noise, bandpass ≈ 10 kHz then highpass 7 kHz. A closed hat chokes a ringing open hat | CH decay 20–150 ms, OH decay 100–1000 ms |
| **Crash** | `MetalBank` (base 300 Hz, wider spacing) plus noise, highpass 4 kHz, lowpass 13 kHz, 1.6 s decay | Tune: bank pitch ×0.75–1.33 |
| **Ride** | `MetalBank` (base 480 Hz) with a bell partial (a sine at the bank's 4th partial, 0.9 s), highpass 5 kHz, lowpass 13 kHz, 1.5 s decay | Tune: bank pitch ×0.75–1.33 |

`MetalBank` squares alias at 48 kHz: PolyBLEP keeps that low at these pitches. `tst_voices` checks that under 2% of each metallic voice's energy is above 20 kHz (measured: hats 0.6%, cymbals under 0.1%). If it is ever audible, the bank can run at 2× through k303's half-band decimator (moved to `shared/` then).

**Levels.** Each voice has a fixed output trim (`kTrim`) so default-level hits sit in a sensible balance; `k909-render` prints every voice's peak, normal and accented. Velocity maps 1..127 to 0.5..1.0 of the voice's gain.

## Accent and velocity

A hit's gain is `level² × (velocity-derived 0.5..1) × accentGain`, where an accented hit has `accentGain = 1 + accent` (up to +6 dB with Total Accent at full). Accent also shortens the bass drum's pitch-envelope time and raises the snare's noise level slightly, so accents sound harder, not just louder.

Sequenced hits use a fixed velocity: normal steps 0.8, accented steps 1.0 plus Total Accent.

## Retriggering and polyphony

Each voice is monophonic: a new hit restarts it, with a 1 ms fade of the previous tail to avoid clicks. Closed Hat chokes Open Hat (the open hat fades over 5 ms). The eleven voices run in parallel.

## CPU

Budget: all eleven voices ringing at once under 2% of a core at 48 kHz / 64 frames, measured by `k909-render --bench`. D1 measures 1.05% with every voice hit on every beat. Voices that have decayed below −100 dB are skipped.
