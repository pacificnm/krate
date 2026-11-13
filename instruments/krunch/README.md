# Krunch

A guitar amp for the [Krate](../../README.md) rack, built around **amp captures**. Plug a guitar into your audio interface's instrument input, choose that input under **AUDIO** in Krate's top bar, and play through Krunch. Each channel plays a capture of a real amp (a `.nam` file, an open format shared for free on community capture sites) that you choose; a channel without one is silent ([roadmap](docs/ROADMAP.md)).

**Now:** four channels, each with its own amp capture (**CHOOSE AMP ▾**; the channel buttons show each one's name, or NO AMP), gain (how hard the capture is driven, noon as captured), bass, mid, treble and presence (an EQ after it), volume, gate and an effects-loop switch; cabinets from your own impulse responses (two per channel, mixed and spread with SPACE), a built-in speaker, or none for full-rig captures; meters and the delay from input to speakers. Captures: [docs/CAPTURES.md](docs/CAPTURES.md) (upload them with Krate Remote, **Files ▸ Places ▸ Amp captures (Krunch)**, or IMPORT). How it works: [docs/DSP.md](docs/DSP.md).

**Presets:** the whole rig by name, each channel's capture, the pedals and IRs included: keep one per song or style, and switch channels within it. The preset bar on the grille (◂ ▸ learnable, SAVE, SAVE AS…, a list to load, rename or delete). See [docs/PRESETS.md](docs/PRESETS.md).

**Pedals:** under the amp: **+** adds one of 15 (Prism, a whole early-70s spacey lead and clean rig in one; Compressor; Green Drive, a mid-boost overdrive; Round Fuzz; 5-Band and 10-Band EQ; Phaser; Flanger; Vibe; Chorus; Tremolo; Delay; Drum Echo; Reverb; Dual Head, two amps set apart), **FRONT / LOOP** puts it in front of the amp or in its effects loop, drag to reorder, **×** removes, the footswitch turns it on and off (learnable). See [docs/PEDALS.md](docs/PEDALS.md).

**Tuner:** **TUNER** on the grille (learnable, for a footswitch): the note, a needle in cents and the strings, for 23 tunings (Standard E, E♭, D, C and B standard; Drop D, C♯, C, B, A; DADGAD and open tunings; 7- and 8-string; 4- and 5-string bass; or Chromatic), with A4 anywhere from 400 to 480 Hz (440 as standard). It reads the guitar before the pedals, and mutes the amp while it's open (or not). See [docs/MANUAL.md](docs/MANUAL.md#tuner).

**Cabinets:** click a channel's **A** or **B** to open the IR library; a click on an IR plays it at once, so you can compare while you play. **IMPORT** (or drop on the amp) adds WAV files, folders or zip packs; each IR is copied into the library once, and ★ marks favourites, listed first.

The panel shows the playing channel's controls under the four channel buttons. The input meter's clip light means the interface's gain is too high; under about 10 ms of delay feels like playing an amp (set the buffer to 128 frames under **AUDIO**).

Krunch has no MIDI notes and no standalone app; its knobs and the channel buttons are learnable in the rack. `krunch-render --capture amp.nam` plays a DI WAV (or a synthetic riff) through each channel offline. Tests: `tests/tst_krunch.cpp`.
