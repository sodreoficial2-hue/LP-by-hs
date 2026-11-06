# NPNO/NPMO auxiliary release: pedal-up only (5 November 2026)

Hugo identified that the auxiliary sound appeared when releasing keys after a
long hold, rather than when releasing sustain. The preceding converter used
`trigger=release_key` in both CC64 ranges, including an uncontrolled duplicate
for pedal-down. This explicitly bypassed sustain; the prior acceptance test
validated that incorrect key-up behavior.

The NPNO converter now emits one auxiliary bank with `trigger=release locc64=64 sustain_lo=64`.
In sfizz, `Layer::registerNoteOff` first checks CC conditions, then queues a
release while sustain is pressed. `Synth::Impl::startDelayedSustainReleases`
plays that queue on pedal-up. Explicit `sustain_lo=64` makes the threshold
match the host's binary sustain route even in direct offline rendering (sfizz's
default threshold is 1, not 64). Consequently:

- A key-up with CC64 below 64 cannot start or queue the auxiliary.
- A key-up with CC64 at least 64 queues it without playing it.
- CC64 falling below 64 plays the pending releases.
- Keys still held when the pedal rises retain their normal attack sound.
- Pedal movements without pending releases do not fire all sample regions.

This is the per-note source bank, not an invented mechanical pedal sample.
Attack envelopes, normal damped string tails, original sample bytes, gain
controls and the ordinary CC64 MIDI route remain intact. Other import formats
keep `release_key` semantics. The additional trigger discriminator is included
in velocity/crossfade/group identities.

`updateNpnoRelease` migrates existing generated banks at catalog startup, even
when their saved gain/enabled options are unchanged. It removes old passthrough
duplicates, uses the immutable release template to avoid accumulating gain,
and records `pedal_release_revision=2`. Original WAV files are never rewritten.
Disabled releases can be restored without reconversion. Metadata outside the
zone list and unknown fields inside retained zones are preserved; audit release
entries and post-policy region counts are updated transactionally.

The audio acceptance test now checks the corrected event order, rather than
expecting sound at key-up: velocities 40/80/110, holds of 0.125/2/5 seconds,
CC64 threshold 64/63, repeated values, key-up with pedal up/down, empty pedal
cycles and keys held across pedal-up. It also compares the complete instrument
with the auxiliary muted to verify identical attacks and ordinary string tails.
The isolated release diagnostic uses `rt_dead=on` only because its attacks are
intentionally absent; production output retains sfizz's normal attack-voice check.

Primary implementation reference inspected:
https://github.com/sfztools/sfizz/tree/f5c6e29f23b8057867c08e88f5f6ac6738baa30b/src/sfizz
(`Layer.cpp` and `Synth.cpp`). This source is also used for the local real
renderer build. No exact Nord DSP equivalence or Windows/ASIO hardware validation
is inferred from synthetic fixture rendering. No new portable release or Drive
upload was requested for this source correction.

Authenticated GitHub quota remains unknown: no account quota/usage tool is
exposed. Verification uses local builds, without hosted workflows or binaries.

Local validation completed:

- Debug build: successful.
- Baseline CTest: 36 executed tests passed; 2 external-fixture tests skipped
  because personal source files were not supplied (38 registered at that stage).
- `npno_release_runtime_and_profiles`: passed, including both old single-bank
  and old split/passthrough schemas, disable/restore, idempotence and PCM hashes.
- `npno_release_audio_acceptance`: passed with the real locally built sfizz;
  all nine velocity/hold combinations and full-signal comparisons passed.
- `shared_format_real_render`: passed, including pitch, velocity/crossfade
  boundaries, pedal-up voice counts, stereo/mono and voice-budget checks.
- Generated/migrated metadata and audit JSON parsed successfully; whitespace
  check passed. No Windows executable/hardware test is claimed for this change.

## Requested Windows update delivery

Hugo subsequently requested a compiled update ZIP and Google Drive delivery.
Windows run 42143310123 completed successfully on source/package revision
8e2cbf728759b72b565e632871c405dca94fc122: 40 executed CTest cases passed and
2 optional external-fixture cases were skipped. Production ASIO classic/Vision
applications compiled; no additional compiler runtime DLL dependency was found.
Actual ASIO hardware remains outside this automated validation.

The initial Windows gate exposed an older envelope test's fixed late/early
attack ratio assumption. The test now compares OSC-controlled ADSR audio with
the same runtime's explicit native SFZ ADSR reference for attack, independent
voices and release (relative error < 0.0001), while retaining audible rise,
decay, sustain, final silence and native-control restoration checks. This
changes only validation, not the production engine or the sustain correction.
Local reference comparisons had zero error; the complete Windows gate passed.

Delivered ZIP: LivePerformance-Atualizacao-Sustain-Pedal-Windows-x64.zip
SHA-256: 48ca43ae549d1316130315711e3826ed0d8ddd8e2a40d3cfd8be777ceb3f18ee
13 payload hashes were verified against the archive's SHA256SUMS.txt, both EXEs
were checked as AMD64, and archive CRC/integrity checks passed.
