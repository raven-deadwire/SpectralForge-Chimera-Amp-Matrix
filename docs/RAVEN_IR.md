# Raven cabinet IR restoration

The Raven captures are Celestion G12-100 Raven cabinet impulses from **The other John Browne**, not the application's two embedded factory impulses. The previous portable update carried neither these WAVs nor their catalog entries, and its personal ZIP importer only recognised the older 26-capture collection.

The library now recognises the original four files by filename and SHA-256:

| Library label | Original WAV |
|---|---|
| Raven G12-100 — SM57 In | `Mar1960_Raven_SM57_In.wav` |
| Raven G12-100 — SM57 Out | `Mar1960_Raven_SM57_Out.wav` |
| Raven G12-100 — SM57 Ref | `Mar1960_Raven_SM57_Ref.wav` |
| Marshall 1960 V30 — SM57 comparison | `Mar1960_V30_SM57.wav` |

All four are unchanged mono PCM24 WAVs at 48 kHz, 9,601 frames (approximately 200.02 ms). In/Out/Ref are the creator's position labels; no physical distance is inferred. Exact cabinet suffix, source normalization and phase processing remain unspecified. Canonical metadata and original hashes are in `reference/raven-ir-catalog.json`.

The user's private Windows package places these WAVs and metadata sidecars in `Chimera-Personal-IRs` beside Setup. Extract the entire package and run Setup; the companion files are installed to `C:\ProgramData\SpectralForge\Chimera\IRs` and preserved during uninstall. They become selectable through **CAB → IRs** and **IR LIBRARY**. Search for `Raven` to locate the collection.

For an existing installation, extract `Chimera_Raven_IR_20260928.zip` yourself and select its IR folder with **IR LIBRARY → ADD FOLDER**, or add one WAV with **OPEN IR**. The default **Import: keep existing type** preserves the catalogued Guitar classification; **Import as: Guitar** can set it explicitly. Original filenames and SHA-256 hashes identify the four catalogued captures. This works independently of the older 26-capture personal pack. **REMOVE FROM LIST** removes a selected imported entry from the library and cabinet menu while preserving the original file and IR audio already loaded in a project/A·B. **OPEN IR** can add the file again.

Source: [The Raven has landed! Celestion G12-100 Raven Demo + IRs](https://www.youtube.com/watch?v=_TzZ_FVMGfg), published September 17, 2026. The creator supplied a public download, but no grant to redistribute the audio in a public product was recorded. Public source/CI contain metadata only; the WAVs remain in the user's private package.
