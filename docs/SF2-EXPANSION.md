# SF2 expansion discovery

Authorized by Hugo on 25 October 2026: build a researched free SF2 collection and make it additive and automatically categorized when extracted beside the existing application.

This stage extends the latest ASIO-buffer-removal branch (d7560593), retaining native FluidSynth glide, large-hall reverb, persistent parameter views, the Performance editor and Vision layout.

A new `Timbres-SF2-Selecionados` root is scanned along with the two existing locations. Its numbered category folders map to the existing category IDs. The `14-Bancos-GM` folder maps GM/GS melodic program families per preset, with bank 128 assigned to drums. Existing Red filenames and other banks retain their earlier routing. No legacy catalog, sample file, show profile or user folder is overwritten by the new discovery code.

Expansion discovery parses RIFF and phdr headers without loading PCM. This matters because the selected SoundFonts total approximately 3.77 GB, including a 1.2 GB Salamander piano. Full sample loading remains deferred until a user selects a sound. Preset references use the existing sf2:// scheme. This preserves existing reference behavior; moving the entire portable may still require resolving old absolute-path favorites as before.

Validation: the new category test covers family mappings, existing-bank exclusion, path traversal exclusion, and truncated SF2 rejection. Running the production header parser and categorizer over the actual expansion found 104 SoundFonts / 616 presets, all assigned to the expected existing categories. Offline real FluidSynth 2.3.4 loaded all files and rendered all presets with finite, nonzero audio using octave probes. This checks functionality and not every key, velocity, controller or artistic quality. No Windows ASIO hardware validation is claimed. The Windows x64 `LivePerformanceVision` target compiled locally.

The distributed ZIP is an expansion/update for an existing working portable: it contains the new EXE, matching Vision files, WebView2Loader and the expansion; it does not replace the existing sfizz/FluidSynth DLL set. Sources, per-file SHA-256, license terms and test results accompany the collection. The only converted bank is the attributed jRhodes3c SFZ-to-SF2 adaptation (samples under CC BY-NC-SA with the author's musical-use permission). Original source archive and conversion code accompany that bank. The Sonatina conversion's archive metadata and upstream licensing differ; both origins and the conservative original Sampling Plus conditions are documented.

No sample binaries are uploaded to GitHub, no Actions are launched. Authenticated remaining quota is unknown because no usage/quota API is exposed. This stage is published as one grouped source commit.
