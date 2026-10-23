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

Integration execution results are pending CI. No local C++ runtime was
available during this integration; source inspection is not a runtime pass.
The simulated matrix does not certify physical OS DPI, mixed-monitor or DAW
automation behavior. Final tested SHA and workflow results belong in PR #121.
