# k303 manual

k303 is an acid bass synthesizer: one voice (a saw or square oscillator into a resonant diode-ladder filter), an envelope that sweeps the filter, accent and slide, and a 16-step pattern sequencer to program the squelchy, gliding bass lines of acid house and techno. After the voice, an overdrive and a delay and reverb make it as big or as dirty as you like.

This manual goes through the panel from the top left to the bottom right. Every knob, button and display is described here: what it does, its range, and how to use it. Drag a knob up or down (Shift for fine moves) or use the mouse wheel; its value shows while you drag; double-click to put it back to its default.

## Quick start

1. **Play.** Press **RUN / STOP** (or Space): pattern 1 plays.
2. **Turn the classic knobs.** **CUT OFF FREQ** and **RESONANCE** first, then **ENV MOD** and **DECAY**: that's the acid sound. Turn them while it plays.
3. **Make it bigger.** Turn **DRIVE** up a little for grit, and **DELAY** to about a third for echoes (TIME at 1/8. is the classic dotted echo).
4. **Write your own line.** Stop, click a step button (1–16), click a note key, then **ACCENT** or **SLIDE** for that step. Run it again.
5. **Something new.** **RANDOMIZE** writes a random line into the pattern; keep pressing until one grabs you.

Everything is kept when you quit: the knobs, the 64 patterns and your MIDI mappings.

## How k303 makes sound

```
OSC (saw / square) ─► FILTER ─► AMP ─► DRIVE ─► TONE ─► DELAY / REVERB ─► VOLUME
                       ▲          ▲
          CUT OFF + ENV MOD × envelope + ACCENT sweep     accent: louder
```

- The **oscillator** makes a bright, buzzy tone.
- The **filter** (a resonant diode ladder) cuts the highs and, with resonance, adds the singing, squelching peak.
- An **envelope** sweeps the filter open at every new note and lets it close over the DECAY time: that's the "wow".
- **Accent** makes a note louder, opens the filter harder and drives it harder; **slide** glides into the next note without restarting the envelope.
- After the voice: **drive**, **tone**, **delay** and **reverb**, then **volume**.

The bass itself always stays mono in the centre; the delay and reverb can spread around it.

## Waveform

| Control | What it does |
| --- | --- |
| **WAVEFORM** | **Saw** (left): bright, buzzy, the classic acid sound. **Square** (right): hollower and more woody. Both are shaped like the analog original: the saw slightly bent, the square made from the saw, a little uneven. |

## The sound knobs

| Control | What it does |
| --- | --- |
| **TUNING** | The pitch, −12 to +12 semitones. Tune k303 to your track. |
| **CUT OFF FREQ** | The filter's cutoff, about 60 Hz to 5 kHz: dark and muffled at the left, bright at the right. The envelope and accents move it up from here. |
| **RESONANCE** | The peak at the cutoff: from smooth (left) to a squelchy, vocal, singing peak (right). Loud resonance growls and compresses, as on the analog original, and the bass keeps most of its weight. Turn the cutoff and resonance together for the classic sweeps. |
| **ENV MOD** | How far each note's envelope opens the filter, 0 to 4 octaves. Low: a dull, round bass. High: a sharp "wow" on every note. |
| **DECAY** | How long the envelope takes to close the filter again, 0.2 to 2 s. Short: plucky, percussive. Long: notes stay open and squelchy. Accented notes always use the shortest decay. |
| **ACCENT** | How much accented steps stand out: louder (up to +6 dB), a harder filter push, more grit in the filter. At 0, accents do nothing. |
| **SWEEP** | How far accents push the filter open, 0 to 4 octaves at full ACCENT (more with resonance). Several accents in a row stack up and push the filter higher and higher: that's what makes accented runs scream. Turn it up for wilder accents, down for subtler ones. |
| **DRIVE** | Overdrive after the filter, from 0 (clean) to heavily distorted: warmth and density at a little, the dirty, distorted acid sound at a lot. The peaks stay where they are while the sound gets thicker (and up to about 3 dB louder). |
| **TONE** | The overdrive's tone: dark at the left (800 Hz), open at the right (16 kHz). It only does something with DRIVE up. Turn it down to tame a harsh drive. |

## Effects

| Control | What it does |
| --- | --- |
| **DELAY** | Echo level (0 = off). The echoes bounce left and right. |
| **TIME** | The echo time, in time with the tempo: **1/16**, **1/8T** (triplet), **1/8**, **1/8.** (dotted eighth, the classic), **1/4**, **1/4.**, **1/2**. |
| **FEEDBACK** | How many echoes, 0–90 %; each one a little darker. |
| **REVERB** | Reverb level (0 = off). It hears the bass and its echoes. |
| **SIZE** | The reverb's length, 0.4 s (a small room) to 8 s (a huge hall). |
| **WIDTH** | How wide the echoes and the reverb spread: 0 = mono, the middle = as they come, right = extra wide. The bass itself always stays in the centre. |

Keep DELAY and REVERB low for a bass that sits in a mix; turn them up for a lead line or a breakdown.

## Volume and level

| Control | What it does |
| --- | --- |
| **VOLUME** | k303's output level. |
| **Red bar** (under the displays) | The output level meter. |

## Patterns and timing

| Control | What it does |
| --- | --- |
| **A–H** | Banks of 8 patterns: 64 in all. Press a bank (it blinks), then a number: that pattern plays. Pressing the bank again cancels. While playing, a new pattern starts as one change, so you can line up the next one on the beat. |
| **1–8** | Choose a pattern in the current bank (or in the blinking one). Keys 1–8 on the computer do the same. |
| **PATTERN** display | The pattern shown, **A1** to **H8**; ▲▼ step through all 64 in order. |
| **Beat light** (by the status line) | Flashes on each beat. |
| **LENGTH** display | How many steps the pattern has, 1–16 (▲▼). Short patterns make odd loops. |
| **TEMPO** | Standalone only: 40–240 BPM. In the Krate rack, the rack's transport sets the tempo. |
| **SCALE** | How long a step is: **1/16** (normal), **1/32** (double speed), **1/16T** (triplets). |
| **PLAY MODE** | The step order: **FORWARD**, **REVERSE**, **FWD+REV** (back and forth), **INVERT** (forward, every note mirrored around C: an instant variation), **RANDOM**. |
| **RUN / STOP** | Start and stop (Space too). |

## Writing a pattern

The lower deck edits the pattern step by step.

| Control | What it does |
| --- | --- |
| **Step buttons 1–16** | Click to choose the step to edit. Each shows its note (↑ / ↓ for octave up or down, – for a tie, blank for a rest); the light above shows the step playing. Left / Right arrows choose too. |
| **Note keys** (C … C) | Set the chosen step's note. The lit key is its note. |
| **ACCENT** | Turn accent on or off for the step (A). |
| **SLIDE** | Glide from this step into the next one (S): the pitch slides, the envelope does not restart. Slides into a rest do nothing. |
| **DOWN**, **UP** | Move the step's note an octave down or up (−1 … +1). The lights under OCTAVE show where it is. |
| **TIME** | Time mode: the buttons then set the step's length instead: **DOWN** = Note, **UP** = Tie (the step continues the note before it), **ACCENT** = Rest (silence). The labels above the buttons show this. Keys: N, T, R. |
| **KEYBOARD** | On: the note keys just play the sound (to try notes) instead of editing the step. |
| **RANDOMIZE** | Replaces the pattern with a random line in C minor pentatonic: the first step always a note, as many playing steps as DENSITY says (the rest are rests, and now and then a step ties onto the one before), about a quarter of the notes accented and a fifth slid. The length is kept. |
| **DENSITY** | For RANDOMIZE: the share of steps that play, 0–100 % (85 % by default). Low makes sparse, syncopated lines with lots of space; high, busy rolling ones. MIDI-learnable, so you can turn it from your controller between presses. |
| **CLEAR** | Clears the pattern. |

**Ties and slides** are what make acid lines flow: a slide glides into the next note, a tie holds a note across steps, and accents on a few notes make them jump out. Try: a run of sixteenth notes, a slide on every third, accents on the off-beats.

## MIDI and MIDI Learn

| Control | What it does |
| --- | --- |
| **MIDI LEARN** | Learn mode (also the L key; Esc leaves it): click a knob or button, then move a knob or press a button on your MIDI controller; they're linked. Right-click a control in Learn to remove its link. |
| **UNMAP** | Removes every MIDI link. |
| **AUDIO** | Lit when k303 is connected to the audio system; click to reconnect. |
| **HELP** | Opens this manual (with a search). |

- **Keys** play k303 when the sequencer is stopped: holding one key and playing another slides to it; velocity 100 and up plays accented.
- Every knob (including the effects), the waveform switch, RUN / STOP and the pattern buttons can be learned.
- In the Krate rack, k303 listens on its MIDI channel and runs with the rack's transport.

## Sound recipes

- **Classic squelch**: Saw; CUT OFF low (about a quarter); RESONANCE high (70–80 %); ENV MOD half; DECAY short; accents on a few steps, slides here and there. Sweep CUT OFF slowly while it plays.
- **Screaming acid**: RESONANCE and ACCENT high, SWEEP up, accents on consecutive steps; DRIVE 40–60 %, TONE in the middle; DELAY 30 % at 1/8.
- **Deep, round bass**: Square; CUT OFF low; RESONANCE low; ENV MOD low; DECAY long; no effects. Sits under a kick.
- **Dirty techno line**: Saw; DRIVE high, TONE low (dark distortion); RESONANCE medium; short DECAY.
- **Dub echo**: DELAY 40–50 % at 1/4. or 1/8., FEEDBACK 60 %, REVERB a little, WIDTH up; a sparse pattern with long rests.
- **Hypnotic loop**: LENGTH 7 or 13 against the drums' 16, PLAY MODE Forward; turn CUT OFF and ENV MOD slowly.

## Troubleshooting

- **No sound**: is it running (RUN / STOP lit)? Is VOLUME down, or CUT OFF all the way left with ENV MOD at 0? Is AUDIO lit? In the rack, press the rack's PLAY.
- **The pattern is silent on some steps**: they are rests (blank step buttons) or ties after a rest.
- **Notes don't glide**: a slide only glides into a Note or Tie on the next step.
- **Accents sound the same**: turn ACCENT (and SWEEP) up.
- **Too much low end in a mix**: lower RESONANCE a little or raise CUT OFF; k303 keeps its weight at high resonance, so you may want less than before.
- **Distorted when you didn't want it**: DRIVE is up, or RESONANCE and ACCENT are very high (the filter itself growls). Turn VOLUME down if the whole mix clips.
- **Keys don't play**: they only play the sound while the sequencer is stopped (or in KEYBOARD mode for the panel's keys).
