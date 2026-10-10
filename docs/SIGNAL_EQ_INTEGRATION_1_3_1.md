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
The EQ native test enters each bank through the integrated path, edits all 24
nodes, checks graph drag/wheel host gestures and stereo FFT, and verifies visible
controls after host updates, A/B, factory recall and project restoration.

Artifacts include JUnit, original logs, EQ timing CSV, 16 route/scale integration
screenshots, four EQ scale screenshots and Standalone preview binaries. Existing
full-product and CAB workflows remain enabled with their original limits.
Do not substitute old #37/#38 results for results on this integrated source.

At authoring time the local native build and new-head CI are pending. Exact
results and workflow links will be recorded in the integration PR description.
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
