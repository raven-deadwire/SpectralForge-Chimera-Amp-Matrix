# Preparation v3 verification — 2026-09-28

Scope: amp-specific panel definitions and silent UI/state, on top of the existing
five-slot pedal preparation. Production Source/, root CMake, host parameters,
installer and release files are not changed.

## Executed locally

Linux x86_64; Python 3.13.5; Node; C++20; system Chromium with Playwright.
Command: `python run_checks.py --browser /usr/bin/chromium`

| Suite | Passed cases |
|---|---:|
| C++ board state | 20 |
| Python assets, catalog and amp descriptors | 76 |
| JavaScript board state | 22 |
| JavaScript pedal control state | 10 |
| JavaScript amp state | 30 |
| Existing pedal browser suite | 20 |
| Amp browser suite | 24 |
| **Total** | **202** |

CTest and C++ -> JavaScript -> C++ .cbp exchange also passed; these reruns are
not added again to the case count. No hardware audio fixtures are included.
The existing 116 checks remain, plus 86 amp descriptor/state/browser checks.

Amp checks include all 23 panel targets, channel-specific visibility and values,
shared EQ keys, per-model/per-lane recall, isolated software trim, Undo/Redo,
actual .calab export/import, malformed-file atomic rejection, unchanged pedal
state, five slots, and 1280/1024/900/640 px layout without horizontal overflow.
Continuous hardware controls retain normalized positions, not measured tapers.
Browser opening of a panel is not a manufacturer or audio accuracy certification.

Screenshots for SVT-VR, Mark IV, ZUTA CH4, Fortin and JTM45 were generated;
SVT-VR and Mark IV were visually inspected. Reports and generated HTML/catalog
are local generated outputs and are excluded from Git.

## Evidence boundary

11 amp targets have core primary-panel review, 7 have partial/revision review,
and 5 remain pending original-panel review. All explicitly have
`production_dsp_connected=false`, `capture_approved=false`, and
`circuit_response_verified=false`. See amp_controls.py and AMP_PREPARATION.md.
Existing pedal evidence remains 19 panel-reviewed and 6 pending.

Not performed: production JUCE UI/DSP connection, real MIDI/DAW automation,
.chimera/APVTS migration, NAM rendering/listening or real IR quality audit,
Windows VST3/DAW validation, product build/release, or hardware response tests.
The new .calab format is a separate amp-preparation JSON state; it is not a
production project or C++ amp-state implementation. The existing C++ tests
exercise the unchanged pedal board core.
