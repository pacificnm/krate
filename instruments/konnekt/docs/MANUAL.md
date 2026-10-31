# Konnekt manual

Konnekt is a modular synthesizer: instead of a fixed synth with its parts already joined, you get the parts, oscillators, filters, envelopes, LFOs, VCAs, mixers, effects, and join them yourself with patch cables. Every synth in Krate is made of these same parts; here you can see each one, hear what it does, and wire them however you like. It plays from your MIDI keyboard, up to 8 notes at once.

## Quick start

1. **Play.** Konnekt opens with a small patch: MIDI In → Oscillator → VCA (opened by an Envelope) → Output. Play your keyboard.
2. **Point at things, right-click them.** Hover over any module, knob or jack: the bar at the bottom says what it is in a line. Right-click it for the whole story: what it is, how it works, why you'd want it, where it goes, what to try (see [Explanations](#explanations)).
3. **Do the lessons.** **LESSONS** loads patches that build a synth up a module at a time, each with what to listen for and what to try. Start with lesson 1.
4. **Add a module.** **+ ADD MODULE**, pick one: it appears in view. Drag it by its header.
5. **Wire.** Click a jack, then another: an output and an input. The cable follows the modules when you drag them.
6. **Keep it.** **PATCHES**, type a name, **SAVE**.

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
8. **A sequenced bass part**: a clock, a sequencer, glide.
9. **Noise and percussion**: drums with no oscillator.
10. **Effects**: chorus, delay and reverb in stereo.

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
