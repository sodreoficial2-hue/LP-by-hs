# Performance editor: live layers and independent saving

Entering the editor stops the category/favorite instrument and starts an empty
four-part workspace. Choosing a part or moving the highlighted selection with
arrows immediately applies it to audio. Removing a part or changing split mode
also applies immediately. Native and Vision controls share this behavior.
The redundant Activate button is removed. Program Changes and number shortcuts
cannot replace the layers with a favorite while the editor is open.

Save Performance appends a named snapshot, atomically flushes the separate
performances.lphs file in the existing application settings folder, refreshes the
Performance category and clears the editor only after success. Parameters,
ADSR enablement, zones and MIDI mappings are included. A failed write retains
the library and editor. Saving a show profile is no longer needed for saved
performances, and browsing away does not implicitly create library entries.

On reopening, the independent library is restored. Opening an older show profile
merges its performances with the permanent collection and remaps indexed favorite
references. Exact snapshots are deduplicated; same-name, different arrangements
remain distinct. Previously profile-only arrangements must be imported once by
opening that profile. Profiles still export performances for portability.

Right-click (or Shift+F10) in the Performance category offers Delete Performance
with confirmation. Deletion is saved atomically, removes references to the deleted
arrangement and adjusts subsequent favorite indices. Instrument source files are
preserved. Ctrl+D does not hide saved performances. An explicitly opened old
profile may import its archived arrangements again.

## Audio changes

Two code paths could contribute to the reported delay: MIDI notes previously
waited behind synchronous bank loading in the window queue; transitions could
render the current four-layer graph plus two obsolete four-layer graphs. These
are identified contributors, not a hardware reproduction of the persistent ASIO
symptom. Hardware notes, bend, sustain and modulation now use a separate SPSC
queue fed by the WinMM callback. UI controls retain a separate producer queue.
Native CC1 still reaches the engine when learned, without duplicate forwarding.

Performance changes retire previous graphs without layered crossfades; ordinary
single-sound transitions retain their existing bounded tails. Unchanged parts
reuse their prepared synths, so selecting one layer does not reload the other
three. Only the callback touches live synth state. Preparation, shared-ownership
changes and graph destruction remain on the owner thread. Routing changes reset
reused voices to avoid stuck notes from previous split zones/transpositions.
No ASIO buffer, sample rate or driver setting is changed by editing a Performance.
A newly selected large sound still needs its initial disk/sample preparation.

## Validation

- All 17 local CTest groups passed with assertions enabled. Updated audio handoff
  tests cover repeated four-layer changes, reuse without re-opening definitions,
  split changes, immediate return to a single synth, unload and unchanged ASIO
  start counts. Independent storage tests cover restart, full part settings,
  deduplication, favorite index remapping, deletion and failed writes.
- Production JavaScript selection/menu/caret fixtures passed. The full renderer
  ran in JSDOM with a simulated native bridge: all four keyboard selections,
  Save/reset and absence of script errors passed. This is functional DOM testing,
  not a rendered visual inspection or an audio test.
- Windows x64 Vision production executable and the native bridge test harness
  cross-compiled with the real PortAudio ASIO backend. The Windows harness was
  compiled but not executed. No Windows/ASIO hardware timing validation occurred.
- Chromium was unavailable and its browser download did not produce a valid ZIP;
  no new screenshot/visual-layout approval is claimed. The updated Playwright
  fixture remains available for a browser-enabled environment.
- No hosted builds or binary uploads. Authenticated GitHub quota remains unknown:
  available connected tools do not expose an authenticated allowance query.
