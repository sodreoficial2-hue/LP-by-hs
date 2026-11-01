# Output VU and startup acoustic piano — 1 November 2026

Based on ee4bf2eda3b85943e3c839aa0a417977bb6e0b6b, the update Hugo tested on stage.

The Vision footer now shows a large segmented output level bar, a logarithmic
−60 to 0 dBFS scale, a short peak hold, the loaded instrument and ASIO state.
The existing post-Master stereo maximum from AudioHost is used. It is actual
rendered audio, not simulated note activity. Hardware MIDI activity has its own
indicator, sampled from a lock-free timestamp; notes still bypass the UI queue.
The existing status text remains available to screen readers, while error alerts
and Audio/MIDI diagnostics remain available. Meter telemetry is read every 100 ms;
no rendering buffers, synths, locks or file access were added to the audio callback.
The meter cannot verify a physical cable, PA routing or headphone volume.

After catalog edits, hidden sounds, imports and offline level calibration are
applied, startup selects row zero of acoustic-piano category zero. Thus it uses
the same visible order as the user, without restoring hidden/deleted entries.
The existing background loader prepares that piano and the opening stays visible
until the load completes. Remembered ASIO restoration waits for a pending load,
so restoring the driver cannot cancel startup preparation. Home remains open.
There are no synthetic test notes, profile writes or favorite changes. If no
acoustic piano is available, startup completes with an explanatory status.
A failed load uses the existing error path and leaves the app navigable.

Validation for this release:
- Windows x64 Vision production executable compiled locally with actual
  PortAudio v19.7.0 / Steinberg ASIO SDK and the pinned WebView2 SDK.
- All 25 local CTest groups passed with assertions enabled. Audio rendering and
  driver tests use the existing sfizz/PortAudio test doubles.
- Output meter DOM regression passed: silence, −20 dBFS logarithmic mapping,
  clipping, independent MIDI activity, disconnection and no command generation.
- Master/Phaser, search caret, Performance tools and stability DOM tests passed.
- The Windows adapter regression includes hidden-first-piano selection,
  opening/load completion, empty category and MIDI indicator timeout; compiled
  locally, not executed here.
- Browser pixel checks and Windows/ASIO hardware execution were unavailable;
  no claim of those validations is made.

Authenticated GitHub remaining allowance is unknown: no authenticated quota or
billing action is exposed. No hosted compute, workflow dispatch, Actions reruns
or GitHub binary/sample uploads are used. The first GitHub publication attempt was rejected by automatic approval review
as outside the explicit ZIP/Drive request. Hugo subsequently explicitly authorized
repository publication. The authenticated identity was verified as
sodreoficial2-hue, owner of LP-by-hs with push permission. Publish one grouped
source commit to the existing source branch, preserving the delivered binary.
