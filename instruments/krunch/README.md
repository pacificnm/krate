# Krunch

A guitar amp for the [Krate](../../README.md) rack. Plug a guitar into your audio interface's instrument input, choose that input under **AUDIO** in Krate's top bar, and play through Krunch. Where it is going: several channels, each with its own gain, volume and EQ; a preamp and a power amp with an effects loop between them; cabinets from an impulse-response library you fill with your own IRs; pedals in front of the amp and in the loop ([roadmap](docs/ROADMAP.md)).

**Now (G1):** four channels (Clean, Crunch, Heavy, Lead voices to start, each with gain, bass, mid, treble, presence, volume, gate and an effects-loop switch); a power amp with Master, Sag and Resonance; a fixed speaker filter until cabinets arrive; meters and the delay from input to speakers. How it works: [docs/DSP.md](docs/DSP.md).

The panel shows the playing channel's controls under the four channel buttons. The input meter's clip light means the interface's gain is too high; under about 10 ms of delay feels like playing an amp (set the buffer to 128 frames under **AUDIO**).

Krunch has no MIDI notes and no standalone app; its knobs and the channel buttons are learnable in the rack. `krunch-render` plays a DI WAV (or a synthetic riff) through each channel offline. Tests: `tests/tst_krunch.cpp`.
