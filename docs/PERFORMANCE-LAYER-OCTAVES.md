# Independent layer octaves — 11 November 2026

Base: `178591d0e3fdcd25c089ecab63a1c55606ce8f40`.

Each of the four Performance Editor layers now has an **Oitava** selector:
Original (0), -1 through -4, +1 through +4 octaves. For example, layer 1 can keep
piano at 0 and layer 2 play pad at +1. Disabled layers have disabled selectors.
Vision supplies a numbered accessible label and a keyboard-operable native
HTML select. The classic UI has an octave combo alongside its layer volume.

The engine already supports -48..48 semitone transposition per layer. The new
control exposes that existing routing with octave steps; it does not add DSP,
pitch resampling, a new audio buffer, or a profile schema. The bridge encodes
an octave choice as b=0..8 (octave=b-4), with layer a=0..3. It validates bounds
and calls a shared model helper. Model changes use the existing asynchronous
editor preparation and cached-engine routing cleanup.

Split ranges apply to the physical input key before transposition. Out-of-range
MIDI results (<0 or >127) remain silent rather than wrapping or folding. When
changing the mapping, previously held voices on the affected cached layer are
cleaned by the existing handoff; play a new note to audition the new octave.

Octave belongs to the Performance layer. Changing its instrument keeps its
selected octave; clearing a layer or resetting the editor returns it to 0.
Copy/swap follows the existing full-part behavior, carrying the octave with the
copied/swapped sound while preserving the destination split zone.

Existing saved Performance/profile serialization and automatic editor drafts
already include transpose. New choices therefore persist in the permanent
Performance library, show profiles/favorite references and recovered drafts.
No standalone instrument is transposed globally. Existing non-octave semitone
values are preserved and shown as `Atual: +N semitons` until a new octave is
explicitly chosen; loading/editing them does not silently round them.

## Validation

Linux Debug CTest: **42 passed, 2 skipped, 0 failed**, 44 registered. The optional
personal NPNO/NSMP fixtures were unavailable. Existing volume-recall, NPNO
recovery, MIDI and parameter persistence tests continue to pass.

New `performance_layer_octaves` tests the exact piano-original/pad-+1 example,
all nine octave choices, invalid/disabled layer rejection, note-on and note-off
routing, source-key split ranges, positive/negative MIDI bounds, held-note/sustain
cleanup across an asynchronous mapping change, and profile/library/draft/copy/swap
persistence. An opt-in fake-sfizz fixture reports the received note in its output,
so these assertions observe the host's dispatched notes rather than just the
stored model. SF2 uses the same host note mapping, but no new real SF2/sfizz
render or musical-quality validation is claimed.

Two production-JavaScript renderer tests pass: existing keyboard audition/save
behavior and the full DOM editor test, extended for per-layer octave choice,
accessible labels, disabled layers, state restoration and legacy semitone display.
Protocol tests cover the new command bounds. JavaScript syntax and git whitespace
checks pass. See `validation/layer-octaves-2026-11-11/`.

The Windows bridge harness now checks command dispatch, native combo selection,
permanent save/reopen and editor reset. It was not compiled or executed in this
Linux environment. Classic layout pixels, Windows execution and ASIO hardware
remain unverified here. This source stage does not produce an executable or
installation ZIP; rebuild/update the Windows app and its matching vision-ui files
to make the feature available on the musician's machine.

All validation is local. Authenticated GitHub remaining allowance is unknown;
no hosted Actions, workflows or binary uploads are used.
