# Small-screen editor — Issue #113

Baseline: main `f09eae237bfa79fe3548d5dcb6105cfddff21574`, including PR #114.

## Behavior

- The 1180 × 780 design canvas retains a minimum 75% text/control scale.
  Its containing window may be smaller, with 16 logical-pixel scrollbars.
- Fit screen and Standalone maximise/F11 fit the **window** to the current
  display's work area, accounting for JUCE/native decorations. Space that
  cannot fit at 75% is scrollable, not rendered at a smaller scale.
- Selecting 75/100/125/150% explicitly preserves that readable scale and uses
  scrolling if necessary. Fit screen returns to automatic sizing.
- PRE → HEAD → CAB → TONE EQ → POST → FINAL EQ, ROUTING and SETTINGS retain
  their existing actions. Wheel/trackpad and scrollbars pan the editor;
  dragging a knob does not pan. Keyboard focus reveals off-screen controls.
- Artwork, hit tests, IR drop coordinates and meter invalidation share the
  same transformed, scrolled canvas.
- Oversized HEAD/CAB, native AMP/POST detail, routing and settings dialogs
  retain their layout inside an owning viewport. Native file choosers remain
  the responsibility of the host OS.
- F11/Esc restores normal dimensions, requested zoom and scroll position.
  A removed/smaller monitor bounds the restored window to a reachable area.
- No DSP, parameter IDs/order/ranges, state schema, presets, ASIO configuration
  or audio processing code is changed. Scroll/zoom values are editor-only.
  No new fields are written into a host project or preset.

## Six direct dynamics entries — 2026-10-24 KST

The upper 230 × 130 navigation area now uses a two-column, three-row grid:
Gate / Expander, Compressor, Limiter, Dynamic EQ, Multiband and De-esser.
Gate is visible in the first cell and opens its existing editor with one click.
Each cell selects an editor; its highlight represents the current page, not
the processor's enabled/bypassed state. Internal dynamics tabs also update
the matching header highlight.

Tone EQ and Final EQ retain their existing signal-path entries. Their
redundant upper bank buttons are removed. Dynamic EQ remains one of the six
existing dynamics functions and reuses the same EQ editor/analysis ownership
handoff. PRE, POST and Matrix LOW compressor controls stay in their existing
positions. DSP, parameter/state schema, presets and release gates are unchanged.

Regression coverage now opens all six direct entries in every small-screen
matrix case, checks the selected page, exclusive highlight, non-overlap,
strict hit test and zero parameter/gesture writes, then opens both EQ banks
from the signal path. Existing Gate tests require one-click access; dynamics
tests cover direct entry and inner-tab synchronization at 75/100/125/150%.
Graphical EQ tests use the retained signal-path bank entries.

Implementation baseline: main `71595636dd21855b702488f70b94bddebabc5b27`. These are
test changes, not a claim of execution success. The local execution environment
was disconnected when this follow-up was prepared; new-source build/runtime
results must be read from its PR CI, separately from PR #121's passing results.

## Automated evidence

`ChimeraSmallScreenUITests` runs in the existing Windows/macOS/Linux native
navigation workflow and in the full CTest build. Its output includes
`small-screen-matrix.csv` and screenshots for each of six main pages at each
resolution/DPI combination in Classic mode.

| Physical pixels | Simulated OS scales | Routing modes |
|---|---|---|
| 1280 × 800 | 100%, 125%, 150% | Classic, Dual, Matrix |
| 1366 × 768 | 100%, 125%, 150% | Classic, Dual, Matrix |
| 1920 × 1080 | 100%, 125%, 150% | Classic, Dual, Matrix |

The 27 cases convert physical pixels to logical coordinates, reserve a
40-physical-pixel taskbar and a 16 × 64 logical-pixel frame/notification
allowance. They check window bounds, minimum scale, scroll endpoints, actual
component hit testing, synthetic mouse navigation, SETTINGS popup opening,
keyboard focus reveal, readable 100% zoom, view-size/scroll restoration,
editor reopen, and zero parameter writes/host gestures during navigation.
Dialog tests check retained content dimensions, both scroll endpoints and
ownership on destruction. Existing native-state, parameter/preset, EQ and
head interaction suites remain enabled without weakening their assertions.

These are automated JUCE tests with simulated available work areas. They do
**not** change the runner's Windows DPI setting or prove physical OS/DAW
acceptance. CI result URLs and exact tested commit belong in the PR.

## Physical acceptance still required

On Windows and macOS, record OS/build, display/scaling, host/format and source
SHA. At each applicable resolution/scale, exercise title-bar maximise, F11,
Esc, all six pages, SETTINGS, HEAD/CAB enlargement, scrollbars, wheel and
trackpad, Shift fine adjustment, typed values, Tab/Shift-Tab and popups.
Move between monitors of different scales, disconnect the original monitor,
restore normal bounds, close/reopen the editor and recall a saved project.
Verify automation gestures and parameter values in a real DAW. Report any
host that rejects requested editor resizing separately from Standalone.

No physical Windows/macOS or DAW result is claimed by this document.

## PR #121 / #122 integration — 2026-10-23 KST

Integration baseline: main `11c350721d652bfba53bdfa24f201b6e70a29483`,
which already includes PR #122 (`bb0362c3b798a760bfab7646211e7cbe2b943ca4`).
Viewport implementation retained from PR #121
(`694e9f0a60b97a8ca611152882acd5a42ecf4ee7`).

The seven overlapping files retain #122's top-bar workspace entries and
PRE/POST/Matrix LOW controls, with #121's recursive canvas discovery and
viewport geometry. Tests unwrap owned dialog viewports before checking
the original panel type, style, contents and teardown. Geometry assertions
require both ends of a target to be reachable when an OS constrains the
native test window; ordinary non-editor layout checks remain strict.

Every one of the 27 matrix cases additionally checks all three top-bar
entries from the six signal-chain pages, opens each workspace, drags a PRE
and POST compressor control (plus LOW in Matrix), checks balanced host
gestures and host-to-control updates, and restores the exact parameter vector
after factory-preset replacement and binary project recall. The expanded
test alone has a 900-second budget; no release/DSP acceptance thresholds or
other test budgets are changed.

### Existing failures before integration

PR #122's head has 14 successful workflows and one intentionally skipped
preset-publishing workflow. This is baseline evidence, not an integration pass.

PR #121's head already failed native dialog discovery (POST, AMP, CAB and
routing wrappers), Linux whole-window bounds assertions, and the macOS
SETTINGS popup check. Logs:
- Signal/UI matrix: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39847257343
- Dynamics: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39847257467
- CAB UI: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39847257414
- EQ: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39847257415

The existing macOS realtime run also failed its CAB callback budget in both
the fixed baseline and candidate. This remains separate from UI acceptance:
https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39847257362

### Integration findings — 2026-10-23 KST

At `7e68ee1f745d76b7ea77f93f99ec630be6fd8ad7`, Linux native navigation
passed 15 of 16 tests. The expanded small-screen test stopped at its first
case: its new PRE DETAIL assertion selected display slot 0, although factory
preset 0 puts the filter there and compressor owner 0 in slot 1. The test
now resolves the compressor's display slot from the retained board order and
also requires the opened spectrum to select owner 0. Product navigation and
preset ordering are unchanged. This integration-test defect is separate from
the older dialog/bounds failures and from the missing `rect` compile variable
already restored in `7e68ee1`.

Evidence for `7e68ee1` (not a pass for a subsequent revision):
- Linux native navigation/state: 15/16 passed; small-screen failed as above.
  https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39889149314/job/120204440293
- macOS Dynamics UI: all three tests passed.
  https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39889149372/job/120204440053
- Linux EQ and CAB jobs passed; CAB realtime jobs passed on all three OSes.
  https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39889149411
  https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39889149430
  https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39889149324

Local Ubuntu 24.04 / GCC 13.3 / JUCE 8.0.8 Release builds succeeded for the
Standalone, UI, CAB state and graphical EQ UI targets. Native-state CTest
passed in 19.4 seconds on the corrected `7e68ee1` source tree. Configuration
started at `5d21fbb`; the changed test translation unit was rebuilt, while
the generated manifest retains the configure-time SHA. Native UI execution
is blocked locally because AF_UNIX socket creation is denied and Xvfb cannot
create a display. No local matrix or screenshot pass is claimed.

The corrected small-screen matrix and remaining platform checks require CI.
The simulated matrix does not certify physical OS DPI, mixed-monitor or DAW
automation behavior. Final tested SHA and workflow results belong in PR #121.

### Follow-up CI at `926f787`

Linux signal/UI/state passed 15/16 tests; the small-screen test reached
1280x800 / 150% / Matrix and failed its strict control hit test. macOS passed
13/15: small-screen failed on the second case's SETTINGS popup, and CAB state
failed the old whole-workspace containment assertion. The CAB log shows a
1000x650 workspace inside an intentionally scrollable 1016x634 window; the
native window and outer frame were contained in the available display.

- Linux: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39896012216/job/120227406722
- macOS: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39896012216/job/120227406499

The follow-up test changes preserve strict hit testing and frame containment.
They expose nested panel viewports before the editor viewport and log the
target/hit identities if a hit still fails. SETTINGS checks command delivery,
modal opening and completed dismissal separately, with the same bounded wait
budget. CAB checks the owned viewport's containment plus reachability of both
workspace endpoints. The Linux hit-test cause and macOS popup resolution
remain subject to the next runtime CI; these changes are not a claimed pass.
No product, DSP, parameter or acceptance-threshold change is made.

Dynamics, amplifier head controls and CAB realtime jobs passed on all three
OSes at `926f787`; lifecycle runtime jobs also passed on all three. Full build,
remaining signal UI jobs and Windows candidate packaging were incomplete.
The previous local checkout/build cache is unavailable in this session, so
the follow-up tests require CI compilation as well as runtime verification.

### Windows IR library lookup at `64eb433`

The Windows CAB job passed 17/19 tests. CAB state and IR isolation both
stopped at the global IR library lookup: the button listener ran once and
the message-loop fence was delivered, but the test cast the window's direct
content to IRBrowserPanel. On the 1024x720 available desktop this content
is an owning AccessibleViewport. The source-isolation helper now unwraps
that viewport with the existing dialogContent helper at both lookup sites.
The panel-type, source-switch, unrelated-parameter, teardown and project
recall assertions are retained. This is another missed adaptation of the
existing #121 dialog wrapper, not evidence of an audio/state regression.

Evidence (before the lookup correction):
https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39902036955/job/120246047971

At the same source, Dynamics UI, head controls and CAB realtime jobs passed
on Windows, macOS and Linux. The complete small-screen matrix and Windows
installer were still in progress. The lookup correction requires fresh CI;
no local build or runtime pass is claimed for it.

### Full-suite detached-window lookup at `17ab873`

The Windows candidate job passed 72/73 tests, but the combined ChimeraUITests
process crashed during "UI refresh: detached ALL window lifetimes". The
refresh test still cast the dialog's direct content to PostNativePanel and
AmpNativePanel, then dereferenced it without checking. On a constrained
desktop the direct content is the owning AccessibleViewport. Both casts were
also present in original #121 `694e9f0`; this is a remaining test adaptation
to its dialog wrapper, separate from the integration's rect/owner fixtures.

The two lookups now use the existing dialogContent helper and explicitly
require the expected panel before dereferencing. Hidden-parent refresh,
detached-panel updates, model changes and dialog teardown assertions remain.
No product, DSP, state schema, timeout or release gate is changed. Compilation
and runtime of this follow-up require CI; the patch itself is not a pass.

- Windows candidate: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39925966084/job/120322834061
- JUnit/log evidence artifact: 12495708123; SHA-256
  `3024af30949b0f16fad111b80ec3cd831b24a45646ab3fe805e982cebfda38d6`.
- At `17ab873`, standalone native-navigation jobs passed Windows 15/15
  and Linux 16/16. Both matrices contain all 27 required cases and 54 valid
  page screenshots; this does not replace the combined-suite check.
- Linux evidence artifact 12496021468; SHA-256
  `5c5575e994972f9753b1c1ff4840a827f105f174fd882e131156ae10a82b9496`.

A separate macOS tuner worker run failed at 44.1 kHz / 256 samples / 82.406889 Hz:
acquisition 242.801667 ms, settle -1, unavailable error 99999. Its diagnostic
normal-QoS control passed (settle 223.371625 ms), but does not override the
required failing run. This executable exercises GlobalDSP without an editor;
no UI source change is established as its cause. Keep it as an unresolved
required check, retain all timing/accuracy gates and inspect the next run.
https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39925966099/job/120323457072
