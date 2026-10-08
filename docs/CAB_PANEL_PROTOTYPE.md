# CAB Panel prototype — fixed captured IRs

Stacked on the IR-management implementation in PR #24. No release/version change.
Open **PANEL** in each RIG. Mic A and Mic B have independent cabinet and captured
microphone/voicing choices, project IRs and the existing IR library browser.
The library browser retains Open IR, Add Folder, instrument tags and Remove from List.
Unknown generic imports remain selectable without requiring naming conventions.
Origin-style cabinet labels are filename associations only. The selector preserves
full capture filenames, including Bright/Medium/Dark and prepared Mix labels.
No Position/Distance control or inferred physical coordinates are presented.

## Signal and compatibility

Each lane keeps the existing cabinet position in Classic, Dual Blend/Crossover and
Matrix. Both microphones receive the same lane input, then independently run:
low cut → existing convolution → high cut → gain/polarity → fractional signal delay.
The linear sum is `(1-blend)*A + blend*B`; cabinet bypass wraps the complete pair.
Existing cablow/cabhigh and cabtype automation IDs remain Mic A's IDs. New parameters
append after all existing parameters, with blend=0, unity mic gain, normal polarity
and zero delay, so legacy projects retain the A-only path. Mic B starts Filters only.
Signal delay is 0–20 ms, an explicit offset, never a reconstructed microphone distance.
Gain, polarity, delay and blend use 20 ms ramps; existing IR swaps use 50 ms fades.
No IR phase/time alignment or minimum-phase conversion is applied. Existing JUCE IR
normalization/resampling remains in place; source sample rate comes from its header.

The IR worker prepares and retires both lanes' kernels off the audio callback.
Buffer/delay storage is allocated at prepare time. Parameter pointers are cached.
Mic B convolution is skipped at a settled zero blend. Up to six active mic paths
are possible in Matrix; CPU/DAW acceptance on target hardware remains separate.

Project and comparison snapshots embed each slot's original encoded bytes and
metadata. USER_IRS retains lane 0–2, with optional slot=0 (A) or slot=1 (B);
legacy children with no slot mean A. Removing a file from the user library never
removes loaded audio. An invalid import preserves the prior asset in that slot.
Embedded project/preset export can contain privately licensed audio: this PR's
public tests use only author-generated impulse fixtures and never private packs.

## Validation

`ChimeraCabPanelTests` writes its own mono 48k and stereo 96k fixtures, then executes
the actual decoder and convolution at 44.1/48/96k, mono/stereo and 64/256 frames.
It checks header rate, independent A/B responses, blend, level, polarity, delay,
filters, invalid-import preservation and embedded two-slot restore.
`ChimeraCabPanelStateTests` exercises the production processor in Classic, both
Dual routes and Matrix; project restore after deleting the original files;
comparison snapshot recall; and a software panel snapshot.
Existing ChimeraTests and IRLibraryTests remain regression gates.
`.github/workflows/cab-panel.yml` runs all four on Linux/Windows/macOS. Its explicit
artifact allowlist contains only logs and a synthetic-panel screenshot.

For local user-owned WAV/AIFF verification, use existing `ChimeraValidateExternalIR`
with explicit file arguments. No Origin audio is committed or fetched by CI.
This implementation's synthetic native loading is distinct from testing all 291
Origin WAVs, actual guitar/bass DI listening, native-window UX and commercial DAWs.
