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

## Additive free SF2 expansion (25 October 2026)

The user explicitly requests a free, researched SF2 ZIP sent to Drive and automatic additive category discovery by placing the extracted content beside the EXE. The existing EXE does not support these folders. Include a companion Vision EXE update and matching UI assets so the requested extraction workflow is concrete; preserve the installed runtime DLLs and banks. Do not mislabel this update as a complete portable. Discover the expansion from Timbres-SF2-Selecionados, use explicit numbered category folders/GM families, and avoid loading all PCM on startup. Keep all prior fixes. No Actions or GitHub binary uploads.

## Final user approval and catalog exclusion shortcut (26 October 2026)

Hugo confirms all changes in the 104-SF2 expansion/update are approved and the software works as intended. Add Ctrl+D on a selected category sound with a Yes/No confirmation. Exclude only that catalog entry, persist automatically outside show profiles, keep original SF2/SFZ files and other presets, and preserve saved favorites/performances and the live audio engine. Favorites retain their existing Space removal shortcut. Source is based on 4a461a31d7f8f74c56c2ac231aefa573d2e64f34. This request asks to develop the command; no new portable/Drive release was requested.

Validation: hidden-sound persistence and Vision protocol tests passed locally; JavaScript syntax checks passed. Windows Vision production target and both Windows UI harnesses compiled locally, without executing Windows confirmation dialogs or ASIO hardware. No browser fixture execution is claimed. GitHub exposes no authenticated quota/usage tool; remaining allowance is unknown. Publish one source-only commit without Actions or binaries.

## Stage readiness improvements (26 October 2026)

The user authorized all six review suggestions before the next compiled update: unsaved-profile protection, restore catalog exclusions, name search, continuous sound transitions, peak indication and missing-file repair/portable profiles. Preserve the approved ASIO/MIDI, SF2 native glide, upper-range hall and manual panel closing. Changes are based on 73ae3a8. See docs/STAGE-READINESS.md for bounded tails, synchronous UI preparation, v9 portability, tests and hardware/renderer validation limits. No hosted Actions. Authenticated GitHub allowance remains unknown.

## Automatic persistent preset volume balance (28 October 2026)

Hugo explicitly authorizes implementation of automatic first-start calibration,
permanently saved outside show profiles and reused after closing/rebooting. Only
new presets require analysis; modified source SFZ/SF2 files also invalidate their
own measurements. Preserve musical dynamics and manual/favorite/Performance
volumes. Do not add master compression. Use private offline synthesizers, never
send calibration notes to the output or live MIDI. Apply fixed per-preset gains
before the layer sum, with no new audio buffers or callback file access. Keep the
search-caret fix from 28ad0f5 and all previously approved behavior. Details and
validation limitations are in docs/AUTOMATIC-LEVEL-BALANCE.md. Local Windows
compilation is authorized; no new portable/Drive release or Actions requested.

Validation: 18 local CTest groups passed with assertions active, including real
FluidSynth audio, cache reuse/new additions, independent presets and compensated
playback. Vision/classic Windows production executables and Win32 harnesses
cross-compiled with real PortAudio/ASIO sources. Windows/ASIO hardware and actual
user-bank loudness validation remain pending. SFZ calibration was exercised with
a test double; no new real-sfizz offline validation is claimed. Authenticated
GitHub allowance is unknown; publish one grouped source-only commit.

## Catalog context menu (29 October 2026)

Hugo authorizes rename, category move, reversible hide and permanent source-file
removal through right-click on instrument-category sounds. Preserve the latest
startup level calibration and approved MIDI/ASIO behavior. Context operations
capture identity before modal UI and save metadata automatically outside show
profiles. Ctrl+D remains reversible hide. Multi-preset SF2 and shared SFZ
definitions must not lose other instruments. SFZ sample audio is preserved.
See docs/CATALOG-CONTEXT-MENU.md for deletion behavior and validation limits.
No new portable/Drive release was requested in this step. Authenticated GitHub
quota remains unknown; no hosted builds or binary uploads.

## Live Performance editor flow (29 October 2026)

Hugo authorizes immediate audition/activation in all four layers, exclusive layer
routing on entering the editor, removal of Activate Performance, automatic
permanent Save Performance independent of show profiles, editor reset after a
successful save and right-click deletion from the Performance category. Investigate
latency during editing and after returning to single sounds. Preserve startup
level calibration and catalog editing. See docs/PERFORMANCE-EDITOR-FLOW.md for
implementation, migration and validation limits. Local Windows compilation is
authorized; no new portable/Drive release requested in this turn. Publish one
source-only stage without Actions. Authenticated GitHub allowance remains unknown.

## Performance editor tools (29 October 2026)

Hugo authorizes all proposed editor improvements immediately: per-layer browsing
memory, solo/mute, direct volume, copy/swap, MIDI split learning, Edit/Duplicate
and Save Changes/Save as New, automatic recoverable draft, and latest-selection
background loading. Preserve prior independent Performance saving, startup gain
calibration, catalog management and ASIO/MIDI contracts. Details and validation
limits are in docs/PERFORMANCE-EDITOR-FLOW.md. Local compilation is authorized;
this turn does not request a portable/Drive release. Preserve one grouped source
commit without Actions. Authenticated GitHub allowance remains unknown.

## Audit stability follow-up (30 October 2026)

Hugo authorizes all fourteen diagnosed fluency/stability corrections. Preserve
ASIO driver-owned buffers, MIDI contracts, home category keyboard navigation and
accessibility. See docs/STABILITY-AUDIT-FIXES.md for implementation and limits.
Validate locally and publish one grouped source-only stage without Actions.
No portable/Drive release requested. Authenticated GitHub allowance is unknown.

## Performance recall, main Master and Phaser (30 October 2026)

Hugo reports residual notes when recalling a Performance and asks for more
natural switching, restored left/right Master shortcuts, a Master control
integrated into the main screen and a Phaser replacing Master in parameters.
Implement on f274a942, preserve ASIO/MIDI and prior functions. See
`docs/PERFORMANCE-RECALL-MASTER-PHASER.md` for cache cleanup, bounded shared
engine tails, Home sequential up/down navigation and profile v10 migration.
Publish one grouped source stage without hosted compute or binary uploads.
Authenticated GitHub allowance remains unknown. No new portable/Drive release
was requested in this turn; Windows/ASIO and pixel-layout checks remain pending.
