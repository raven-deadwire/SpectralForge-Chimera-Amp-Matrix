# PR #270 — Drop A / Low B frequency-view regression

- Review date: 2026-11-06 KST.
- Reviewed source: `837f95f2b9f596e09fc3fb25fe470e466e6e6b38` (`claude/eq-low-zoom`).
- PR: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/270
- Proposal only: no GitHub commit, push, comment, review resolution or merge is authorized yet.

## Scope and acceptance

Test the real `ChimeraProcessor`, `ChimeraEditor`, `GraphicalEQPanel`, parameter attachments,
EQ edit history and binary serializer. Synthetic JUCE mouse events drive the production
handlers on a native Linux Xvfb/Openbox desktop. No mocked frequency mapper is used.
This is not an actual DAW-hosted mouse/automation test or an instrument-DI listening test.

A0 uses 27.5 Hz and Low B uses the requested 30.87 Hz. The frequency parameter step is
0.01 Hz. Below 100 Hz the cursor deliberately displays one decimal: `27.5 Hz` and
`30.9 Hz`. The rounded cursor string must not rewrite the stored 30.87 Hz parameter.

Expected logarithmic x coordinate, as a percentage of plot width from its left edge:

| Note | Full 20–20,000 Hz | Low 20–2,000 Hz | Bass 20–500 Hz |
|---|---:|---:|---:|
| A0 27.5 Hz | 4.6101% | 6.9151% | 9.8933% |
| B0 30.87 Hz | 6.2836% | 9.4253% | 13.4846% |

Bass view gives approximately 2.146 times the horizontal separation of Full view.
This is an editing-scale change, not increased analyzer FFT frequency resolution or a tuner.

## Existing evidence versus added coverage

| Area | Existing HEAD evidence | Added regression |
|---|---|---|
| Frequency coordinates | 100 Hz in Full/Low/Bass | A0 and B0 in all three views, independent expected coordinates |
| Cursor | 100 Hz string | 27.5 / 30.9 Hz, hover, 20 Hz and the frequency immediately below 500 Hz, empty outside plot |
| Mouse selection | Existing general overlap/modifier tests; 5 kHz and −18 dB edges | Both low notes, off-centre press, adjacent equal-gain A0/B0 in every view |
| Drag | General modifier tests and out-of-view anchors | Low-note free, Shift 1/10, Alt gain/frequency, balanced host gestures, Q/unrelated parameters unchanged |
| Boundaries | 5 kHz right edge; −18 dB bottom | A0/B0 left clamp at 20 Hz and −18 dB bottom; exact 20/500 Hz edges; 5k/20k true anchors and full 20k parameter ceiling |
| View-only state | Binary equality for frequency zoom; stronger assertions for the separate dB view | Every host parameter, complete serialized bytes including A/B, active slot, Undo/Redo descriptions/availability, edit revision and zero host notifications |
| Undo/Redo | General/dB-view edits | One exact undo/redo per Bass drag, switching views with populated undo and redo stacks |
| A/B and save/load | General full-view recall | A0 in A; a real Bass drag to B0 in B; inactive/active slot recall; `.chimera` byte and disk roundtrip in all views |
| Factory presets | General/dB-view recall | Same preset (index 0) from same state yields identical parameters and serialized bytes in Full/Low/Bass |
| Reopen | Documented view-local design | Actual editor destruction/recreation defaults both panels to Full and preserves state |
| Menu safety | Static review flags unchecked 4–99 IDs | Production result handler accepts only 1–3 and 100–102; invalid IDs and integer extremes leave views/state unchanged |

The new `ChimeraGraphicalEQBassZoomTests` CTest entry uses `--bass-zoom` on the existing
UI executable, with a separate 120-second timeout. Existing CI regex
`ChimeraGraphicalEQ.*Tests` automatically includes it. Existing tests and timeouts are retained.

## Minimal production change

`Source/GraphicalEQPanel.h`: factor view-menu result handling into `applyViewMenuResult`.
Only dB IDs 1–3 and frequency IDs 100–102 are accepted. The async callback retains its
`SafePointer` lifetime check and delegates to this handler. Unknown IDs are ignored.

Review: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/270#discussion_r4481971106

HEAD can index beyond `displayRanges` for a hypothetical 4–99 result. Current menu entries
never emit those IDs, so this is defensive hardening, not a confirmed user-triggered crash.
Previously, IDs above 102 also reset frequency view to Full through the setter fallback;
the proposal ignores them. The public setter's existing invalid-index fallback is unchanged.
No DSP, frequency range, preset format or A/B implementation changes are proposed.

## Results

Linux Release, GCC 13.3.0, JUCE 8.0.8, native Xvfb/Openbox: **4/4 passed**.

| Test | Result | Time |
|---|---|---:|
| `ChimeraGraphicalEQBassZoomTests` | PASS | 98.62 s |
| `ChimeraGraphicalEQUITests` | PASS | 186.69 s |
| `ChimeraGraphicalEQEditingTests` | PASS | 5.84 s |
| `ChimeraGraphicalEQTests` | PASS | 19.06 s |

`git diff --check` and application of the complete patch to a clean copy of the exact reviewed source also passed.
Actual DAW operation: **not performed**. No GitHub writes were made.

During test development, the first Tone EQ A0/B0 interaction cases passed. The initial
right-edge assertion incorrectly required the literal `500 Hz` at 0.01 pixel inside
the plot. JUCE formats this as a general floating-point value below 500 Hz (decimal
places zero does not force integer rounding). The test now compares the parsed
readout with the independent logarithmic coordinate formula, within 0.05 Hz. No
product formatting or coordinate code was changed for this correction.

The first build had three zero-byte generated artwork objects and failed to link.
Rebuilding only those generated objects resolved it; no artwork or source changes
were required. CMake was explicitly regenerated before the final CTest run.

A diagnostic binary-recall check found a pre-existing Full-view normalization
roundtrip in `deEsserFrequency`: normalized 0.5 becomes 0.5000000596 (one float ULP).
It is unrelated to EQ zoom. Binary recall therefore requires every Tone/Final EQ
parameter to match its pre-save value exactly, and every host parameter in Low/Bass
to match the complete Full-view restore exactly. View-only operations retain exact
whole-parameter and whole-binary comparisons; no blanket tolerance was introduced.

Upstream baseline evidence, distinct from the local proposal:
https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/42049204546
All three platform jobs succeeded. The inspected Ubuntu log reports 12/12 passing,
including `ChimeraGraphicalEQUITests` at 158.08 s. This CI run does not include this proposal.

## Reproduction

```sh
cmake -S . -B build-eq -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-eq --config Release --target ChimeraGraphicalEQUITests ChimeraGraphicalEQEditingTests ChimeraGraphicalEQTests --parallel 2
python3 Tools/run_linux_ui_tests.py -- ctest --test-dir build-eq -C Release -R '^ChimeraGraphicalEQ(BassZoom|UI|Editing)?Tests$' --output-on-failure --no-tests=error
```

The review workspace needs TCP Xvfb with all clients in one invocation instead of the CI
runner's Unix socket. `run_ui_local.py` in the review package is an environment-only runner;
it is not a proposed repository change. Release optimization, JUCE 8.0.8 and production
source are retained. Synthetic beta qualification is enabled; no distributable release
binary is produced by this review.

## Remaining manual validation

Not performed: actual Windows/macOS/Linux DAW hosting; native popup menu opening/selection
and asynchronous menu dismissal/lifetime events; native SAVE AS/LOAD file chooser; OS mouse
routing and HiDPI; host-written automation during a drag; real Drop A/Low B DI playback.
The tests call the production popup-result handler and serializer, not those native dialogs.
The factory-view equivalence check covers preset 0; the existing editing suite covers all
factory preset migrations but does not combine every preset with every view.

Before release, use the user's Windows DAW to open Tone and Final EQ, select Bass through
the actual menu, select/drag A0 and B0 with normal/Shift/Alt input, test edge nodes,
Undo/Redo and A/B, save/reopen the project and `.chimera` reference, and reopen the editor.
Confirm the internal parameter/slider value retains 30.87 Hz. The visible numeric text
and cursor can both round to 30.9 Hz; that display precision must not rewrite the
internal value. The saved screenshots show a synthetic no-audio fixture, not DI playback.
