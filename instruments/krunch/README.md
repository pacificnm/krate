# Krunch

A guitar amp for the [Krate](../../README.md) rack. Plug a guitar into your audio interface's instrument input, choose that input under **AUDIO** in Krate's top bar, and play through Krunch. Where it is going: several channels, each with its own gain, volume and EQ; a preamp and a power amp with an effects loop between them; cabinets from an impulse-response library you fill with your own IRs; pedals in front of the amp and in the loop ([roadmap](docs/ROADMAP.md)).

**Now (G0):** the input passes straight through at the Level knob, so you can check the connection, the level and the delay.

| Parameter | Range | Default |
| --- | --- | --- |
| `input` | the interface's input 1 or 2 (2 positions) | 1 |
| `level` | −inf, −60 … +12 dB | 0 dB |

The panel shows the input as it arrives (with a clip light: turn the interface's gain down if it lights), the output, which capture port the guitar is on, and the delay from input to speakers. Under about 10 ms feels like playing an amp: set the buffer to 128 (or 64) frames under **AUDIO**.

Krunch has no MIDI notes and no standalone app; its knobs are learnable in the rack. Tests: `tests/tst_krunch.cpp`.
