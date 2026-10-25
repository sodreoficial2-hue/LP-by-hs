# Live Performance by Hugo Sodré

## GitHub usage constraint (user instruction, 20 October 2026)

The user reports losing access to a previous account after excessive service usage. Treat usage limits as an ongoing project constraint.

- Inspect available authenticated quota/usage tools before remote write stages. A public unauthenticated quota does not measure this linked account.
- The current connector does not expose account billing or authenticated rate-limit balance, and its generic fetch rejects `/rate_limit`. If unchanged, explicitly record that the remaining allowance is unknown; never claim it was verified or promise protection from suspension.
- Build, compile, and run regression tests locally. Do not create automatic GitHub Actions workflows, launch hosted compute, or rerun jobs while usage cannot be verified.
- Keep changes local until validated. Publish one small source commit per completed stage, using inline Git tree entries to group text changes. Avoid per-file blob upload loops and repeated binary/sample uploads.
- Use sequential remote writes, no polling loops. Stop on quota/rate-limit errors (403/429 or explicit limiting messages), preserve local work, and report the response. Do not rotate credentials or accounts to continue.
- A quota check cannot establish why a prior account was suspended or guarantee a future account will not be suspended.

## Reconstruction constraints

Preserve `recovery/extracted` and SHA-256 inventories. These are original artifacts, not the new source. Do not claim assembly is original C++ source.

Distinguish build success, test-double validation, real sfizz rendering, and Windows/ASIO hardware validation. Never claim the latter from the former. The new host is not yet a replacement for the supplied working executable.

Keep accessibility: large controls, visible focus, keyboard arrows, ESC and E, twenty favorites numbered 1–20. Wire MIDI Program Change is 0–19.

## Favorite MIDI contract (user correction, 20 October 2026)

Favorite N always corresponds to displayed Program Change N (1–20); MIDI wire values are N-1 (0–19). Right-click Favorite Learn accepts only the corresponding Program Change and records its MIDI channel in the manually saved show profile. Do not assign notes or CC to favorite selection. A wrong PC during learning must leave the binding untouched and show the expected PC. Knob MIDI Learn remains CC-based. Legacy reconstruction v2 note/CC favorite bindings are discarded on import while preserving sounds, parameters and knob mappings.

## User validation and release gate (20 October 2026)

The user confirmed that stage 4 audio/ASIO, effects, volumes, keyboard commands, scrolling and buttons work fluently on Windows. Preserve this behavior. The reported defect is reconnecting the selected ASIO automatically after reopening. Stage 5 implements native per-voice ADSR and dynamic Envelope Filter; the next task is layout. Do not generate/publish another test package, installer or Drive upload until the user requests the next release. Local compilation for verification remains authorized. Distinguish stage 4 hardware confirmation from stage 5 local/real-sfizz offline verification.

## Performance MIDI (stage 8)

Forward complete 14-bit pitch bend to sfizz, centered at signed zero. Keep CC64 dedicated to conventional pedal up/down (0–63/64–127); channel-mode CCs must not be learned as knobs. Forward CC1 natively even when it also changes a learned parameter. Preserve received performance controls across instrument and ASIO swaps; do not overwrite untouched SFZ defaults. Panic/overflow must release pedal and center wheels. Curated lead/MTG sax mappings are configured by scripts/configure_bank_performance.py; do not modify sample audio or recovered originals.

## User validation and Performance editor follow-up (24 October 2026)

The user confirmed the Vision frontend portable at `7c639606` works well on their Windows machine, including the layout and normal operation during a stage simulation. The authorized follow-up is limited to per-part category filters, complete parameters for the selected part inside the Performance Editor, four-volume-only playback mix, automatic latest mix recall for favorites, temporary mix for nonfavorites, and removal of the redundant header Favorites icon. A new local compilation and portable are explicitly requested. Preserve all unrelated working behavior. This confirmation covers the preceding portable, not untested changes in the follow-up.

## SF2 portamento investigation (25 October 2026)

The user reported that the compiled SF2 Portamento knob still did not produce smooth glide and requested a thorough investigation with research into the SF2 engine. Native FluidSynth portamento replaces the SF2-only bend simulation after reproducing the invalid 96-semitone sensitivity call (API maximum 72). Preserve Vision, ASIO, SFZ, profiles and per-part routing. Validate the audio pitch trajectory with a real library, including polyphony at zero, sustain, pitch wheel, rapid legato and panic. This source investigation does not request a new portable release.

## Large-hall reverb (25 October 2026)

The user requests deeper, large-room/worship reverb above 70%. Preserve the existing four-comb response through 70%, add a smoothly blended damped eight-line hall only in the upper range, and keep the parameter/profile/UI contract unchanged. This shared DSP affects SF2 and SFZ; no new portable upload was requested. Validate decay, bounded output, reset, automation and existing glide regression locally.

## Parameter panel persistence (25 October 2026)

Remove the 30-second idle close. Parameter views remain open during inactivity and close explicitly with E. Escape may cancel MIDI Learn but must keep the parameter panel open in both Vision and native fallback; regular navigation keeps its existing behavior. Preserve the native SF2 glide and large-hall reverb commits. No portable release requested at this stage.

Validation: local UI-state regression passed; JavaScript syntax checks passed. Production Windows Vision executable and updated native/Vision test harnesses compiled locally. Windows harnesses were not executed (no Windows/Wine runtime), and the browser fixture could not run because Chromium is not installed. Authenticated GitHub quota remains unknown because no quota tool is exposed; no Actions or binaries are uploaded.

## Driver-owned ASIO buffer (25 October 2026)

The user authorized removal of the buffer-size selector from Audio/MIDI. Remove it from Vision and the native fallback, expand the driver field, preserve ASIO Control and driver-reported latency. Connect and reconnect with buffer argument 0 (PortAudio automatic/driver-compatible selection); legacy audio.settings files remain readable but their old requested buffer value must not be reapplied. Driver/MIDI selection and profile formats remain compatible. Retain prior native glide, worship reverb and manual parameter closing. No portable requested in this step.

Validation: audio-host and Vision-protocol regressions passed locally with the PortAudio test double, including restoration from older nonzero buffer settings to automatic mode. JavaScript syntax checks passed. Production Windows Vision and both Windows UI harnesses compiled locally; no Windows/ASIO hardware or browser runtime validation is claimed. GitHub exposes no authenticated quota tool; remaining allowance is unknown. Publish one source-only commit, without Actions or binary uploads.
