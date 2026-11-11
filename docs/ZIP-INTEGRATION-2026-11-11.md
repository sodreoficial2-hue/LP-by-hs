# Selective integration of the 10 November source ZIP

Reviewed `LP-by-hs-atualizacao-2026-11-10.zip` against main commit
`292d59bd36643065d6ededfef476837e75481baa`. The archive was not copied over the
project. The live-volume fix and recoverable NPNO transactions remain intact.

## Decisions

| ZIP proposal | Decision and evidence |
| --- | --- |
| Resolve relative paths against the application folder | Integrated with changes: normalize SFZ/SF2 references once on the owner thread before sync/async preparation. Preserve SF2 bank/program, calibration/cache identity, lexical path normalization and invalidation. Worker threads do not read mutable base configuration. setBase is permitted only before loading/starting audio. |
| Save portable paths for Meus Sons | Integrated through shared, locally tested catalog helpers. Save internal references relative to the app, external references absolute; expand internal paths against the app when reading. Preserve LPHS_MY_SOUNDS 1 and legacy absolute records. Existing catalogs gain relative paths on their next normal save. Use the existing flushed atomic writer instead of a fixed temporary filename. |
| Give imported sounds a path/name-derived bank ID | Not integrated: assigning bank:// IDs changes the instrument identity used by existing favorites, catalog edits and permanent controls; the proposed ID also depends on a display name and on relative paths for external files. A durable-ID migration/alias policy is required first. Current path identities are retained; portable parameter keys continue to work. |
| Defer sfizz envelope readback to parameters() | Rejected after testing: the ZIP implementation performs 19 heap allocations when enabling ADSR/glide after a normal load. parameters() is also called by AudioHost::render, so the change moves preparation into the audio callback. Current eager preparation passes the allocation gate. |
| Soft clipping in each instrument DSP | Rejected after testing: neutral input 0.9 becomes 0.881093; it changes peaks even with neutral controls. The logarithmic curve eventually clamps at 1, and it runs before calibration/layer/master summation, so it is not a master-output protection. No new nonlinear processing was integrated. |
| Drain MIDI queues without an iteration budget | Not integrated: the existing ring holds at most 1023 pending events, already covered by the 1024-iteration panic drain. A new capacity regression verifies this. An unbounded drain can be prolonged by a concurrent MIDI producer and is unsuitable for the real-time callback. |
| Root licensing declarations and CI reconciliation text | Not integrated: these are policy/licensing changes rather than demonstrated runtime improvements. No workflow changes or hosted compute were needed. |

## Tests

New `sound_paths_and_portable_catalog` regression exercises an unrelated CWD,
relative aliases, absolute references, external files, SF2 preset preservation,
legacy catalog loading, moving the entire app folder, portable parameter identity,
synchronous/asynchronous single and layered loads, calibrated gains, cache reuse
and normalized cache invalidation. Cache reuse is verified by temporarily removing
the source after loading, then recalling the same instrument via a relative alias.

New `neutral_dsp_preserves_headroom` and
`live_envelope_updates_no_allocations` gates were compiled against the exact DSP
and sfizz sources from the ZIP (outside the production checkout). Both rejected
that candidate:

```
FAIL: neutral DSP altered sample 0.9 into 0.881093
FAIL: 19 allocations during live parameter updates
```

The integrated version passes both. Existing gain-recall, transition, NPNO integrity,
MIDI, persistent-parameter and migration regressions continue to pass.

Local Linux Debug/GCC 14.2 CTest: **41 passed, 2 skipped, 0 failed** (43 registered).
See `validation/zip-integration-2026-11-11/ctest.txt`. Skipped tests require optional
personal NPNO/NSMP fixtures. Runtime host tests use PortAudio/sfizz doubles; no new
real-render or hardware timing/quality claim is made. The extracted catalog helpers
and host path changes are compiled/tested; the Win32 GUI integration has not been
compiled or executed on Windows in this environment. No new executable or ZIP was
built, and the installed application is unchanged until rebuilt and updated.

GitHub authenticated remaining allowance is unknown because no corresponding tool
is exposed. All builds/tests are local; no Actions are launched or modified.
