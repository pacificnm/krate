# Konnekt manual

Konnekt is a modular synthesizer: instead of a fixed synth with its parts already joined, you get the parts, oscillators, filters, envelopes, LFOs, VCAs, mixers, effects, and join them yourself with patch cables. Every synth in Krate is made of these same parts; here you can see each one, hear what it does, and wire them however you like. It plays from your MIDI keyboard, up to 8 notes at once.

## Quick start

1. **Play.** Konnekt opens with a small patch: MIDI In → Oscillator → VCA (opened by an Envelope) → Output. Play your keyboard.
2. **Point at things, right-click them.** Hover over any module, knob or jack: the bar at the bottom says what it is in a line. Right-click it for the whole story: what it is, how it works, why you'd want it, where it goes, what to try (see [Explanations](#explanations)).
3. **Do the lessons.** **LESSONS** loads patches that build a synth up a module at a time, each with what to listen for and what to try. Start with lesson 1.
4. **Add a module.** **+ ADD MODULE**, pick one: it appears in view. Drag it by its header.
5. **Wire.** Click a jack, then another: an output and an input. The cable follows the modules when you drag them.
6. **Keep it.** **PATCHES**, type a name, **SAVE**.
7. **Take a finished sound apart.** **PRESETS** loads one of 49 ready-made sounds (basses, leads, 80s and synth-pop leads, pads, drum loops, effects), each with how it's built and what to tweak.

The rack as you leave it comes back next time.

## Signals and jacks

Everything that flows through a cable is a signal, a number changing over time. The jacks are coloured by what they usually carry:

| Colour | Signal | What it is |
| --- | --- | --- |
| Orange | Audio | A sound, swinging between -1 and +1 hundreds or thousands of times a second. |
| Blue | Pitch | A note: 1 per octave, 0 is middle C. +1 is the C above, 1/12 a semitone. |
| Green | Gate | 1 while something is on (a key held, a clock tick), 0 when off. |
| Violet | Modulation | A slow control: an envelope 0..1, an LFO -1..+1. |

The colours are a guide, not a rule: any output can go into any input. An LFO into an audio input is a very low hum; an oscillator into a modulation input moves something at audio rate (that's FM). Experimenting is the point.

**Inputs** are round; **outputs** are square, with a strip under them showing their signal over the last second and a half (each slice from its lowest to its highest value). A dot in an input means it's plugged.

**Unplugged inputs** read 0, or the knob that goes with them: a VCA's CV with nothing plugged in reads its LEVEL knob. **Amount knobs** scale what comes into an input: the Oscillator's FM knob sets how far its FM input moves the pitch.

## Wiring

| Do | What happens |
| --- | --- |
| Click a jack, then another | An output and an input are joined. The cable sags between them. |
| Click a plugged input | Its cable comes out, still held by its other end: click another input to move it there, or the empty rack to drop it. |
| Click an input that has a cable, from a held cable | The new cable replaces the old one: an input takes one cable. |
| Esc, or click the empty rack | Lets go of a held cable. |

An output can feed as many inputs as you like. A cable can even go back up the chain to make a loop; a loop's signal arrives one block (16 samples, a third of a millisecond) late, which is what makes feedback patches possible.

## Voices

**VOICES** (1–8, top bar) sets how many notes can sound at once. The lights next to it show the notes held. Modules that run **per voice** (oscillators, filters, envelopes, VCAs, LFOs, mixers, utilities, drive, crusher, ring mod) run once for every voice: each note gets its own oscillator, its own envelope. Modules that run **once** (the effects, the clock, the sequencer, the scope, the output) get the voices mixed together. Hover a module's header to see which it is.

With **1 voice** Konnekt is a monophonic synth: the newest key plays, and letting it go returns to a key still held without restarting the envelopes (legato). With more, each new note takes a free voice, or the one that started longest ago.

## Seeing signals

- **The strip under each output** shows its last 1.5 seconds: watch an envelope rise and fall, an LFO wave, a gate open and close. For several voices it shows the voice of the newest note.
- **Right-click an output** to watch it on the **scope** (bottom right): the actual waveform, with 5 ms, 20 ms or 80 ms across. **✕** closes it. (The right-click also opens the output's explanation.)
- **The Scope module** is a scope on the rack: plug anything into it (its THRU passes the signal on) and click its screen.

## Explanations

**Right-click** anything to read about it in the panel on the right: a knob or button, an input or output jack, a module's header, a button in the top bar (or the KONNEKT name, for this), or a module in **+ ADD MODULE** before you add it.

Each explanation has:

- **What it is**, **How it works**, **Why you need it**, **Where it goes** (in a patch) and **Try this**.
- **What it's doing now** on your rack: a knob's value, range and default and which input it works with, its MIDI Learn link; an input's signal and what's plugged into it (or what it reads with nothing plugged in); an output's signal and every input its cables go to. These follow the rack as you play and turn knobs.
- **Links**: a module's knobs, inputs and outputs, the module a part is on, the signal a jack carries, related modules. Click one to read it; **◂** goes back; **✕** or Esc closes the panel.

For an output, **SHOW ON SCOPE** puts it on the scope. While **LEARN** is on, right-clicking a knob forgets its MIDI link instead.

## Modules

### In / Out

| Module | What it does |
| --- | --- |
| **MIDI In** | Your keyboard. PITCH, GATE, VELOCITY and PRESSURE per voice; MOD WHEEL and BEND for all. BEND sets the pitch bend's range. Start almost every patch here. |
| **Output** | To your speakers. Nothing is heard until something is plugged into LEFT (and RIGHT, for stereo). VOLUME. |
| **Scope** | A screen for a signal; TIME sets how much it shows. |

### Sources

| Module | What it does |
| --- | --- |
| **Oscillator** | Saw, pulse, triangle and sine outputs at once from one pitch. OCTAVE, TUNE; WIDTH for the pulse; FM (pitch modulation: vibrato, sweeps), LIN FM (frequency modulation: bells and metal), PWM (pulse-width modulation); SYNC (restart with another oscillator: the tearing sync sound). |
| **Noise** | White (bright hiss) and pink (deeper) noise: for wind, breath, snares and hats through a filter, or random modulation. |

### Filters

All three have CUTOFF, RESONANCE, CV (how far the CUTOFF input moves it, in octaves) and a KEY input (plug MIDI In's PITCH so the filter follows the notes).

| Module | What it does |
| --- | --- |
| **Ladder Filter** | Kalliope's transistor ladder: smooth and warm, SLOPE -12 or -24 dB, DRIVE for grit, resonance to self-oscillation. |
| **OTA Filter** | Kobalt's four-pole filter: each stage saturates, so a loud resonant note buzzes and rubbers. |
| **State-Variable Filter** | Low-pass, band-pass and high-pass outputs at once: move the cable between them to hear the difference. |

### Envelopes and modulation

| Module | What it does |
| --- | --- |
| **Envelope** | ATTACK, DECAY, SUSTAIN, RELEASE, started by its GATE; RETRIG starts it again while the gate is held. ENV and INVERTED out. |
| **LFO** | A slow wave: RATE, WAVE (triangle, sine, saws, square, random, smooth random), RATE MOD for its RATE input, RESET to restart it. OUT (-1..+1) and 0..1. |
| **Sample & Hold** | Takes its input (or its own noise) on each TRIGGER and holds it: stepped random. |
| **Slew** | Smooths jumps: RISE and FALL times. Between MIDI In's PITCH and an oscillator: glide (portamento). |

### Utilities

| Module | What it does |
| --- | --- |
| **VCA** | A volume a signal controls: LEVEL (with nothing in its CV), CV (how much the CV opens it), RESPONSE (linear or exponential). An envelope into CV is what starts and stops a note. |
| **Mixer** | Four inputs added at their levels. |
| **Attenuverter** | Scales a signal (SCALE: more, less, none, or upside down) and shifts it (OFFSET). Unplugged, its OFFSET is a steady value: a fixed pitch, a VCA held open. |
| **Ring Mod** | Multiplies two signals (MIX against the first alone): bells and robots from two oscillators, tremolo from an LFO. |

### Effects

| Module | What it does |
| --- | --- |
| **Drive** | Soft clipping (DRIVE) into a wavefolder (FOLD), LEVEL. Per voice. |
| **Crusher** | BITS (16 down to 2) and RATE (down to 1/32): digital damage. Per voice. |
| **Chorus** | Thickens and spreads a sound to stereo: MODE I, II, Ensemble. |
| **Delay** | Echoes bouncing left and right: TIME, FEEDBACK, MIX. |
| **Reverb** | A room: SIZE, MIX. |

### Sequencing

| Module | What it does |
| --- | --- |
| **Clock** | TEMPO and RUN; BAR, BEAT, 1/8 and 1/16 gates out; RESET in. |
| **Sequencer** | Eight steps, each a note (knobs 1–8, ±12 semitones) and on or off (the buttons under them); LENGTH (1–8) and GATE (how long each step's gate stays open). CLOCK advances it, RESET returns to step 1, TRANSPOSE adds to the pitch (plug MIDI In's PITCH to play the pattern from a key). PITCH, GATE and STEP out. |

## The lessons

**LESSONS** in the top bar. Each loads a patch (replacing the rack: save yours first) and opens its explanation (bottom left), with **◂ ▸** to go through them and **✕** to close the text (the patch stays).

1. **A tone**: an oscillator, through a VCA your keys open. The waves.
2. **Notes that start and stop**: an envelope and a VCA.
3. **The filter**: cutoff and resonance.
4. **A filter envelope**: the movement that makes synths sound alive.
5. **LFOs**: vibrato and wah.
6. **Two oscillators**: detune, octaves, sync.
7. **FM**: bells and metal.
8. **A sequenced bass part**: a clock, a sequencer, glide; hold a key around C3 to move it.
9. **Noise and percussion**: drums with no oscillator.
10. **Effects**: chorus, delay and reverb in stereo.

## Presets

**PRESETS** in the top bar: 49 finished sounds in the styles of industrial, EBM (body music), acid, big beat, 80s synth-pop and new wave, to play and to take apart. Click one to load it (the rack is replaced: save yours first); its text opens at the bottom left (the sound, how it's built, how to play it, what to tweak), with **◂ ▸** to step through them and **✕** to close the text.

| Category | Presets |
| --- | --- |
| Bass | **Body Music Bass** (a sequenced EBM pluck), **Acid Squelch** (a resonant, sliding acid line), **Industrial Grind Bass** (distorted, for playing under guitars), **Big Beat Wobble** (an LFO-filtered growl), **Sub Drop** (a sine that drops into each note), **Rubber Pluck Bass** (a resonant OTA 'boing'), **Sync Bass** (hard sync, ripping), **FM Slap Bass** (a twangy digital slap), **Rolling Detuned Bass** (three saws churning), **Played Acid Bass** (acid under your fingers), **Octave Pulse Bass** (eighth-note octaves that play themselves), **Crushed Bass** (lo-fi and aliasing), **Folded Bass** (a wavefolder brightened by the envelope), **Growl Bass** (a filter shaken at 22 Hz) |
| Leads and stabs | **Rave Stab** (chords that hit and fade), **Siren Lead**, **Sync Scream** (a tearing hard-sync lead), **Whirling Rave Lead** (detuned, swirling pulses that swoop up), **Metal Clang** (ring-modulated scrap metal), **Industrial Power Stab** (distorted root-fifth-octave hits), **House Organ Stab**, **Acid Lead** (a sliding, squelching line), **Screaming Resonance Lead** (a filter on the edge of singing), **Trance Saw Lead** (a wall of detuned saws), **Crushed Lead** (lo-fi square), **Trance Gate Stab** (held chords pulsing on the clock), **Metallic FM Stab**, **Random Step Lead** (a random tune around the key you hold) |
| 80s and synth-pop leads | **Pop Pluck Riff** (a bright sequenced riff that plays itself), **Pop Pluck Lead** (the same pluck to play), **Breathy Pipe Lead** (a sampled-style pipe with breath and a vibrato that fades in), **Anvil Lead** (a ring-modulated, hammered-metal melody), **Dark Detuned Lead** (two squares out of tune, gliding), **Square Glide Lead** (a bouncy square that slides, with a dotted echo), **Cold Wave Lead** (a wall of detuned saws, an octave deep), **Polysynth Brass** (chords that swell and blare), **Anthem Lead** (bright, big, singalong), **Glass Bell Lead** (FM bells), **Fifths Lead** (two notes a fifth apart in every key), **Talking Lead** (a vowel-like band-pass), **Blip Lead** (tiny echoing blips), **Minor Arp** (a minor arpeggio that plays itself) |
| Pads and atmospheres | **Industrial Drone**, **Breathy Acid Pad** |
| Drums and grooves | **Industrial Drum Loop** (kick, crushed snare, hats), **Body Music Groove** (bass, kick and hats together) |
| Effects | **Computer Burble** (sample & hold on a resonant filter), **Laser Zap**, **Noise Riser** |

The ones with a Clock (Body Music Bass, Acid Squelch, Octave Pulse Bass, Pop Pluck Riff, Minor Arp, the drums and grooves, Computer Burble) play by themselves (Trance Gate Stab and Random Step Lead use the clock while you hold keys); on the sequenced basses, hold a key around C3 and the line plays from it, and on the riff and the arpeggio a key around middle C. A vibrato that fades in (Breathy Pipe Lead, Cold Wave Lead) is an LFO through a VCA that a slow envelope opens: worth taking apart. All are set to about the same loudness.

## Patches

**PATCHES** in the top bar: type a name and **SAVE** (the same name replaces it); click a name to load it (the rack is replaced: save first); **✕** deletes one. The name of the patch loaded or saved last shows at the top right. Patches are files in `~/.local/share/konnekt/patches`; a patch keeps its modules, where they are, every knob, the cables, the voices and its MIDI Learn links. **NEW** (press twice) empties the rack.

## MIDI and MIDI Learn

- Konnekt listens to every MIDI controller connected: notes with velocity, pitch bend, mod wheel (CC 1), sustain (CC 64), aftertouch; CC 123 stops every note.
- **LEARN** (top bar): every knob gets a ring; click one (it turns red), then move a knob or slider on your controller: linked. Its value then shows the CC. Right-click a knob's ring to forget its link. **LEARN** again (or Esc) to finish. The links are saved with the patch.

## Tips

- **No sound?** Is something plugged into the Output? Is the VCA's CV plugged in (or its LEVEL up)? Is the envelope's GATE plugged into MIDI In's GATE? Right-click outputs along the chain to see where the signal stops.
- **A note sounds forever:** an oscillator plugged straight into the Output never stops; put a VCA opened by an envelope in between.
- **Too loud or harsh:** turn the Output's VOLUME down; several voices add up.
- **Clicks at the start of notes:** a little ATTACK on the envelope.
- **Learn by taking apart:** load a lesson, then unplug one cable at a time and hear what each did.
- **What's this for?** Right-click it.
