# Actual generated SFZ examples

`nki.sfz`, `nsmp.sfz` and `npno.sfz` are snapshots of the corrected converters'
actual output, generated from the independent 440-Hz fixtures. These text files
are for inspection; their WAV dependencies are generated, rather than duplicated
in the source repository.

To obtain complete playable instruments with WAVs and sidecars, run from the
repository root:

```sh
LP_IMPORT_TEST_OUTPUT=/absolute/path/validation \
LP_REAL_SFIZZ_LIBRARY=/absolute/path/libsfizz.so \
bash scripts/verify_timbre_import.sh /absolute/path/libsndfile.so.1
```

The complete instruments are under `validation/render-output/nki`, `nsmp`,
`npno`; each includes the preserved original, `.converted.sfz`, Samples and
metadata. `*-preview.wav` are short post-conversion previews. The named NKI
programs are under `validation/nki-output/multi.nki`; the default derived SFZ
selects the first program, never superimposes all programs. Additional named
programs may be imported as SFZ using the existing application importer.

No original keyboard/Kontakt recording is claimed as a listening reference.
