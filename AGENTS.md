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
