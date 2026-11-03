# Krunch: amp heads

An **amp head** is Krunch's own tube amp built like a particular amp: how many preamp stages, how each one is biased and filtered, the tone stack, how the power tubes break up and sag, the output transformer's response. The four voices (Clean, Crunch, Heavy, Lead) are the built-in heads. Others are files, made by **fitting** Krunch's tubes to an amp capture (`.nam`, [CAPTURES.md](CAPTURES.md)) with `krunch-fit`.

A head is not a capture. A capture is a recording of what one amp did at one setting: its knobs can only push it harder or EQ it after. A head *is* an amp: GAIN, BASS, MID, TREBLE, PRESENCE, MASTER, SAG and RESONANCE work as an amp's do, around the sound it was fitted to (all at noon: the capture's sound), and it costs a fraction of a capture's processing.

## Making heads

You need, once, **dry recordings of your guitar** (one take per guitar, and per pickup if it has more than one), and **amp captures** of the amps you want heads of (amp only, no cabinet; 48 kHz; standard size).

1. **Record the dry takes** on the player: an audio track with SOURCE on the guitar's input, CHANNELS MONO, ARM, REC, PLAY. No amp involved: the track records the interface's input as it is. Play, letting each ring: each open string low E to high E, softly and then hard; a few notes high on the neck (12th fret, G and B strings); an open E chord and a power chord; some palm-muted chugs on the low E; a second or two of silence at the start. About a minute. Keep the interface's gain the same for every guitar (their levels are part of how they drive an amp).
2. **Copy the takes and captures to your computer** (Krate Remote: Files ▸ Places ▸ Recordings, DOWNLOAD).
3. **Fit**, in a terminal:

   ```
   krunch-fit --di strat-neck.wav --di humbucker-bridge.wav --ir my-cab.wav --out heads/ crunch-amp.nam high-gain-amp.nam
   ```

   Every take goes into every fit (one head per amp that suits all your guitars, as a real amp does). `--ir` is only for the files to listen to. Each capture takes a few minutes (`--evals` sets how long the search runs: 1500 by default; more is closer and slower).
4. **Listen and read the report**: for each capture, `heads/<name>.json` (the head), `<name>.txt` (how close it got, and how close the nearest built-in voice was), and `<name>-capture.wav`, `<name>-head.wav`, `<name>-ab.wav` (your playing through both; the A/B file switches every 3 seconds, capture first).
5. **Put the heads on the player**: Krate Remote, Files ▸ Places ▸ **Amp heads (Krunch)**, upload the `.json` files (or IMPORT them in Krunch's AMP list). They show up by themselves.
6. **Play one**: on a channel, **AMP** ▸ the head. VOICE and PREAMP TUBES dim (the head has its own), so does the power amp's TUBES when the head has its own power tubes; every other knob works on the head.

A head's name is the capture's file name; rename the `.json` (or its `"name"`) as you like. The fit keeps where it came from in the file (the capture, the takes, the scores).

## How the fit works

`krunch-fit` plays the same material through the capture and through Krunch, both with every knob at noon and the cabinet off, and compares them:

- **Material**: windows from the dry takes, chosen evenly from the softest playing to the hardest (each with a lead-in that's played but not compared, so the amps settle), plus steady tones at 98, 220 and 660 Hz at three levels up to the takes' peak.
- **Comparison**, average dB apart:
  - **tone**: the spectrum in sixth-octave bands, 85 ms frames;
  - **attack**: half-octave bands, 2.7 ms frames (picking, transients);
  - **envelope**: the level every 10 ms (compression, sag, decay);
  - **harmonics**: for the tones, the 2nd to 10th harmonics against the fundamental (how it distorts, as you play harder).

  One overall level difference is taken out first (it becomes the head's output level, so the head is as loud as the capture). Score = tone + ½ attack + ½ envelope + ½ harmonics.
- **Where it starts**: every built-in voice with every preamp tube and every power tube (64 heads) is scored; the closest few (one per voice and preamp tube; `--starts`, 3 by default) are searched from. A search keeps its start's number of stages, its tubes and its tone stack.
- **The search**: separable CMA-ES over about 35 of the head's insides, each mapped to 0..1 (log scales for ohms and hertz): each stage's cathode resistor, cathode bypass, coupling capacitor and treble roll-off, the dividers into the third and fourth stages, the Gain pot's range, the loss into the second stage, the top cut, the preamp's level into the power amp; the power tubes' headroom, knee, bias, sag and voicing, how hard Master's noon drives them; after them, a low cut, a high cut and a peak (the output transformer and the speaker's pull). Each generation is played in parallel, one head per core.
- A capture that's nearly silent with the takes (below −60 dBFS) is skipped: not an amp.

Checked: a built-in voice reshaped at random (an amp none of the built-ins is, 5.8 apart from the nearest) is fitted back to 0.23 apart (0.07 dB tone, 0.04 dB envelope) in 3000 tries (`krunch-fit --target-voice heavy`, the self-test).

## The head file

JSON (`HeadFile`, `app/HeadFile.cpp`): `name`; `fit` (capture, takes, start, score, built_in_score, date); `preamp` (`stages`: each `tube` — `high_gain`, `smooth`, `medium`, `low_gain` — `cathode_ohms`, `bypass_hz`, `coupling_hz`, `rolloff_hz`, and `into` for the third and fourth; `gain_pot_min_db`, `pad_into_second`, `tone_stack` r1–r4 c1–c3, `top_cut_hz`, `trim_db`); `power` (`headroom`, `knee`, `bias`, `sag`, `voice_hz`, `voice_db`, `voice_q`, `master_offset_db`, `low_cut_hz`, `high_cut_hz`, `peak_hz`, `peak_db`, `peak_q`, `output_db`). Every value is brought into a safe range when read, so an edited file can't make the amp unstable.

## In Krunch

- `dsp/Head.h`: `HeadDesign`; the four voices are `builtInHead()` (the same numbers as before: renders of every voice, tube and power tube came out byte for byte the same after the change). `Preamp` and `PowerAmp` are built from a head; the engine takes one per channel (`Krunch::setHead`, by pointer, as captures; freed two blocks later), the power amp the playing channel's.
- `app/HeadController` ("Heads" in QML): the heads folder (`~/.local/share/krunch/heads`, watched), each channel's head (`~/.config/krunch/heads.json`, presets and songs' sounds, by file name), IMPORT.
- The AMP list (`qml/CapturePicker.qml`): Krunch's tubes as the VOICE, the heads, the captures; a channel plays one of them.

Tests: `tst_krunch` headsBuildTheChannel (a voice made explicit plays exactly as the voice; another head plays otherwise, on its channel only; the file round trip; wild values brought in), `tst_headfit` (the same amp scores nothing apart, the level isn't the sound; the space's round trip; the search finds a known minimum; a fit closes most of the gap to a known amp).
