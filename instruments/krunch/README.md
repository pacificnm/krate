# Krunch

A guitar amp for the [Krate](../../README.md) rack. Plug a guitar into your audio interface's instrument input, choose that input under **AUDIO** in Krate's top bar, and play through Krunch. Where it is going: several channels, each with its own gain, volume and EQ; a preamp and a power amp with an effects loop between them; cabinets from an impulse-response library you fill with your own IRs; pedals in front of the amp and in the loop ([roadmap](docs/ROADMAP.md)).

**Now (G2):** four channels (Clean, Crunch, Heavy, Lead voices to start, each with gain, bass, mid, treble, presence, volume, gate and an effects-loop switch); a power amp with Master, Sag and Resonance; cabinets from your own impulse responses (two per channel, mixed), or a built-in speaker; meters and the delay from input to speakers. How it works: [docs/DSP.md](docs/DSP.md).

**Pedals:** under the amp: **+** adds one (Prism, a whole early-70s spacey lead and clean rig in one; Madman, the early-80s neoclassical metal rig in one, for the clean channel; Compressor; Green Drive, a mid-boost overdrive; Round Fuzz; 5-Band and 10-Band EQ; Phaser; Flanger; Vibe; Chorus; Tremolo; Delay; Drum Echo; Reverb), **FRONT / LOOP** puts it in front of the amp or in its effects loop, drag to reorder, **×** removes, the footswitch turns it on and off (learnable). See [docs/PEDALS.md](docs/PEDALS.md).

**Cabinets:** click a channel's **A** or **B** to open the IR library; a click on an IR plays it at once, so you can compare while you play. **IMPORT** (or drop on the amp) adds WAV files, folders or zip packs; each IR is copied into the library once, and ★ marks favourites, listed first.

The panel shows the playing channel's controls under the four channel buttons. The input meter's clip light means the interface's gain is too high; under about 10 ms of delay feels like playing an amp (set the buffer to 128 frames under **AUDIO**).

Krunch has no MIDI notes and no standalone app; its knobs and the channel buttons are learnable in the rack. `krunch-render` plays a DI WAV (or a synthetic riff) through each channel offline. Tests: `tests/tst_krunch.cpp`.
