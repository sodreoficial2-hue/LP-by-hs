# Ctrl+D: remove a sound from its category

The user approved the expansion/update before requesting this addition. Ctrl+D on a selected Instruments-category row opens a native Windows Yes/No question naming the sound. No is the default, Escape cancels, and only IDYES writes the exclusion. Repeated keydown is ignored; text-entry fields retain normal editing behavior. Favorite rows direct the user to the existing Space shortcut or the original sound category.

SF2 identity includes its bank and program, so one exclusion cannot remove other presets from the same file. Portable-relative paths and stable bank:// IDs survive a portable-folder move. Imported external files use absolute references. Saved Performance identities use name, source instruments, zones and transposition instead of volatile library indices; changing live mix volumes does not resurrect an exclusion. Performances with identical identity share an exclusion even across profiles.

The registry is hidden_sounds.catalog in the application's existing roaming settings directory. Writes use the existing atomic file helper; the in-memory filter changes only after the write succeeds. Loading is validated with a 16 MiB size limit, and a damaged registry disables new exclusions rather than silently replacing it. Missing registries mean an empty exclusion set.

Only category catalog rows are filtered. Original sample files, preset definitions, saved show profiles, favorites, and Performance parts remain intact and playable; the dialog states this scope explicitly. The Performance Editor's source selection remains available for maintaining saved performances. Exclusion does not unload the current synth, stop audio, or auto-load the neighboring sound; subsequent manual selection loads the next desired sound normally.

The target key/name are captured before entering the confirmation loop, so a MIDI-driven change of selection cannot retarget the deletion. The same command is supported by the Vision bridge and native fallback. No change is made to the audio engine, ASIO or knob mappings.

Validation: local persistence tests cover preset isolation, restart, portable relocation, external references, Performance identity, duplicate exclusions, failed writes, corrupt load handling and preservation of original bank bytes. Vision command parser bounds/text validation and JS syntax checks passed. The Windows production Vision EXE and native/Vision test harnesses compile; dialog interaction and Windows hardware validation remain untested. No new distributable was uploaded during this source stage.
