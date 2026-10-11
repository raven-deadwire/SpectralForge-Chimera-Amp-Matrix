# Amplifier head controls — 1.3.1 all-model development

Base: PR #40, `5712cefa379c38d071ef0a4a6ab97cd3ff1e466a`.
This is a separate development PR stacked on its integration branch. It does
not alter the 1.3.0 preparation branch or publish a release.

## Implemented

- Shared `AmpHeadControlView` with an enlarged native `AmpHeadWorkspace` for
  all 25 active models and the recallable retired Iron Compact. The retired
  model remains absent from the active menu; its stable project ordinal stays intact.
- Every selected channel exposes all native catalog knobs on its model-specific
  fascia. Native choices/toggles and input paths use a compact row beneath the
  enclosure where they would otherwise crowd the artwork; Brit Edge retains its
  existing on-face input selector. Named channel buttons appear only for multi-channel amps.
- The 26 authored layouts cover 315 knob keys and 92 choice/toggle keys across
  61 channel states (675 visible bindings). Dense Liquid Lead, Modern Bass and
  Metro Clean panels use explicit two-row arrangements. These are authored
  Chimera arrangements, not exact reproductions of each reference's hardware panel.
- Raised knob bodies use exposed cylinder depth, fluted grip, contact and cast
  shadows, separate bevel/cap/gasket and fixed upper-left studio illumination.
  Bakelite, cream, nickel, brass and chrome follow the head's material family.
  The engraved position marker rotates; lighting and shadows do not.
- Static knob surfaces are supersampled and cached by material/output size with
  a 32 MiB cap. FaceLook renderers retain the shared cache only while UI owners
  exist, so bitmap resources are released before plug-in unload.
- Parameter names stay visible at rest. Only an adjusted knob shows its value:
  during pointer drag, for 800 ms after keyboard adjustment, or during precise
  entry. No persistent numeric values are added to other heads.
- Niflheimr: all 14 existing controls, five independent channel memories,
  frequency in Hz and blend in percent. Original enclosure/ornamentation is
  preserved; only baked knob disks are replaced with adjacent fascia material
  before live JUCE knobs are drawn at the authored positions.
- Náströnd: all 13 controls and five independent channel memories, including Hz
  conversion. Its original baked controls are removed from the bounded fascia
  region before the same raised live controls are rendered.
- Brit Edge: six native controls on its clean gold faceplate and all five
  existing input routes. No unnecessary single-channel selector.
- Classic and focused CAB head clicks and independent Dual/Matrix room head targets open the
  same enlarged view. Cabinet targets retain their existing destination.
  Back/Escape closes the enlargement and leaves the previous view intact.
- Existing detailed controls remain available beside the compact head and via
  DETAILS in the enlargement. Chimera global trim/output and routing utilities
  remain available there alongside the complete native panel.
- A/B buttons inside the enlargement select the existing global comparison slots;
  no extra snapshot or per-window state is introduced.
- Controls use existing IDs and `ParameterAttachment`; no parameter addition,
  reordering, DSP change, extra blend, state schema or latency change.
- Closing, hiding, rebinding or changing channel ends active drag gestures.
  A nonserialized recall revision invalidates pending edits after project,
  factory preset or A/B recall, including a late pointer event before refresh.
  Choices/toggles use the same identity guard. Input menus and channel buttons
  are replaced on rebinding, cancelling queued callbacks from the old bank.
- All geometry derives from the same cropped front rectangle used by the head
  renderer. Compact heads are navigation surfaces; direct knob operation is
  available in the enlargement. Knobs use native focus, arrow keys, fine drag
  and editable numeric values. UI refresh rebinds only on structural changes.

## Verification

`ChimeraAmpHeadControlTests` requires a real native display. It retains the
exhaustive Niflheimr/Brit Edge lifecycle cases in Classic, Dual Blend, Dual Crossover and all Matrix lanes; pointer
interaction, host notifications/gestures, value conversion, channel switches,
input routes, automation reflection, context isolation, A/B/project/factory
recall, interrupted drags, independent room targets, and editor scales.
It additionally visits every model/channel, checks all 675 native bindings and
conditional visibility, manipulates each knob/choice/toggle/input path, and
requires balanced host gestures. Four workspace sizes are checked for every
channel; default-channel views at four sizes and other channels at the smallest
size are captured. Queued old-bank route/channel callbacks must cause no writes.
It writes actual native component PNGs to `amp-head-snapshots/` (200 per OS).

The dedicated three-OS workflow builds the Standalone preview and runs the new
head contract and packaging-version contract. The existing signal/EQ, CAB and
full-product workflows remain unchanged and run their own regression suites.
Exact-source logs, JUnit, screenshots and preview
binaries are retained in platform-specific artifacts, including failures.

Local native display startup is blocked by AF_UNIX socket permissions. Local
compilation and display-independent tests are reported separately from CI;
no headless skip is counted as a native UI pass. CI results are pending until
recorded in the PR after the run completes.

## Remaining coverage

| Models / area | Remaining work |
|---|---|
| Small in-place controls | Compact main/room heads intentionally open the enlargement |
| Production acceptance | Real Studio One automation record/playback, save/reopen, plug-in removal and DAW shutdown; listening and full release/installer gates |
| Performance | Same-machine before/after message-thread profiling with two instances and audio callback timing under UI activity |

The supplied native automation tests exercise JUCE host listener contracts;
they are not a claim that a real DAW acceptance session has been performed.
