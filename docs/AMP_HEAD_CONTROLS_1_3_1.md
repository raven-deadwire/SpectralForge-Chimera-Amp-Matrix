# Amplifier head controls — 1.3.1 first stage

Base: PR #40, `5712cefa379c38d071ef0a4a6ab97cd3ff1e466a`.
This is a separate development PR stacked on its integration branch. It does
not alter the 1.3.0 preparation branch or publish a release.

## Implemented

- Shared `AmpHeadControlView` with an enlarged native `AmpHeadWorkspace`.
- Niflheimr: all 14 existing controls, five independent channel memories,
  frequency in Hz and blend in percent. Original enclosure/ornamentation is
  preserved; only baked knob disks are replaced with adjacent fascia material
  before live JUCE knobs are drawn at the authored positions.
- Brit Edge: six native controls on its clean gold faceplate and all five
  existing input routes. No unnecessary single-channel selector.
- Classic and focused CAB head clicks and independent Dual/Matrix room head targets open the
  same enlarged view. Cabinet targets retain their existing destination.
  Back/Escape closes the enlargement and leaves the previous view intact.
- Existing detailed controls remain available beside the compact head and via
  DETAILS in the enlargement. Other models open their full detailed panel.
- A/B buttons inside the enlargement select the existing global comparison slots;
  no extra snapshot or per-window state is introduced.
- Controls use existing IDs and `ParameterAttachment`; no parameter addition,
  reordering, DSP change, extra blend, state schema or latency change.
- Closing, hiding, rebinding or changing channel ends active drag gestures.
  A nonserialized recall revision invalidates pending edits after project,
  factory preset or A/B recall, including a late pointer event before refresh.
- All geometry derives from the same cropped front rectangle used by the head
  renderer. Compact heads are navigation surfaces; direct knob operation is
  available in the enlargement. Knobs use native focus, arrow keys, fine drag
  and editable numeric values. UI refresh rebinds only on structural changes.

## Verification

`ChimeraAmpHeadControlTests` requires a real native display. It tests both
models in Classic, Dual Blend, Dual Crossover and all Matrix lanes; pointer
interaction, host notifications/gestures, value conversion, channel switches,
input routes, automation reflection, context isolation, A/B/project/factory
recall, interrupted drags, independent room targets, and editor scales.
It writes actual native component PNGs to `amp-head-snapshots/`.

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
| Nastroend | 13-control face and five-channel layout |
| Cinder 120, Special Edition, Liquid Lead | Dense panels, shared controls, choices/toggles and secondary banks |
| Other active heads | Model-specific clean fascia inventory and native layouts |
| Retired Iron Compact | Recallable ordinal remains supported through full detailed controls |
| Small in-place controls | Compact main/room heads intentionally open the enlargement |
| Production acceptance | Real Studio One automation record/playback, save/reopen, plug-in removal and DAW shutdown; listening and full release/installer gates |
| Performance | Same-machine before/after message-thread profiling with two instances and audio callback timing under UI activity |

The supplied native automation tests exercise JUCE host listener contracts;
they are not a claim that a real DAW acceptance session has been performed.
