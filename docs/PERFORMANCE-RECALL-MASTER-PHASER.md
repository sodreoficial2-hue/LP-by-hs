# Performance recall, visible Master and Phaser — 30 October 2026

Based on f274a94274684e07280bbb5d7d49df2921a104a9, the exact source
identified in the user's latest tested update.

## Result

Performance transitions now use the same bounded outgoing tails as single
sounds: up to two seconds, maximum two retiring graphs, maximum eight rendered
engines across active and outgoing graphs. When capacity or tail slots are
full, the oldest necessary tail receives a 20 ms release before the queued
replacement activates. A newer queued selection supersedes the previous one.
No ASIO stream restart is introduced. This can defer a rapid selection by the
short release plus a callback block; it does not enlarge the ASIO buffer.

A warm cached synth which was no longer continuously active is silenced and
its DSP history reset on the audio owner thread before reuse. The latest patch
is reapplied. Shared continuous layers retain their notes but are masked out
of the outgoing graph, avoiding double rendering and duplicate MIDI events.
Routing/transpose changes still clear the affected engine. Note releases reach
outgoing graphs using their original routing. Recalling a cached engine that
is still a tail now waits for that tail's short fade before cleaning it.
This bounded-memory approach does not promise unlimited sustained generations.
The production overlap mixer applies normalized graph weights with a 20 ms
ramp when the new graph becomes audible. It prevents overlap alone from
exceeding the louder individual graph; it is not a Master compressor or a
limiter for a preset which already exceeds full scale. Silent outgoing blocks
add no normalization weight. Diagnostics count all rendered active/tail voices.

Master is now a separate persistent global control in the Vision status bar
and classic main window, outside the parameter panel. It supports mouse and
MIDI Learn over its entire visible area, retains the existing stored
Master/CC fields, and synchronizes via full state and telemetry. Focus alone
no longer blocks incoming MIDI updates; only an actual mouse drag defers the
range position update. The screen displays the learning state. Absolute CC
knobs/faders on all sixteen channels can bind, excluding sustain CC64 and
channel-mode CC120–127. Notes, pitch wheel and Program Change cannot bind.
The first CC movement also adjusts Master, and Ctrl+S preserves the mapping.
Master consumption is exclusive even when a recalled layer has a conflicting
legacy CC mapping; native CC1 forwarding remains unchanged. Left/right adjust Master by 2% including Home;
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

- Debug C++ build with assertions active and 25 CTest groups, including a new
  cache lifecycle regression for immediate and expired-tail A → B → A recall,
  outgoing four-layer audio, release routing and unchanged driver start count.
- Existing asynchronous selection, concurrency, MIDI, DSP, storage, calibration,
  catalog and profile regressions.
- Phaser signal modification, stereo sweep, finite bounded samples, reset,
  block-size independence and sample-rate tests.
- Profile migration, Master/Phaser persistence and per-layer effect values.
- Exhaustive Master Learn checks for all CC numbers on all sixteen channels,
  reserved-message rejection, duplicate mapping cleanup and serialized recall.
- Production transition tests: rapid A → B → C, bounded engine count, overlap
  peaks, deferred Program Change notes, latest queued choice, immediate cached
  A → B → A recall with continuous short fades and no resurrected notes.
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
the available connector exposes no balance tool. The correct connected account
is sodreoficial2-hue; repository API access has been verified. Source stages
are published as grouped inline-tree commits without hosted builds.
