# Performance recall, visible Master and Phaser — 30 October 2026

Based on f274a94274684e07280bbb5d7d49df2921a104a9, the exact source
identified in the user's latest tested update.

## Result

Performance transitions now use the same bounded outgoing tails as single
sounds: up to two seconds, maximum two retiring graphs, maximum eight rendered
engines across active and outgoing graphs. Oldest tails are retired first on
rapid changes. No ASIO stream restart is introduced.

A warm cached synth which was no longer continuously active is silenced and
its DSP history reset on the audio owner thread before reuse. The latest patch
is reapplied. Shared continuous layers retain their notes but are masked out
of the outgoing graph, avoiding double rendering and duplicate MIDI events.
Routing/transpose changes still clear the affected engine. Note releases reach
outgoing graphs using their original routing. Recalling a cached engine that
is still a tail ends that particular old tail before the clean activation;
this bounded-memory approach does not promise unlimited sustained generations.

Master is now a separate persistent global control in the Vision status bar
and classic main window, outside the parameter panel. It supports mouse and
MIDI Learn, retains the existing stored Master/CC fields, and synchronizes via
full state and telemetry. Left/right adjust Master by 2% including Home;
text fields, range controls and dropdowns keep their own arrow behavior.
Home up/down now visit categories sequentially so every category remains
reachable without using the Master arrows. Tab also remains available.

Parameter 15 is now a real stereo six-stage all-pass Phaser in the shared
SFZ/SF2 insert processor. Its amount is smoothed; the 0.35 Hz sweep coefficients
are updated every 32 samples, with no allocation in process. Phaser defaults
to off and is stored/mapped independently per preset and Performance layer.

Profiles write version 10 and read versions 1–10; prior versions initialize
Phaser to zero and its CC to unassigned. The original snapshot embedded in old
Performance drafts is normalized when loaded, preserving guarded Save Changes.
Earlier app versions do not read the new profile format; retain backups when
reverting to an earlier release.

## Local validation

- Debug C++ build with assertions active and 23 CTest groups, including a new
  cache lifecycle regression for immediate and expired-tail A → B → A recall,
  outgoing four-layer audio, release routing and unchanged driver start count.
- Existing asynchronous selection, concurrency, MIDI, DSP, storage, calibration,
  catalog and profile regressions.
- Phaser signal modification, stereo sweep, finite bounded samples, reset,
  block-size independence and sample-rate tests.
- Profile migration, Master/Phaser persistence and per-layer effect values.
- JSDOM tests: new Master/Phaser contract, existing stability controls and
  Performance editor tools. JavaScript syntax and whitespace checks.

The cache regression uses the deterministic SFZ test renderer to exercise the
shared warm-engine lifecycle; it is not a real FluidSynth/audio-hardware test.
No new real SFZ/SF2 audio rendering or Windows/ASIO validation is claimed.
Windows cross compilation was unavailable in this environment. Browser layout
screenshots could not be generated because Chromium installation failed; DOM
checks do not establish pixel-layout correctness. Visual and hardware checking
remain required before an installable release.

No new portable/ZIP, Drive upload, hosted workflow or binary upload is requested
or produced in this stage. Authenticated GitHub allowance remains unknown;
the available connector exposes no balance tool and repository calls failed.
Source is preserved as one grouped Git commit via Git.
