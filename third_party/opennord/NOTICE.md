# OpenNord attribution and source provenance

The C++ files in `src/nsmp/` and `src/nsmp_to_sfz.cpp` are ports/adaptations of
OpenNord, https://github.com/simonflore/opennord, maintained by Simon Flore and
OpenNord contributors, under **AGPL-3.0-or-later**. Preserve upstream notices,
this attribution and the accompanying complete license in `LICENSE`.

Pinned upstream commit: `67ba1b657ff65a803d568f0ba1fba08598fdc830`.
The C++ adaptation and tests were made for Live Performance by Hugo Sodré.
Do not remove SPDX notices from the port. When conveying a combined derivative,
comply with the AGPL and provide its corresponding source; a credit comment alone
does not replace those license terms. The source/patch deliverable includes the
ported files, build changes, fixture generator and license.

| C++ module | OpenNord source and functions |
|---|---|
| `cbin.cpp` | `clavia/cbin.ts`: cbinFraming/readCbinHeader; `clavia/sample-identify.ts`: identifyNsmp/nsmpLayout |
| `checksum.cpp` | `clavia/checksum.ts`: crc32/verifyNs4Checksum; `clavia/crc16.ts`: crc16ccitt/verifyTrailerChecksum |
| `bitstream.cpp` | `clavia/bitstream.ts`: readBits/writeBits |
| `nsmp_sections.cpp` | `ns4/nsmp.ts`: parseNsmpSections/readName/readNsmpZones/parseLegacyZoneRecords/parseCodec2ZoneRecords/parseCodec3V12ZoneRecords/parseCodec4ZoneRecords/walkMultiStrokeZones/readPerNoteRows |
| `nsmp_codek.cpp` | `ns4/nsmp-codec.ts`: readBlockHeader/decodeStroke; `ns4/nsmp.ts`: decodeStrokeSection/findStrokeStopAt/streamReachesEnd/readStrokeRegions/strokeLoop/strokeSampleRate |
| `nw1_dsp.cpp` | `ns4/nw1-dsp.ts`: dsp2Level/dsp2Detune; `ns4/nsmp.ts`: loopDecayDbPerSecond |
| `nsmp_to_sfz.cpp` | Decoder adapter; channel scaling from `ns4/nsmp-audio.ts`: normalizeChannels, known map fields from nsmp.ts |

Upstream's spelling is **nsmp-codec.ts**, not `nsmp-codek.ts`. The user requested
`nsmp_codek.cpp`; that C++ spelling does not refer to a different codec.
`nw1-dsp.ts` supplies level/tune math; the actual predictive audio decoder is in
`nsmp-codec.ts`. No `docs/NSMP-CODEC.md` was present in the pinned tree.

The port adds explicit size/allocation bounds and fails instead of returning
partial/truncated audio. It makes the codec4 final stop-without-residual EOF case
explicit. It does not add undocumented Nord field offsets. Normalization and
hardware replay-rate uncertainties in upstream are retained in the sidecar/docs.

Fixtures contain original synthesized test tones, passed through upstream's
public workshop writers and decoded by its original TypeScript algorithms.
They contain no Clavia factory or commercial sample audio. See the fixture README.
