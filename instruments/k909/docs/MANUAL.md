# K909 manual

K909 is a drum machine: fifteen synthesized drum voices (the classic eleven of an analog drum machine, plus four for industrial and EBM sounds: Metal, Clank, Noise and Boom), a sample voice for your own WAV files, a mixer with drive and effect sends for every voice, a gated reverb, a delay, a bus compressor and crusher, 16 kits, and a 16-step sequencer with accents, flams, step velocities and per-step tune and decay.

This manual goes through the panel from top to bottom. Every knob, button and display is described here: what it does, its range, and how to use it. Drag a knob up or down (Shift for fine moves) or use the mouse wheel; its value shows while you drag; double-click to put it back to its default.

## Quick start

1. **Play a beat.** Press **START** (or Space). Pattern **A** plays.
2. **Hear the industrial side.** Click **I–P** above the pattern buttons and choose **I**. Then step the **KIT** display to **2 Industrial** with its ▲.
3. **Program.** Click a drum name in the strip under the step buttons (right-click it to hear the drum), then click step buttons: off → on → accent → flam.
4. **Shape.** Turn the drum's knobs on the **SOUND** page. Press **MIX** for each drum's pan, drive and effect sends.
5. **Keep it.** Press **WRITE** next to KIT, choose a slot with ▲▼, press **WRITE** again.

Patterns save themselves as you edit; kits only change when you WRITE.

## How K909 makes sound

Every drum is synthesized when it plays: no samples. Each drum is a separate voice with its own knobs, and each has a place in the mix:

```
drum ─► DRIVE ─► LEVEL ─► PAN ─┬─────────────────────────────► mix ─► COMP ─► CRUSH ─► VOLUME
                               ├─ REVERB send ─► reverb (GATE) ─┘
                               └─ DELAY send ──► delay ─────────┘
```

A drum plays when its step comes round in the pattern, or when you hit its pad or key on your MIDI controller. Each drum plays one hit at a time: a new hit restarts it.

## Total accent

| Control | What it does |
| --- | --- |
| **TOTAL** | How much harder accented steps hit: up to +6 dB louder, and punchier (a shorter kick sweep, more snare noise). At 0, accents play at full velocity but no louder than that. |

## Bass drum

A sine wave that starts high and drops to its pitch, with a click on top.

| Control | What it does |
| --- | --- |
| **TUNE** | The pitch it settles on, 40–70 Hz. |
| **LEVEL** | Its volume. |
| **DECAY** | How long it booms, 80–900 ms. |
| **ATTACK** | The click at the front: 0 is a soft thump, up is a hard beater click that cuts through. |
| **SWEEP** | How far above its pitch it starts, from no drop at all (a pure sine thud) to four times the pitch (a laser-like zap). The default starts at 2.5 times. |
| **BEND** | How long the drop takes, from a quarter to four times the default (about 30 ms). Short: a tight punch. Long: a falling "boom". |
| **TONE** | The body's shape: round (a soft sine) at the left, squarer and harder at the right. Turn it up for a driving industrial kick. |

## Snare drum

Two tuned sines (the drum's body) and noise (the snare wires).

| Control | What it does |
| --- | --- |
| **TUNE** | The body's pitch, 150–260 Hz. |
| **LEVEL** | Its volume. |
| **TONE** | Dark and full at the left, bright and thin at the right (the noise's brightness and the body/noise balance). |
| **SNAPPY** | How much snare wire: more noise, ringing longer. |
| **DECAY** | How long the body rings: from a quarter to four times the default. |
| **CRACK** | A bright snap on the front, like a rimshot or a heavily compressed snare: 0 is off. Great for industrial and EBM snares. |

## Toms

Low, mid and hi tom: a sine with a small pitch drop and a touch of noise on the hit.

| Control | What it does |
| --- | --- |
| **TUNE** | Low tom 70–130 Hz, mid 110–190 Hz, hi 160–280 Hz. |
| **LEVEL** | Its volume. |
| **DECAY** | From a short tap (100 ms) to a long ring (900 ms). |

## Rim and clap

| Control | What it does |
| --- | --- |
| **TUNE** | Rim: its two resonances, an octave down or up. Clap: its noise band, an octave down or up. |
| **LEVEL** | Its volume. |
| **DECAY** | Rim: from a click to a ringing knock. Clap: the length of its tail. |
| **SPREAD** (clap) | The clap is three quick bursts and a tail, like several people clapping. Spread sets how far apart the bursts are: tight (one big clap) to loose (a sloppy crowd). |

## Hi hats and cymbals

Hats and cymbals are six detuned square waves (a metallic cluster) through filters.

| Control | What it does |
| --- | --- |
| **CH LEVEL**, **OH LEVEL** | The closed and open hat's volumes. |
| **CH DECAY**, **OH DECAY** | Closed hat 20–150 ms, open hat 100 ms–1 s. A closed hat cuts a ringing open hat short, as on a real hi hat. |
| **TUNE** (hi hat) | Both hats' pitch, darker to brighter: the metal and its filters move together. |
| **CRASH LVL**, **RIDE LVL** | The crash and ride's volumes. |
| **CR TUNE**, **RD TUNE** | The crash and ride's pitch. |

## The industrial voices

| Control | What it does |
| --- | --- |
| **METAL**: TUNE | The pitch of the struck metal, 120–1500 Hz. |
| **METAL**: DECAY | How long it rings, 0.15–3 s. |
| **METAL**: MATERIAL | What you hit: a pipe or bar at the left (a few far-apart, clangy tones: an anvil, a scaffold pipe), a plate or sheet at the right (many close tones: a steel sheet, a trash lid). |
| **CLANK**: TUNE | Its pitch, 80–900 Hz. |
| **CLANK**: DECAY | 50 ms (a short hammer hit) to 1.2 s (a ringing chain or spring). |
| **CLANK**: HARD | How hard and metallic the strike is: soft and bell-like at the left, harsh and clattering at the right. The brightness falls away quickly, like a hammer on steel. |
| **NOISE**: TONE | Where the noise sits, 300 Hz (a dull thud of air) to 8 kHz (a hiss). |
| **NOISE**: DECAY | 30 ms (a stab) to 1.5 s (a wash). |
| **NOISE**: SWEEP | How far the filter falls during the hit: 0 is a steady hiss, up is a zap or a falling "whoosh" (up to 4 octaves). |
| **BOOM**: TUNE | A sub-bass hit, 28–65 Hz, falling slowly from three times its pitch. |
| **BOOM**: DECAY | 0.3–3 s: from a deep thump to a long, rumbling impact for drops and breakdowns. |

Each also has a **LEVEL** knob. Play them from MIDI on notes 56 (Metal), 54 (Clank), 52 (Noise) and 34 (Boom).

## The sample voice

SAMPLE plays a WAV file of your own: a drum hit, a vocal shout, a hit on a metal bin you recorded, a short loop. It works like every other drum: its own track, accents, flams, step velocities and locks, and a MIX strip with drive and sends.

| Control | What it does |
| --- | --- |
| **LOAD** | Opens a file dialog: choose one or more WAV files (or drop them anywhere on the panel). They are copied into K909's sample library, so they stay available even if you move the originals. |
| **SAMPLE** display | The sample playing and its length; ▲▼ step through your library. |
| **TUNE** | ±24 semitones: lower for a bigger, darker hit, higher for a smaller one. Lock TUNE on steps (the step editor) to play a sample as a melody. |
| **LEVEL** | Its volume (every sample is normalized to the same peak, so they start at a similar level). |
| **DECAY** | **Full** at the top: the whole sample plays. Turn it down to fade it out sooner (4 s down to 50 ms): tighten a loose recording, or chop a long sound into a short stab. |
| **START** | Where in the sample it starts, 0–90 %: skip a soft attack, or pick a different part of a long sound. |

Play it from MIDI on notes 60 and 61. A kit you WRITE remembers its sample. Files: any WAV (stereo is mixed to mono); longer than 30 s is cut; non-WAV files are refused with a message in the display.

## The mix page

Press **MIX** (top right) to show a strip for every drum instead of its sound knobs; **SOUND** goes back.

| Control | What it does |
| --- | --- |
| **PAN** | Its place from left to right. The defaults spread the toms and lean the hats right and the ride left. |
| **DRIVE** | Distortion on that drum alone, from warm to wrecked. The peak stays the same; the body and the tail come up and get dirtier, so a driven kick sounds bigger and harder, a driven snare fatter. Drive on the kick, snare and hats is the core of an industrial kit. |
| **REVERB** | How much of the drum goes to the reverb. |
| **DELAY** | How much of the drum goes to the delay. |

## Effects

| Control | What it does |
| --- | --- |
| **REV SIZE** | The reverb's length: a small room to a big hall (0.4–8 s). With GATE on, how long the gate stays open after each hit (80–500 ms). |
| **GATE** | **On**: the reverb rings big and dense and is then cut off sharply: the famous gated snare of the 80s, and the huge chopped drums of industrial records. Send the snare (and toms) to the reverb and turn GATE on. |
| **DLY TIME** | The delay, in time with the tempo: **1/16**, **1/8T** (triplet), **1/8**, **1/8.** (dotted), **1/4**, **1/4.**, **1/2**. |
| **FEEDBACK** | How many echoes, 0–90 %; each one a little darker. The echoes bounce left and right. |
| **COMP** | A compressor on the whole kit. A little glues the drums together; a lot makes them loud, dense and pumping (the quiet parts between the hits swell up). 0 is off. |
| **CRUSH** | Lo-fi: fewer bits and a lower sample rate, for gritty, aliased, broken-machine sounds. 0 is off; a little adds grit, a lot destroys. |

The effects only run while some drum sends to them, so they cost nothing when unused.

## Kits

A kit keeps every sound and mix knob and the effects (not VOLUME, TEMPO or SHUFFLE). There are 16.

| Control | What it does |
| --- | --- |
| **KIT** display | The kit loaded, with a dot after its name if you've changed a knob since. **▲ ▼** load the next or previous kit at once. |
| **WRITE** | Store the panel into a kit: press WRITE (it blinks and the display says "WRITE to …"), choose the slot with ▲▼, press WRITE again. Esc cancels. |

**Factory kits**: **1 Classic** (the original sound), **2 Industrial** (driven kick and snare, gated reverb, metal and clank, compressed), **3 EBM** (tight, punchy and driving, noise stabs on the delay), **4 Gated 80s** (the big gated snare and toms), **5 Crushed** (driven drums through the crusher and a pumping compressor), **6 Machine Shop** (struck plates, hammered steel, hissing stabs, a long boom). Kits 7–16 start as Init for your own.

Kits and patterns are separate: any kit plays any pattern.

## Patterns

16 patterns in two pages: **A–H** (four classic grooves and four empty) and **I–P** (eight industrial grooves).

| Control | What it does |
| --- | --- |
| **A–H**, **I–P** (above the buttons) | Which eight patterns the buttons show. |
| **Pattern buttons** | Choose the pattern to play and edit. The dark button is the one shown; its light is the one playing. While playing, a new choice waits for the end of the bar (its light blinks), so it never cuts a bar short. |
| **PATTERN** display | The pattern shown; ▲▼ step through all 16. |
| **LENGTH** | 1–16 steps: shorter patterns make odd, looping rhythms. |
| **SCALE** | How long a step is: **1/16** (normal), **1/32** (double speed), **1/8 triplet**, **1/16 triplet**. |
| **ORDER** | **Forward**, **Reverse**, **Fwd+Rev** (back and forth), **Random**. |

**Factory patterns**: A Floor, B Broken, C Groove (for SHUFFLE), D Fill; I Stomp, J Drive (an EBM eighth-note kick), K Anvil, L Machine Gun, M Half-Time, N Clatter, O Gated Floor (for the Gated 80s kit), P Breakdown.

## Steps

| Control | What it does |
| --- | --- |
| **Track strip** (the drum names) | Click a name to choose the drum the step buttons edit (it lights up); right-click to hear it. ↑ ↓ choose too. |
| **Step buttons 1–16** | Click to cycle the step: off → on (orange) → accent (red) → flam (blue) → off. The lights above show the step playing. |
| **Flam** | Two hits: a soft grace note on the step, then the main hit just after (18 ms): the drummer's flam, great on snares and toms. |
| **Right-click a step** | The step editor for that step of the chosen drum. |
| **CLEAR** | Clears the chosen drum's steps; Shift-click clears the whole pattern. |

**The step editor** gives one step its own settings:

| Control | What it does |
| --- | --- |
| **VELOCITY** | How hard this step hits, 1–127 (otherwise steps play at the pattern's level and accents at full). Use it for ghost notes and dynamic rolls. |
| **TUNE** | This step's pitch, instead of the drum's TUNE knob: a melodic tom line, a boom that drops, metal hits that change note. |
| **DECAY** | This step's length, instead of the drum's DECAY knob: an open hat that's short on one step and long on another. Crash and ride have none. |
| **CLEAR** | Removes the step's own settings. |
| **CLOSE** | Closes the editor (Esc too). |

A knob in orange shows the drum's own value; move it and it turns blue: the step now has its own. Double-click it to go back to the drum's. A small dot on a step button marks a step with its own settings.

## Tempo, volume and shuffle

| Control | What it does |
| --- | --- |
| **TEMPO** | Standalone only: 40–240 BPM. In the Krate rack, the rack's transport bar sets the tempo. |
| **VOLUME** | K909's output level. |
| **SHUFFLE** | Swing: every second step comes a little late, from straight to fully swung (6 positions). |
| **START**, **STOP** | Run and stop (Space too). In the rack, the rack's transport runs K909. |

## MIDI and MIDI Learn

| Control | What it does |
| --- | --- |
| **LEARN** | MIDI Learn (also the L key; Esc leaves it): click a knob or button, then move a knob or press a button on your controller; they're linked. Right-click a control in Learn to remove its link. |
| **UNMAP** | Removes every MIDI link. |
| **AUDIO** | Lit when K909 is connected to the audio system; click to reconnect. The line under it shows the sample rate, buffer and dropouts, and the meter the output level. |

- **Notes** play the drums (General MIDI numbers): 35/36 bass drum, 37 rim, 38/40 snare, 39 clap, 41/43 low tom, 42/44 closed hat, 45/47 mid tom, 46 open hat, 48/50 hi tom, 49/57 crash, 51/59 ride, 56/67 metal, 54/69 clank, 52/70 noise, 33/34 boom, 60/61 sample. Velocity 100 and up plays accented.
- **Channel**: standalone, any channel; in the Krate rack, channel 10.
- **Learnable buttons**: the patterns (A–P) and START, so a controller can switch patterns while it plays.

## Sound design recipes

- **Industrial kick**: TUNE low, ATTACK up, SWEEP a little up, BEND short, TONE high; DRIVE 50–70 % on the MIX page; COMP up a bit.
- **EBM drive**: pattern J; a short kick (DECAY low), DLY TIME 1/8 with the noise sent to the delay; a little COMP.
- **Gated snare**: SNAPPY up, CRACK a little; REVERB send high on the snare (and toms), GATE on, REV SIZE to taste for the hold.
- **Anvil**: METAL with MATERIAL at the left, TUNE in the middle, DECAY medium; a touch of REVERB.
- **Trash-can snare**: METAL with MATERIAL at the right on the snare's steps, DRIVE up; or the snare with CRACK high and CRUSH a little.
- **Hammer on steel**: CLANK with HARD high and DECAY short; lock different TUNE values on a few steps for a melodic clatter.
- **Hiss stabs**: NOISE with SWEEP up, DECAY short, TONE high; DRIVE for grit; DELAY send for echoes.
- **Drop**: BOOM with DECAY long on the first step of the bar, DRIVE a little; the kick off on that step.
- **Your own industrial hit**: record a hit on a metal object or a door slam, LOAD it, TUNE it down 5–12 semitones, DRIVE 40 %, REVERB with GATE on.
- **Stab melody**: a short vocal or synth stab as the sample, DECAY around 300 ms, TUNE locked to different notes on a few steps.
- **Broken machine**: CRUSH 30–60 %, COMP high, DRIVE on everything.
- **Ghost notes**: snare steps at VELOCITY 30–50 between the backbeats.

## Troubleshooting

- **No sound when pressing START**: is the pattern empty (an E–H slot)? Is VOLUME down? Is AUDIO lit? In the rack, press the rack's PLAY.
- **A drum is silent**: its LEVEL (SOUND page), or a step's VELOCITY set very low. The SAMPLE voice is silent until you LOAD a sample.
- **A file won't load**: only WAV files are read (no MP3, FLAC or AIFF); convert it first.
- **The reverb or delay does nothing**: turn up that drum's REVERB or DELAY send on the MIX page.
- **The sound is distorted**: DRIVE, COMP or CRUSH are up (load the Classic kit to compare), or many loud drums hit together: turn VOLUME down.
- **The open hat stops short**: a closed hat on a later step cuts it, as on a real hi hat.
- **A pattern change doesn't happen at once**: while playing, it waits for the end of the bar.
- **The kit display has a dot**: you've changed a knob since loading it. WRITE to keep it, or load the kit again to go back.
