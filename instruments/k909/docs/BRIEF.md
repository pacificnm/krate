# K909: brief

2026-10-28

K909 is a drum machine for the [Krate](../../../README.md) suite. It has eleven synthesized drum voices, a tracks × steps sequencer and a hardware-style panel. It runs standalone (`k909`) and as a device in the Krate rack.

It follows the classic early-80s analog/hybrid drum machine layout. Every name, logo and graphic is our own: the panel reads "K909" and "Drum Machine". No third-party names, logos or artwork are used, and the reference screenshot used for layout stays out of the repository (`reference/` is git-ignored).

Related docs: [DSP.md](DSP.md) (the voices), [SEQUENCER.md](SEQUENCER.md), [UI.md](UI.md), [ROADMAP.md](ROADMAP.md). Suite design: [SHARED_FOUNDATIONS.MD](../../../docs/SHARED_FOUNDATIONS.MD).

## Scope (v1)

- **11 voices**: Bass Drum, Snare Drum, Low Tom, Mid Tom, Hi Tom, Rim Shot, Hand Clap, Closed Hat, Open Hat, Crash, Ride. All synthesized (no samples), so the repo carries no third-party audio.
- **Sequencer**: 8 patterns (A–H), 1–16 steps each, every voice has its own track. Each step is off, on or accented. Total Accent, Shuffle, and the shared play orders and scales from `StepClock`.
- **Panel**: one knob set per voice, as in the reference layout, plus Total Accent, Volume, Shuffle and (standalone) Tempo.
- **MIDI**: notes play the voices (General MIDI drum note numbers); MIDI Learn for every knob, pattern button and start/stop; in the rack, default channel 10.
- **Storage**: `~/.config/k909/panel.json` and `midimap.json`, `~/.local/share/k909/patterns.json`. Shared with the rack, as k303's are.

Not in v1 (see [ROADMAP.md](ROADMAP.md#later)): kits (saved knob sets), flam, per-step parameter locks, per-voice outputs, song mode.

## Parameters

All parameters are normalized 0..1 in the `IInstrument` descriptor table; the table below gives what they mean. Keys are stable: they are used by `panel.json`, `midimap.json` and rack sessions.

| Key | Label | Meaning |
| --- | --- | --- |
| `accent` | Total Accent | How much louder (up to +6 dB) and punchier accented steps are |
| `bd_tune`, `bd_level`, `bd_attack`, `bd_decay` | Bass Drum | Pitch, level, click amount, body decay |
| `sd_tune`, `sd_level`, `sd_tone`, `sd_snappy` | Snare Drum | Pitch, level, noise brightness, noise (snare wire) amount |
| `lt_tune`, `lt_level`, `lt_decay` | Low Tom | Pitch, level, decay |
| `mt_tune`, `mt_level`, `mt_decay` | Mid Tom | Pitch, level, decay |
| `ht_tune`, `ht_level`, `ht_decay` | Hi Tom | Pitch, level, decay |
| `rs_level` | Rim | Level |
| `cp_level` | Clap | Level |
| `ch_level`, `ch_decay` | Closed Hat | Level, decay |
| `oh_level`, `oh_decay` | Open Hat | Level, decay |
| `cr_level`, `cr_tune` | Crash | Level, pitch |
| `rd_level`, `rd_tune` | Ride | Level, pitch |
| `volume` | Volume | Output level |
| `shuffle` | Shuffle | Delays every second step (0 = straight, 6 positions) |
| `tempo` | Tempo | 40–240 BPM; drives the transport standalone, stored but unused in the rack (as k303) |

31 parameters in all; level knobs use gain = v², as Klick and k303 do.

## MIDI notes

| Voice | Notes (any channel standalone; the device's channel in the rack) |
| --- | --- |
| Bass Drum | 35, 36 |
| Rim Shot | 37 |
| Snare Drum | 38, 40 |
| Hand Clap | 39 |
| Low Tom | 41, 43 |
| Closed Hat | 42, 44 |
| Mid Tom | 45, 47 |
| Open Hat | 46 |
| Hi Tom | 48, 50 |
| Crash | 49, 57 |
| Ride | 51, 59 |

Velocity 100 and above plays the hit accented. Played notes do not record into the pattern in v1.

## Registry entry

| Field | Value |
| --- | --- |
| id / name | `k909` / `K909` |
| default MIDI channel | 9 (channel 10, the usual drum channel) |
| QML module / panel | `K909` / `Panel` |
| actions | `patternA` … `patternH`, `startstop` |

## Outputs

Stereo. Each voice has a fixed pan position (toms spread left to right, hats slightly right, ride slightly left), not exposed as a parameter in v1. Per-voice outputs are possible later through `outputChannels()`.
