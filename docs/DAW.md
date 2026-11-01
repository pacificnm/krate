# Krate as a DAW: roadmap

Krate grows into a studio of its own: the rack of instruments it already is, plus recording, an arranger, editing, automation and export, instead of handing the instruments to another DAW as plugins. The instruments stay built in (their panels as they are), and the rack can later host CLAP plugins alongside them.

What Krate already has to build on: one transport (with JACK transport sync to other apps), the rack running every instrument with its panel, a mixer with EQ and sends, an 8-track pattern sequencer with a piano roll, MIDI and Guitar Pro song import, audio inputs (Krunch), sessions, rack-wide MIDI Learn, an offline renderer.

Each milestone ends with something you can use, and is committed and pushed when done.

| # | Milestone | Done when |
| --- | --- | --- |
| D1 ✅ | **Add and remove instruments** | The rack holds only the instruments you add (**+** to add and remove them), one of each; an instrument not in the rack has no tab, no mixer strip, no MIDI and costs no CPU, and is left out of the sequencer's and the song player's choices; the set is saved with the session. Every instrument is still built at startup (their panels are one per app), so adding and removing is instant; plugins (D7) come and go for real. Result: **+** after the tabs lists the instruments, each with ADD / REMOVE (the backing track always stays). In the rack, a device has a `present` flag (any thread): one not present isn't processed (its JACK outputs are fed silence), takes no MIDI (its MIDI channel falls back to the focused instrument), no focus (stepping, Program Change, learned focus buttons pass it by), no solo, feeds nothing (Krunch fed by it goes back to the interface's input), and its learned buttons don't fire; back in the rack, it's prepared afresh (no hanging notes). Its tab, panel (not even made) and mixer strip are hidden; the sequencer's and the song player's instrument menus leave it out, showing a track's choice "(not in rack)" if it's there already, while their lists stay whole so saved songs and sequences keep their choices; the song player doesn't suggest it. The session keeps the list (`"absent"`; older sessions: every instrument). Tests: tst_rack (skipped and silent, outputs cleared, no focus / solo / feed, reset on return), tst_racklearn (remove and add, focus moving, the backing track staying, the session), tst_song (suggestions only from the rack). |
| D2 | **Audio tracks and recording** | Audio tracks that record from an input (a guitar through Krunch, a microphone) or from any instrument's output to WAV files on disk, without dropouts (a disk thread fed from the audio thread through a lock-free buffer), and play back in time with the rack, the latency of the interface compensated. |
| D3 | **The arranger** | A song timeline: per track, patterns, MIDI clips and audio clips laid out across bars, instead of one pattern looping; loop and punch ranges. |
| D4 | **Clip editing** | Move, trim, split, duplicate and fade clips; undo and redo for every edit. |
| D5 | **Automation** | Knob movements recorded while playing, and drawn as lanes per track. |
| D6 | **Bounce and export** | The whole song (or a range, or each track) rendered to WAV, faster than real time. |
| D7 | **CLAP plugins in the rack** | Third-party CLAP instruments and effects added to the rack like the built-in ones, their windows opened from their tabs, their state saved with the session. |

Decisions:

- **Grow Krate rather than build plugins for another DAW, or fork one**: everything stays the way we want it, and the instruments' panels need no work (as plugins they would have to run as separate programs next to the host).
- **One of each instrument** for now: each instrument's controllers exist once per app. Copies (two Kobalts) can come later as their own project.
