# Independent 440-Hz vectors

`reference.json` contains compressed, hash-verified binary vectors produced by
`scripts/generate_pitch_fixtures.mjs` using OpenNord
`c86b22a8014afcc9d6dc96409992f10f3c004f57` (AGPL-3.0-or-later; attribution and
license in `third_party/opennord` and `third_party/npno`). The source is a pure
440-Hz sine, 6000 PCM units, 35002 Hz. Files include all four sample generations
and both piano-module families. These are synthetic interoperability vectors,
not factory samples or claims of original-hardware loudness equivalence.

`python3 tests/fixtures/pitch/unpack.py OUTPUT` restores bytes without network,
Node, CMake or a reference checkout. SHA-256 and byte length are verified.
The generator requires Node 24 and a disposable checkout of the pinned reference.
