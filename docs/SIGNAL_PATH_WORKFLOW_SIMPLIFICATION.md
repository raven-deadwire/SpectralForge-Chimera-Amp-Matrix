# Signal-path workflow simplification

Prepared: **October 13, 2026, Asia/Seoul**.

This development change starts from the verified [PR #55](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/55)
source `e7b00cb09c5431d86c03282392eafddbe8edbfdd` on
`integration/beta2-test-20261014`. It belongs to a separate stacked development
PR. Preparing or testing this branch does not update PR #55's head, qualify its
new source for an installer, or approve publication.

The implementation commit and each verification result must be identified by
their own complete source SHA. The earlier PR #55 CI results remain evidence
for `e7b00cb09c5431d86c03282392eafddbe8edbfdd` only.

The initial development revision `e27e60769104c2b5606717fb9f793a4995da3f69`
used a combined current-mode rig button. The revised scope separates **HEAD**
and **CAB** so each editing destination is directly available. Results and
artifacts from that initial revision are historical evidence only; the six-stage
implementation requires its own full source SHA and fresh matching verification.

## Baseline comparison and editing contract

At the baseline, both views read the complete routing snapshot from
`Source/SignalPath.h`. The compact view then builds its own list of buttons in
`Source/SignalPathView.h`; this display list can be simplified without changing
the snapshot or audio processing.

The presentation change also updates the navigation callbacks and transient
selection in `Source/PluginEditor.cpp` and `Source/PluginEditor.h`. Those changes
route HEAD and CAB to distinct existing workspaces. `Source/SignalPath.h`
remains identical to the original verified baseline; DSP processing,
parameter definitions, preset data and state serialization are outside this
change.

| Surface | Baseline | Development behavior |
| --- | --- | --- |
| Compact strip | Ten stage buttons: Input, Gate, Transpose, PRE, Rigs, Tone EQ, POST, Utilities, Final EQ, Output; Gate moves with its configured position. A separate PATH + button opens the graph. | Six editing stages: **PRE → HEAD → CAB → TONE EQ → POST → FINAL EQ**, followed by **ROUTING** to inspect the complete path. HEAD and CAB open their respective editing destinations for the current mode. |
| Expanded routing graph | Individual processors, current bypass/model/channel state, rig branches and sums, tuner/detector/clean taps, utilities, and output. | The complete snapshot, nodes and connections remain available. Gate location, Dual Blend/Crossover and Matrix LOW routing can still be inspected. |
| Existing global controls | Input/output and the top Gate/Transpose controls remain directly accessible; Tuner retains its existing bottom-toolbar control. | Existing locations and behavior are retained. They do not gain another compact stage or another audio-control binding. |

The baseline compact strip did not contain a separate Tuner button. Tuner was
already represented in the expanded inspection graph and by its existing
bottom-toolbar control. This change removes the five unnecessary compact buttons—Input, Gate,
Transpose, Utilities and Output—and keeps Tuner out of the compact inventory.
It also separates the combined Rigs stage into HEAD and CAB.
It does not remove those functions or their detailed routing representation.

The six compact stages summarize the editing workflow. Their connecting
arrows do not assert that no utility processing occurs between those stages.
ROUTING exposes the detailed processing order and tap relationships.

## Navigation, keyboard and accessibility

PRE, Tone EQ, POST and Final EQ retain their existing editor destinations.
HEAD opens the current mode's existing amp controls, while CAB opens the
existing inline cabinet room, where the user can select a lane for detailed
editing. The compact actions do not arbitrarily choose the first lane.
Both remain directly accessible in Classic, Dual and Matrix.
Tone EQ and Final EQ remain independently selectable. Compact selection follows
the existing editing context, including related PRE/POST slots and the distinct
AMP and CAB destinations reached from the detailed graph.
Inactive or bypassed stages remain available for editing.

ROUTING opens the existing non-modal inspection window. Selecting a detailed
node continues to use its existing navigation target. Inspecting the tuner
must not enable it or change its mute state. Inspecting the routing must not
create a host change gesture or write an audio parameter.

The compact control order is the six displayed stages followed by ROUTING.
Tab and Shift-Tab move between controls; Enter and unmodified Space activate
the focused action. Space is scoped to the focused navigation button and is
not registered as a global shortcut.
Keyboard traversal, activation, visible focus, accessibility names/help and
the button roles must match this order. Buttons with stable identities should
retain focus when host automation refreshes the snapshot. The detail graph
retains its full navigation and accessibility targets. Native control checks
do not establish acceptance with an actual screen reader or inside a DAW.

The per-mode editing selection remains transient editor state. This work
does not add a serialized UI preference, alter preset/A-B data, or change what
an old project restores. Startup retains the existing cabinet-room default;
routing-mode changes while viewing rigs also return to that room. The compact
selection now identifies the destination as CAB. Editor recreation retains
this startup behavior. Mode changes while editing PRE, POST or either EQ keep
that global editor and its matching selection; PRE/POST now follow the same
selection rule as the EQ banks.

## Processing and compatibility boundary

`Source/SignalPath.h` remains the read-only routing authority. The detailed
graph continues to show the existing post-rig sequence:

`rig sum → optional post-rig gate → Tone EQ → POST → doubler/click → Final EQ → output trim/tuner mute`

The input gate option, tuner/detector taps, PRE clean tap, Dual branches and
Matrix LOW DI/AMP+CAB relationship remain represented by the same snapshot.
Gate automation therefore changes the detailed graph while leaving the six
compact editing stages in a stable order.

The scope is UI presentation/navigation and its regression coverage. DSP
processing order, parameter IDs and ordinals, host names, preset format,
factory-preset data, A/B state and existing global utility controls are preserved.
No routing reorder, new processor or release-policy exception is introduced.

## Verification record

Results below are pending until attached to the complete implemented source.
Replace a pending entry only after observing that check; source inspection,
synthetic native tests, platform CI and installed-product acceptance are
different forms of evidence.
The development PR's verification section records subsequent results against
its full source SHA; this preparation table does not certify a later build.

| Verification | Required evidence | Status at preparation |
| --- | --- | --- |
| Compact inventory and destinations | Exactly six stages plus ROUTING in Classic, Dual Blend, Dual Crossover and Matrix; no utility duplicates; separate HEAD and CAB destinations; working PRE/HEAD/CAB/Tone EQ/POST/Final EQ navigation. | Pending six-stage implementation-source results |
| Detailed routing | Full node/edge and bypass-state checks with both Gate positions, Tuner/detector taps, PRE clean tap, Dual routes and Matrix LOW; detailed click targets remain usable. | Pending implementation-source results |
| Keyboard and accessibility | Forward/reverse traversal, activation, focus indication and retention, declared accessible names/help/roles, and ROUTING access using the supported native test harness. | Pending implementation-source results |
| State and host compatibility | Zero navigation-induced host writes/gestures; factory presets, A/B, project restore, mode switching and editor recreation preserve existing state semantics. | Pending implementation-source results |
| Visual/UI regression | Native screenshots for every routing mode and supported editor scale; compact strip, expanded graph and established global controls remain legible and accessible. | Pending implementation-source results |
| Exact-source platform CI | All required workflows/jobs on Windows, macOS and Linux, identified by full SHA, run and attempt, with matching artifacts. | Pending new-source CI |
| Installed product and actual hosts | Previous-version upgrade, repair/removal and personal IR/preset preservation; actual DAW click/keyboard navigation, recall and editor/window lifecycle. | Not exercised by this development record |
| Assistive technology | Actual screen-reader and keyboard behavior in the installed Windows/macOS host environments. | Not exercised by this development record |

The signal-path workflow already includes `ChimeraSignalPathUITests`, native
state, CAB state, graphical EQ, Gate and package/version checks; Linux also
includes `ChimeraNativeFocusTests`. Strengthening this coverage must retain the
existing native assertions and deadlines. Existing graphical EQ/CAB/amp UI and
full product tests remain part of platform verification.

## October 14 inclusion decision

The [weekly calendar in planning PR #50](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ceb9c1a9510a733a05618d65f6eecb934b504f86/docs/RELEASE_CALENDAR_2026_2027.md)
reserves October 14 testing, October 15 candidate selection and October 16
`1.3.1-beta.2` publication for utility-display cleanup and weekly publishing.
Those dates are conditional. This separate PR makes the cleanup reviewable
without changing the verified integration candidate.

If this change is selected for beta.2, the selected product source becomes a
new candidate. Follow [the weekly pipeline](WEEKLY_RELEASE_PIPELINE.md) and
[the integration verification sequence](TEST_BUILD_20261014_INTEGRATION.md):

1. Freeze the full source, check its clean tree and matching `VERSION`,
   `RELEASE_CHANNEL` and strict release scope, then configure a fresh build.
   `chimera-build-version.json` must contain the selected full SHA and version.
2. Run the publisher, weekly approval, historical fixtures, version/package,
   consolidation and raw-evidence contract regressions on that source.
   Retain the strict rules, CI ownership map, check inventory and receipt.
3. Require the newest successful exact-source runs and current attempts of
   **all seven** workflows: `build.yml`, `update-candidate.yml`,
   `cab-panel.yml`, `cab-realtime.yml`, `amp-head-controls.yml`,
   `signal-path.yml` and `graphical-eq.yml`. Verify the additional Niflheimr
   audition/stock-preset platform checks inherited from the candidate scope.
4. Inspect the matching candidate artifact, all five binary packages, compiled
   revision, filenames, hashes, original raw evidence, portable payload,
   packaged notes and three update-feed platform entries. Prior-source
   packages or successful CI attempts cannot be relabeled as this source.
5. Complete the applicable installed-product, previous-version upgrade,
   personal-data, UI/accessibility and DAW recall/lifecycle acceptance before
   claiming the affected scope qualified. Record anything still untested.
6. Before public release, recompute acceptance and bind actual publication
   approval to the selected version/SHA and policy/map fingerprints. The
   unapproved weekly receipt remains unapproved during development.

`Source/**` changes trigger the required feature workflows through this PR.
At the baseline, **CAB Panel is PR-only**: `cab-panel.yml` has no manual
dispatch and checks out the PR head. The other six required workflows expose
`workflow_dispatch`. A version/policy-only change or a merge that changes the
tested SHA must be checked for missing exact-source runs; do not substitute an
older-source result. Normal PR runs are sufficient for this UI-source change
and require no workflow edit.

The tested product SHA must remain an ancestor of the eventual default-branch
publication controller. Only a separate weekly approval-receipt change is
permitted after that product SHA is tested. A squash/rebase or another product,
document, policy or workflow change that produces a different selected source
requires fresh matching evidence under the existing contract.

**Current disposition: separate development candidate; October 14 inclusion
is conditional, not committed.** If the required new-source checks or installed
acceptance would compromise that test window, preserve PR #55 and carry this
change to the next weekly slot: **October 21 test build → October 22 candidate
→ October 23 `1.3.1-beta.3` target**, coordinated with the separate EQ work.
Combining the changes still requires verification of the combined source. If
beta.2 itself slips, explicitly remap unpublished version reservations instead
of overwriting a published tag or silently claiming the planned date was met.

This document does not modify the baseline integration record, historical
release documents, product version, release policy, approval receipt or CI
workflow files. Implementation evidence and the outstanding installed/host
acceptance must remain separately reported in the development PR.
