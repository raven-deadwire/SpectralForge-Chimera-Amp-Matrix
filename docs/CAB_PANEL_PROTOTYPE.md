# CAB Panel prototype — fixed captured IRs

Stacked on the IR-management implementation in PR #24. No release/version change.
Open **PANEL** in each RIG. Mic A and Mic B have independent cabinet and captured
microphone/voicing choices, project IRs and the existing IR library browser.
The library browser retains Open IR, Add Folder, instrument tags and Remove from List.
Unknown generic imports remain selectable without requiring naming conventions.
Origin-style cabinet labels are filename associations only. The selector preserves
full capture filenames, including Bright/Medium/Dark and prepared Mix labels.
Fixed IRs do not expose inferred physical coordinates. This branch also adds the
separate [Original / Modeled v1 path](ORIGINAL_CAB_DSP_V1.md): guitar 4x12 and bass
4x10, independently selected units and microphones, modeled Position/Distance,
rear structure and optional tweeter. Those controls do not remike a loaded capture.
Enable each modeled slot explicitly in the lower section of PANEL; legacy states
leave both off. Names are provisional and the model is not a measured hardware clone.

The [microphone catalog](CAB_MICROPHONE_CATALOG.md) defines 20 identities: 9
dynamics, 3 ribbons and 8 condensers, including the original condenser Chimera
Strike. Available captures are grouped by microphone family and show their alias
plus full filename; the selected reference appears in smaller text beneath it.
IR LIBRARY can filter all 20 identities, with an Other / mixed / unspecified
route for remaining imports. A filter with no matching capture cannot load audio.
The catalog adds no independent microphone DSP or bundled responses. Loaded
Mic A/B metadata supplies the display after project recall and same-name imports.

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
Mic B convolution is skipped at a settled zero blend after servicing pending swaps; frozen history is reset before waking it. Up to six active mic paths
are possible in Matrix; CPU/DAW acceptance on target hardware remains separate.

Schema 11 exports contain one root `IR_ASSETS` table. Each ASSET stores the SHA-256
of the original WAV/AIFF bytes and one base64 payload. Current USER_IRS and both
COMPARISONS contain `asset` hash references, with independent names and metadata.
Only referenced assets are exported, so replacing a slot does not accumulate old
payloads in the file. USER_IRS retains lane 0–2, with slot=0 (A) or slot=1 (B);
legacy inline-data children and children without slot (A) remain supported.
All hashes, uniqueness, payload bounds and references are validated before any
parameter, MIDI map, comparison or library state changes. The original bytes are
then passed to the unchanged IR decoder; no resampling/normalization rule changed.

`tryGetStateInformation` serializes into a temporary JUCE binary buffer and checks
its actual byte count, including header, UTF-8 XML and terminator, against 64 MiB.
A failure leaves the caller's destination and both comparison snapshots intact.
The JUCE host callback uses the same path; its void API cannot report rejection to
the host. Explicit reference-file export checks the boolean before writing and
therefore preserves an existing file on size rejection. The 4 MiB import limit and
64 MiB read limit remain unchanged. Removing a file from the user library never
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
comparison snapshot recall; and a software panel snapshot. It also requires two
assets for six reused mic slots, six distinct near-4-MiB stereo WAVs shared across
both comparison slots, deleted-source restoration, whole-state rejection of corrupt
hashes/dangling references, and exact serialized overflow with destination and
comparison preservation. Audio equality remains max difference <1e-6 with nonzero
energy in Classic, Dual Blend/Crossover and Matrix, including dirty legacy restore.
Existing ChimeraTests and IRLibraryTests remain regression gates.
ChimeraIRCollectionTests also checks the 20-entry microphone identity contract,
conservative filename matching, metadata precedence and microphone filtering.
The state suite covers same-name Mic B replacement, embedded microphone metadata,
grouped capture selection and empty-filter LOAD safety with synthetic fixtures.
`.github/workflows/cab-panel.yml` runs all five on Linux/Windows/macOS. Its explicit
artifact allowlist contains only logs and a synthetic-panel screenshot.

For local user-owned WAV/AIFF verification, use existing `ChimeraValidateExternalIR`
with explicit file arguments. `ChimeraCabPanelTests user-A.wav user-B.wav`
executes the two-mic contracts with user-owned files. For production routing/restore,
`ChimeraCabPanelStateTests private-snapshot.png user-A.wav user-B.wav` makes temporary
copies and deletes only those copies before project restoration. Keep these private
runs outside CI and public artifact paths. No Origin audio is committed or fetched by CI.
This implementation's synthetic native loading is distinct from testing all 291
Origin WAVs, actual guitar/bass DI listening, native-window UX and commercial DAWs.
