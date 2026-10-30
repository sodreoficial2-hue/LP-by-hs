# Stability and fluency follow-up — 30 October 2026

This stage implements the fourteen findings from the Live Performance audit.
It is a source update, with local Windows compilation; no portable release,
installer, Drive upload or hosted build is part of this stage.

1. Completed background loads post a window notification immediately. The timer
   remains a fallback rather than imposing its full interval on activation.
2. Catalog sounds, favorites and saved Performances use the latest-selection
   background loader. Preparation does not restart the ASIO stream.
3. Each requested graph owns parameter controls. Adjusting a pending sound cannot
   change the old live graph. Failed requests restore the previous sound model.
4. A ticketed MIDI barrier holds hardware notes following Program Change until
   the requested graph is published and the UI acknowledges the selection.
   Failure/panic cancels waiting notes instead of leaving a stuck pedal/note.
5. DSP skips disabled effects, lets existing time-effect tails decay, caches
   coefficients and clears tiny comb values. It adds no master compressor.
6. Graph disposal runs on the loader worker. Up to four small SF2 voices are
   retained for same-slot reuse (eligible banks <=8 MiB each). This is a bounded
   voice cache, not shared mutable synthesis across layers. Explicit permanent
   source deletion stops and joins resource owners before removing the file.
7. Permanent Performance saving atomically records both the library and completed
   draft epoch. A leftover draft cannot revive an already-saved arrangement.
   Configuration writes preserve the previous version in a .backup file.
8. Corrupt settings recover from a valid backup; invalid originals are preserved
   under unique names. Missing individual catalog entries produce warnings and
   leave valid entries available. Structural catalog failure falls back to SFZ
   discovery. One named Windows instance prevents concurrent settings writers.
9. SFZ calibration fingerprints include sample/include dependencies and metadata,
   with bounded conservative inventory for unresolved macros. SF2 preset discovery
   reads preset headers without decoding sample PCM.
10. The renderer receives compact meter/parameter telemetry between structural
    changes. Draft writes are coalesced on a worker; memory holds the latest draft.
11. Ctrl+S works while typing; Delete targets the focused layer. Left/right adjust
    master outside text, combo and slider inputs; home category navigation remains.
12. Hidden sounds cannot become new layer selections, while an existing hidden
    reference remains editable. Native zone text is created, and names support
    128 UTF-16 input characters with compatible UTF-8 storage bounds.
13. Production graphs and volume controls use short smoothing ramps. Peak warnings
    show limited headroom at 0.89; driver buffer ownership and DSP gain contracts
    remain unchanged. This is not a limiter or loudness normalization change.
14. Audio/MIDI diagnostics show callback processing load, interruptions, frames,
    synth count and process memory. Automated stress covers 300 four-layer
    requests concurrent with callback/MIDI/sustain and solo-mask changes.

## Persistence compatibility

`performances.lphs` uses the LPHS_PERFORMANCES 2 wrapper, retaining legacy Session
reading. Draft version 3 adds epochs and reads versions 1/2. Show profile formats
remain readable. Permanent performances remain independent of manual profile save.
Backups are previous committed bytes, not unlimited history.

## Validation and limits

The 22 local CTest regressions pass, including asynchronous graph parameter
isolation, first-note ordering, failed-load rollback, durable draft completion,
backup recovery, dependency changes and concurrent switching. Renderer JSDOM tests
exercise actual controls and telemetry; Windows production and dispatcher/layout
harnesses are compiled locally. Test doubles do not establish real device latency.

Real Windows/ASIO testing is still required: listening under sustained four-layer
polyphony, rapid SF2/SFZ browsing, long sessions, MIDI hardware Program Change,
source removal and reconnecting the actual driver. Reported callback load measures
render processing time, not total keyboard-to-speaker latency. The locally built
executable has not replaced the user's installed program.

Authenticated GitHub allowance is unknown: the connector has no authenticated
quota/billing tool. This stage uses one grouped source commit, no Actions or binary
uploads; a quota check cannot guarantee account safety.
