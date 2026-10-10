# Chimera 1.3.1 signal path / graphical EQ integration

## Source and release boundary

Integrated inputs:

- PR #37: `7865b078c26e83b727bd31ffee2d639985f54956`.
- PR #38: `99a6bc3651488b2596e5df14686f6c002ac174cb`.
- Shared 1.3.0 preparation base: `b7e4f18efa295f0babf2278ca63117fbccb54143`.

The integration is on `codex/1.3.1-signal-eq-integration`. Product identity is
1.3.1 Preview, inherited from #38. The release/1.3.0-preparation branch and #35
are not modified. The integration PR targets a separate `develop/1.3.1` branch
anchored at the shared base. It must not be merged into a 1.3.0 candidate. Reconcile
the final 1.3.0 release changes into development before eventual 1.3.1 promotion.
Neither this work nor its automated tests approves a public release.

## Common-file resolution

| File | Resolution |
| --- | --- |
| CMakeLists.txt | Preserve both native EQ targets and the signal-path CTest entry; same product/editor library under test. No production DSP target removed. |
| Source/PluginEditor.h | Preserve GraphicalEQPanel members alongside SignalPathView, dialog lifetime and per-mode navigation selections. |
| Source/PluginEditor.cpp | Resolve both textual conflicts by retaining tab selection bookkeeping, EQ panel creation/bounds, and persistent signal-path bounds. Add explicit Tone/Final destinations, focus and selected-bank outline. |

The PRE description previously intersected the new path strip; it now uses the
unused right-hand area of the PRE routing row. Global EQ selection follows mode
changes while its editor is visible. Navigation never creates a host gesture or
writes an audio parameter. Both banks remain accessible together on the EQ page.

## Route contract

All four routes (Classic, Dual Blend, Dual Crossover and Matrix) end with:

`rig sum -> optional post-rig gate -> Tone EQ -> POST rack -> doubler / click -> Final EQ -> output trim / tuner mute`

The input-position gate and tuner detector remain where they were. Tone EQ is
not the older POST rack EQ, and neither new bank is a per-lane EQ. Matrix LOW
still follows the clean PRE tap through LOW tone/compression, then splits into
latency-aligned DI and AMP -> CAB. The linear DI/AMP+CAB blend precedes LOW lane
level/mute/polarity and the rig sum. CAB microphone blend stays inside CAB.

Both compact and expanded paths expose the two new banks and independently read
bypass and enabled-band count from APVTS. Expanded graph dimensions include the
additional output row. No routing mutation or drag reordering is implemented.
The expanded graph fits its logical canvas, including connections and native hit
targets, to the actual dialog content bounds. This prevents a window manager's
small-display constraint from clipping Final EQ, output or the right-hand lanes.

## Compatibility

Integration changes do not alter #38's DSP, parameter definitions or migration
code. The existing AMP/CAB/PRE/POST engines remain unchanged from the common
1.3.0 base. #38 appends 122 parameters for two independent 12-band banks; old
projects and factory recalls default both banks to bypass. Existing IDs,
ordinals, project format, CAB assets and A/B state handling are retained.

The DSP contracts exercise mono/stereo at 44.1/48/96/192 kHz, all four routes,
24-node automation, exact settled bypass, project/A-B audio restoration and
missing-EQ legacy state. This is not a claim that a real Studio One project has
already passed acceptance or that 1.3.1 can round-trip into an older binary
without losing new EQ fields.

## Integrated validation

The `1.3.1 signal path and EQ integration contracts` workflow checks out the PR
head itself on Windows, Linux and macOS. Each platform runs seven CTests:

1. ChimeraPackagingVersionTests
2. ChimeraSignalPathUITests
3. ChimeraNativeStateTests
4. ChimeraCabPanelStateTests
5. ChimeraGraphicalEQTests
6. ChimeraGraphicalEQUITests
7. ChimeraGateProcessorTests

The extended native path test covers both gate positions, exact EQ edges,
Matrix LOW branching, all expanded destinations and compact PRE/POST/RIGS/EQ
navigation, zero host writes/gestures during navigation, 48 factory presets,
A/B/project restoration, live bypass/band count, and 75/100/125/150% UI scaling.
It also invokes the existing production LOW audio contract at 0/25/50/75/100%
blend, checking a single linear DI/AMP+CAB mix, microphone isolation at the DI
endpoint and both AMP/CAB bypass behaviors without widening its tolerances.
The EQ native test enters each bank through the integrated path, edits all 24
nodes, checks graph drag/wheel host gestures and stereo FFT, and verifies visible
controls after host updates, A/B, factory recall and project restoration.

Artifacts include JUnit, original logs, EQ timing CSV, 16 route/scale integration
screenshots, four EQ scale screenshots and Standalone preview binaries. Existing
full-product and CAB workflows remain enabled with their original limits.
Do not substitute old #37/#38 results for results on this integrated source.

## Findings from integrated CI

On `fb72d8e`, the EQ-focused run passed all nine CTests across Windows, Linux and
macOS: [run 38057334727](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38057334727).
The macOS integration run passed path navigation, packaging, CAB state, gate and
both EQ tests, but exposed a NativeState assertion comparing a snapped POST
parameter directly with literal `7.f`. The range has a 0.01 interval starting at
-18; fused multiply-add can yield `6.999999523` where separate operations yield
`7.0`. The revised test checks the parameter's canonical requested value and then
requires **exact** preservation of the stored value through A/B and migration.
It does not widen a recall tolerance or change production DSP/state code.

Inspection of that run's actual macOS path screenshots also exposed a graph
clipped to 1000 pixels by the native window. The fit above and containment tests
for default and explicitly reduced native windows address this missed case.
New-head results, including these corrections, are recorded in the PR description.

On `f8a43c0`, Windows and Linux each passed all seven integration tests. The first
macOS 26 ARM64 attempt confirmed the exact canonical value `6.999999523`, passed
NativeState and the production LOW blend (residual `3.72529e-09`), and captured
an unclipped path. It then failed asynchronous dialog deletion, a CAB navigation
callback and FFT delivery. These failures are retained as failed evidence.

The pinned JUCE dispatcher's future `NSEvent` deadline matches the mechanism
reported in [JUCE #1574](https://github.com/juce-framework/JUCE/issues/1574) for
delayed nested-loop event delivery on macOS 26. A test-only Objective-C++ adapter
now polls AppKit without that future deadline while yielding through CFRunLoop
and retaining JUCE's modal-event filter. It is linked only into the three native
UI test executables: no product target or JUCE dependency source is patched.
Dialog deletion and FFT readiness use condition-based waits, with the existing
UI-test convention of a two-second deadline. CAB's two-second deadline is
unchanged and failures now identify the button. Audio tolerances are unchanged.
Artifacts include the run-attempt number so a retry cannot replace earlier
failure evidence. Updated source results must be checked before acceptance.

The separate macOS CAB suite fails `ChimeraCabDriverVisualTests` with
`rail tiling leaked the original cabinet parent bitmap`. The identical failure
is present on the unchanged shared 1.3.0 base in
[run 37998107407](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37998107407)
and on the integrated source in
[run 38057334847](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38057334847).
It remains a separate release-preparation blocker; no gate is disabled here.

Local Linux built the Standalone and native test targets and passed the four
display-independent contracts. Local native UI is blocked because AF_UNIX socket
creation returns EPERM, preventing Xvfb from starting; native CI supplies UI evidence.
Any subsequent failure remains a failure until its cause is understood and a
new source or an explicitly identified infrastructure retry is validated.

## Remaining release acceptance

- Actual Studio One (and other supported DAWs) loading, save/reopen, automation
  write/read, A/B recall, scaling and last-instance/plugin/host shutdown.
- Real guitar/bass listening, rapid switching and workstation timing under load.
- Full-product, installer, signing/security and existing release-policy gates.
- Rebase/retarget against the final approved 1.3.0 source if it changes.

Dynamic EQ, compressor/limiter expansion, signal-order editing and controls inside
amp artwork remain outside this static-EQ/navigation integration.
