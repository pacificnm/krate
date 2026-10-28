# Klick

A metronome instrument for the [Krate](../../README.md) rack. It clicks on every beat of the shared transport and accents the first beat of each bar; it is silent while the transport is stopped.

| Parameter | Range | Default |
| --- | --- | --- |
| `level` | −inf … 0 dB (gain = v²) | 0.7 (−3.1 dB) |
| `pitch` | 600 … 3000 Hz, exponential | 0.5 (≈1340 Hz) |
| `accent` | 0 … 100%: how much louder and higher (up to a fifth) the bar click is | 60% |
| `beats` | 2 … 7 beats per bar (6 positions) | 4/4 |

Sound: a sine burst with a 15 ms exponential decay, triggered on the exact sample each beat starts (`dsp/Klick.cpp`). Klick has no standalone app; it runs in the rack, in `krate-render`, and in its tests (`tests/tst_klick.cpp`).
