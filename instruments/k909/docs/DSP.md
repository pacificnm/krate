# K909: voices

Every voice is synthesized at the host sample rate inside `process()`; no samples, no allocation on the audio thread. Each voice is a small struct with `trigger(velocity, accent)` and `render(out, frames)`; the engine sums them into a stereo pair (each voice panned, driven and sent to the effects: [Mixer and effects](#mixer-and-effects)), then the bus compressor and crusher, then Volume; the host's master stage soft-clips. **All the newer controls default to the original kit**: at their defaults every render is byte-identical to before they existed (checked against the reference renders).

The values below are what ships (`dsp/Voices.cpp`). They are a starting point by design and measurement; tuning by ear goes on from listening feedback, and this page follows the code.

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
| **Bass Drum** | Sine with a pitch envelope: starts (1 + Sweep) × the base pitch (Sweep 0–3, default 1.5: 2.5×) and falls to it (Bend: a ~30 ms time constant at the default, ¼–4×; accents ¾ of it); the body through tanh(k·x)/tanh(k), k = 1.5 × 4^(2·Tone−1) (0.375 round … 6 near square). Attack: a short (≈3 ms) filtered noise burst plus a 1 ms pulse, mixed in by `bd_attack` | Tune: base 40–70 Hz. Decay: amplitude 80–900 ms. Sweep, Bend, Tone |
| **Snare Drum** | Two sines at a 1 : 1.65 ratio (body) with a short pitch dip, plus noise through a highpass then a lowpass. Snappy sets the noise level and its decay (120–300 ms); Tone moves the noise lowpass (3–12 kHz) and the body/noise balance; Decay sets the body's (180 ms at the default, ¼–4×). Crack (default 0) adds a 25 ms snap of noise highpassed at 4.5 kHz | Tune: body 150–260 Hz |
| **Toms (Low, Mid, Hi)** | Sine with a ~15% downward pitch sweep, a touch of noise on the attack | Tune: Low 70–130 Hz, Mid 110–190 Hz, Hi 160–280 Hz. Decay 100–900 ms |
| **Rim Shot** | An impulse into two resonant bandpasses (≈ 480 Hz and 1.7 kHz at the default Tune, an octave either way), highpassed, 90 ms decay (to −60 dB; ¼–4×) | Tune, Level, Decay |
| **Hand Clap** | Noise through a 1.2 kHz bandpass (Tune: an octave either way); envelope of three 10 ms bursts 11 ms apart (Spread: 4.4–27.5 ms), then a 200 ms tail (Decay: ¼–4×) | Tune, Level, Decay, Spread |
| **Closed / Open Hat** | `MetalBank` (base ≈ 330 Hz, ratios 1, 1.48, 1.80, 2.55, 2.63, 3.90) plus a little noise, bandpass ≈ 10 kHz then highpass 7 kHz; Tune (both hats) scales the bank and both filters together, ×⅔–1.5. A closed hat chokes a ringing open hat | CH decay 20–150 ms, OH decay 100–1000 ms, Tune |
| **Crash** | `MetalBank` (base 300 Hz, wider spacing) plus noise, highpass 4 kHz, lowpass 13 kHz, 1.6 s decay | Tune: bank pitch ×0.75–1.33 |
| **Ride** | `MetalBank` (base 480 Hz) with a bell partial (a sine at the bank's 4th partial, 0.9 s), highpass 5 kHz, lowpass 13 kHz, 1.5 s decay | Tune: bank pitch ×0.75–1.33 |

| **Metal** | Six struck modes (sines, each with its own exponential decay: the higher, the shorter, ×1 … ×0.25 of Decay) plus a 12 ms burst of noise through a bandpass at 3× the lowest mode (the strike). Material morphs the modes' frequency ratios (log-frequency) from a free bar or pipe (1, 2.76, 5.40, 8.93, 13.3, 18.6: far apart, clangy) to a plate or sheet (1, 1.36, 1.67, 2.10, 2.42, 2.98: dense). Modes above 0.45 × the rate are dropped | Tune: lowest mode 120–1500 Hz. Decay 0.15–3 s. Material |
| **Clank** | FM (phase modulation): a sine carrier, two sine modulators at inharmonic ratios (×1.414 and ×3.732, the second at half depth), index up to 12 × Hardness, falling at 0.3 × the amplitude decay (bright metallic strike, then a duller ring), plus a 6 ms noise click | Tune: carrier 80–900 Hz. Decay 50 ms–1.2 s. Hardness |
| **Noise** (stab) | White noise through a resonant state-variable filter (Q 3; lowpass plus 0.6 × bandpass) whose cutoff starts Sweep × 4 octaves above Tone and falls over half the decay, then tanh grit | Tone 300 Hz–8 kHz. Decay 30 ms–1.5 s. Sweep 0–4 octaves |
| **Boom** | A long sub: a sine falling from 3× to its pitch (60 dB in 250 ms), through tanh(2.5x) for weight | Tune: 28–65 Hz. Decay 0.3–3 s |
| **Sample** | A WAV file of your own ([Sample](#sample)), read at its pitch from Start, faded by Decay | Tune ±24 semitones. Decay 50 ms–4 s, or Full (the whole sample). Start 0–90 % into it |

`MetalBank` squares alias at 48 kHz: PolyBLEP keeps that low at these pitches. `tst_voices` checks that under 2% of each metallic voice's energy is above 20 kHz (measured: hats 0.6%, cymbals under 0.1%). If it is ever audible, the bank can run at 2× through k303's half-band decimator (moved to `shared/` then).

**Levels.** Each voice has a fixed output trim (`kTrim` in `dsp/Voices.cpp`: BD 0.9, SD 0.75, toms 0.8, rim 0.35, clap 0.9, hats 1.4, crash 1.9, ride 1.75; metal 1.0, clank 0.85, noise 1.0, boom 1.15, sample 1.4) so default-level hits sit in a sensible balance; `k909-render` prints every voice's peak, normal and accented. The engine then applies Volume and a fixed −3 dB output trim (`kOutputTrim`), so the demo beat at the default Volume peaks around −3.5 dBFS, under the host's soft clip. Velocity maps 1..127 to 0.5..1.0 of the voice's gain.

## Sample

The SAMPLE voice plays a WAV file (16, 24 or 32-bit PCM or 32-bit float, any rate, any channel count; the shared `krate::readWavFile`, which Krunch's impulse responses use too). On loading, off the audio thread, the file becomes a `Sample`: its first two channels averaged to mono, cut to 30 s, peak-normalized to 0.9 (so the LEVEL knob behaves the same for any file), kept at its own rate.

- **Playback**: the read position starts at Start × 90 % of the length and moves by (file rate / engine rate) × 2^(Tune / 12) per sample; a 4-point (hermite) read between the file's samples. A start inside the file fades in over 2 ms; the last 5 ms of output fade so a file cut short ends without a click. Decay at the top plays the whole sample; below, an exponential fade (60 dB in 50 ms … 4 s). Velocity, accent, per-step velocity and tune / decay locks, drive, pan and sends all apply as to every voice.
- **Handing it over**: the controller owns every `Sample`; `Engine::setSample()` publishes the new one through an atomic, and the engine takes it at the start of the next block. A hit still playing the old sample stops at once (its last value fades out as the 1 ms declick residual), so nothing reads the old one after that block; the controller frees it once `Engine::blocksProcessed()` has moved two blocks past the swap. No allocation or freeing on the audio thread.
- **Notes**: 60 and 61. No sample: the voice is silent.

## Mixer and effects

Every voice has four mixer knobs (the panel's MIX page):

| Knob | What it does |
| --- | --- |
| **Pan** | Left … right, a balance law (centre: both sides at unity). The defaults are the original fixed positions (toms spread, hats slightly right, ride slightly left) |
| **Drive** | tanh(g·x)/tanh(g) on the voice before its level, g = 24 × Drive² (to +28 dB): the peak stays put, the body and tail come up and square off. First-order antiderivative anti-aliasing (the average of tanh over each sample step, (F(u) − F(u′))/(u − u′) with F = ln cosh): far less aliasing than a plain tanh, with no oversampling and no latency. 0 = bypassed |
| **Reverb**, **Delay** | Sends (post-pan) to the two effects |

The effects run on the sends' buses, rendered 256 frames at a time, and their wet signal is added to the mix. Each runs only while some voice sends to it, and starts empty when it comes back.

| Effect | Design |
| --- | --- |
| **Reverb** | `krate::Reverb` (the shared eight-line FDN). REV SIZE: decay 0.4–8 s. **GATE** on: the reverb rings long and dense (3.8 s) and a gate on its wet output opens (1 ms) whenever a send rises above −50 dBFS, holds 80–500 ms (REV SIZE), then closes over 15 ms: the big, chopped 80s / industrial snare |
| **Delay** | `krate::Delay` (shared ping-pong). DLY TIME: 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4., 1/2 at the transport's tempo (the Tempo knob standalone). FEEDBACK 0–90 % (darker each repeat) |
| **Comp** | On the whole mix: feed-forward, stereo-linked; a peak detector with instant attack and a release from 150 ms (gentle glue) down to 40 ms at full (it pumps with the beat); threshold −6 … −30 dB, ratio 1.5 … 10, three quarters of the reduction made up (the compressed factory kits come out denser than Classic: RMS about 2–4 dB up at 8–10 dB more headroom). Reduction applies at once (no overshoot on the hits), recovery over 5 ms. Measured on the first factory pattern (Floor): the peak-to-average ratio from 10.2 to 6.7 at full. 0 = bypassed |
| **Crush** | After the compressor: bits 16 → 4 and the sample rate down to 1/16 (each sample held 1–16 frames, 1 + 15 × Crush²). 0 = bypassed |

## Accent and velocity

A hit's gain is `level² × (velocity-derived 0.5..1) × accentGain`, where an accented hit has `accentGain = 1 + accent` (up to +6 dB with Total Accent at full). Accent also shortens the bass drum's pitch-envelope time and raises the snare's noise level slightly, so accents sound harder, not just louder.

Sequenced hits use a fixed velocity: normal steps 0.8, accented steps 1.0 plus Total Accent.

## Retriggering and polyphony

Each voice is monophonic: a new hit restarts it, with a 1 ms fade of the previous tail to avoid clicks. Closed Hat chokes Open Hat (the open hat fades over 5 ms). The eleven voices run in parallel.

## CPU

Budget: all voices ringing at once under 2% of a core at 48 kHz / 64 frames, measured by `k909-render --bench`. D1 measured 1.05% with the 11 voices hit on every beat; now all 16 voices (the sample playing a test tone), 1.9%; with every voice driven and sent to both effects, the gated reverb, the compressor and the crusher on, 3.4%. Voices that have decayed below −100 dB are skipped.
