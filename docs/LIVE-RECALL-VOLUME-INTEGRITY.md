# Live recall gain and NPNO integrity — 11 November 2026

Base: `9cd4d8fde80743e517b3bc4b626bc5fb8bc96c35`.

## Confirmed live-volume defect

The calibrated per-preset gain is already fixed before playback. The problem is
in the smooth-transition mixer: it divides the active sound by the sum of the
active weight and the outgoing tail weights. A single tail can therefore reduce
the new instrument by roughly 6 dB after its short entrance fade, recovering
progressively over the two-second outgoing tail. Two tails can attenuate it more.
This reproduces the reported quiet-first/volume-rising behavior without changing
calibration, samples, master volume or saved parameters.

Replace that normalization with a complementary 20 ms crossfade at the new
sound's entrance. Normalize the outgoing mixture independently; the new sound
then keeps its own full gain instead of following the old sound's release.
The existing 3 ms handoff and master smoothing remain. The outgoing tails keep
playing until the new sound enters or their existing timeout expires. Once the
new entrance completes, old tails stay inaudible in subsequent rests; the prior
active-weight decay could bring them back. The tradeoff is deliberate: after the
new sound enters, its full volume takes precedence over preserving a two-second
simultaneous audible tail. Render count, handoff budget, MIDI event routing,
level calibration and real-time allocation rules are unchanged.

`live_recall_immediate_gain` holds the old note while alternating two sounds with
different saved gains eight times. Every sample from block 7 (under 35 ms at the
48 kHz test rate) through more than two seconds must match the new expected gain.
The same test compiled with the original audio_host.cpp fails on that assertion;
the corrected mixer passes. This is a deterministic PortAudio/sfizz test-double
measurement of mixing behavior, not a hardware end-to-end latency measurement.
Existing transition tests also verify eight-engine bounds and sample continuity.

## NPNO updater

- Parse metadata and audit as JSON objects/arrays with vendored picojson v1.3.0
  (BSD, int64 support). Change only the intended object fields; preserve unknown
  nested values, including 64-bit integers. Formatting/key order may change.
- Accept compact, pretty, CRLF and reordered JSON. Reject malformed input and
  incompatible field types before committing any update. Avoid repeatedly
  parsing/serializing each individual zone field.
- Match complete `trigger=release` and `trigger=release_key` opcode values on
  generated region lines and ignore comments. Keep unrelated trigger values.
  The supported input remains converter-generated SFZ (one region per line),
  not an arbitrary SFZ parser supporting includes/macros and inherited opcodes.
- Commit template, SFZ, metadata and optional audit via a sibling durable intent
  (`.release-pending.json`) containing exact bytes and SHA-256 per target. Validate
  all entries and JSON before replacing files; reread/verify each replacement.
  On failure the intent remains. The existing startup `applyReleaseControls`
  recovers it before calibration/playback. Recovery is idempotent, finishes the
  intended generation and reports a change even when requested controls match,
  ensuring the engine cache is invalidated after a runtime retry.
- Playback resolution rejects converted banks with a pending intent. Corrupt
  recovery data raises an error without touching targets; keep the intent for
  diagnosis. Targets are restricted to four known sibling suffixes.
- Flush parent directories on POSIX after atomic replacement and journal removal;
  Windows retains flushed file handles and MoveFileEx WRITE_THROUGH. This is a
  recoverable multi-file commit, not simultaneous filesystem atomicity. Hardware
  failures that ignore flushes and physical disk damage are outside this guarantee.
- No PCM, attack/string-tail or pedal-on sample content is modified. Existing
  pedal-up gating and baseline-relative release gain are retained.

## Validation

Local Linux Debug build, GCC 14.2, assertions enabled; libsndfile.so.1 for offline
conversion fixtures. CTest: **38 passed, 2 skipped, 0 failed**, 40 registered.
See `validation/live-recall-2026-11-11/ctest.txt`.

New integrity tests cover empty/compact/pretty/CRLF JSON, unknown nested and large
integer fields, malformed JSON, comment/opcode discrimination, repeated gain and
enable/disable round trips, storage failure after partial replacement, replay at
all five transaction boundaries, invalid hashes and blocking pending playback.
Existing NPNO migration/runtime tests continue to verify unchanged PCM, attack
regions, profile defaults and nonaccumulating gain. Existing audio handoff,
transition-budget, MIDI, four-layer and parameter-persistence tests also pass.

N/A: optional personal NPNO and NSMP fixtures were unavailable. This run did not
render through real sfizz/FluidSynth, execute Windows, test ASIO hardware or load
Hugo's current favorite banks. No new Windows executable or portable ZIP is
produced; the installed executable must be rebuilt/updated to use these fixes.

Authenticated GitHub remaining allowance is unknown: available connector tools
expose no authenticated quota/billing endpoint. Validation is local; no hosted
Actions, workflows, binaries or samples are uploaded. Existing workflows are
manual or restricted to unrelated feature branches/paths.
