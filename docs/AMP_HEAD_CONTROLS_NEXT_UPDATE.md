# Embedded amplifier-head controls: next update after 1.3

Status: implementation plan, 2026-10-09 KST. The owner requested amplifier
knobs that can be operated inside the head design, after preparing the current
CAB release as 1.3. The next version number and date remain unassigned. This
document does not claim that the feature is implemented or included in 1.3.

Source reviewed: CAB PR #34 at
`40d5e8a5ea466eaccc1044ccefce1bd33e6ac222`. References below describe that
checkpoint; an implementation must reconcile later changes before editing.

## Intended experience

The amplifier face becomes the control surface: the visible knob itself moves
and edits the corresponding amp parameter. Labels, pointers, channel switches
and values belong to the head's faceplate. The existing controls and the new
faceplate show the same state when both are open.

In the room, selecting the head opens an enlarged amplifier view for that
specific rig. Selecting its cabinet continues to open speaker and microphone
controls. The selected rig and channel remain obvious through their ordinary
product names. Classic also provides direct access from its head artwork.
When the displayed head is large enough, its controls can be used in place;
an enlarged view supplies usable targets for compact heads and dense panels.

The October 9 feedback about small equipment beside the preview's 8x10 is a
separate current-release issue. The 1.3 candidate now fits only active room
stacks with one common scale and uses the smaller fixed room allowance because
that overview draws no microphone bodies. Focused CAB retains a fixed
microphone-travel allowance, so placement automation cannot reframe it. The
owner subsequently replaced the unshipped 8x10 choice with a 6x10 at its existing
ordinal. Neither change requires arbitrary per-rig size normalization. A head
close-up is an additional control view; whole-room framing remains a separate
requirement.

| View | Interaction | State |
|---|---|---|
| Classic RIGS | Operate the head face; open a larger face when needed | Classic amp context |
| Dual room | Head and cabinet provide distinct focus targets for Rig 1 / Rig 2 | Existing two Dual contexts, including BLEND / CROSSOVER behavior |
| Matrix room | Head and cabinet provide distinct focus targets for LOW / MID / HIGH | Existing three Matrix contexts |
| Enlarged amplifier | Full faceplate, channel/input selection, direct knobs and precise value entry | Same selected rig/model/channel as its owner |
| Detailed controls | Access controls that need a second bank or a rear-panel section | Same parameter IDs; no duplicated audio state |

Product captions should use the current Chimera model names. The menu should
retain its existing categories and single-channel models should not gain an
unnecessary channel list. Developer identifiers, mapping versions, source
review status and geometry diagnostics belong in development evidence.

## Feasibility from current code

This is feasible within the existing JUCE editor architecture. The head is not
a general 3D mesh: its front remains an axis-aligned rectangle and its roof is
projected separately. A component can therefore place native controls on the
front using the same geometry used to paint it. No new audio engine or
parameter model is required to make the controls function. The substantive
work is model-specific layout, readable interaction sizes and lifecycle
handling across views.

| Existing entry point | Verified behavior | Implementation use |
|---|---|---|
| `Source/CabHeadView.h`: `cabHead::geometry`, `paint`, `View` | Creates a level front rectangle, roof, handle and support feet. `View` currently calls `setInterceptsMouseClicks(false, false)`. | Keep the enclosure renderer; add a separate interactive faceplate component tied to `Geometry::front`. |
| `Source/RasterArtwork.h`: `headStyle`, `art::head`, `RasterBank` | Stable model-to-surface and knob-style mappings; the conventional RIGS head is a raster presentation. | Reuse model materials and knob palettes. Keep artwork ownership scoped to the editor. |
| `Source/PluginEditor.cpp`: `paint`, `layoutControls`, `updateModeUI` | Classic paints a 466×198 head and places `AmpNativePanel` beside it. Dual/Matrix control view paints a 55-pixel-high head above that panel. | Replace these decorative head areas with the shared interactive component and explicit focus entry. The compact 55-pixel area is not sufficient for a full control panel. |
| `Source/AmpNativePanel.h`: `refresh`, `refreshIfNeeded` | Resolves context/model/channel, filters channel controls, creates native bindings and keeps an ALL window current independently of its parent tab. | Share binding and selection logic with the head view instead of reimplementing amp selection. |
| `Source/NativeControlView.h`: `bind` | Uses `juce::ParameterAttachment`, distinct drag and complete gestures, and non-notifying host-to-widget updates. | Reuse the binding semantics; add an embedded presentation mode or factor the binding into a shared helper. |
| `Source/CabRoom.h`: `CabRoomOverview::Rig`, `CabWorkspace` | A complete rig is currently one cabinet-navigation button; focus owns a `CabPanel`. | Introduce distinct head/cabinet targets and an amplifier focus destination without altering audio state. |
| `Source/CabScene.h`: `headImage`, `updatePlacement`, `amplifierBounds` | Focused CAB already lays out a head using the cabinet's physical camera scale. | Add a head-focus target using the actual front/head bounds; retain microphone event priority. |

The inspected Cinder 120 asset, `Assets/Artwork/acinder.jpg`, already has a
blank metal fascia. It does not require removing a row of painted knobs.
Inventory the other head assets individually. Preserve an available clean
faceplate; separate any baked control imagery only where it exists. Do not
assume all current assets have identical control content or usable panel areas.

The baseline `CabLayoutView::cameraScale` scans all cabinet layouts, including
the 8x10, and all head dimensions even if they are not displayed. This explains
unnecessary global fit constraints, but does not by itself prove a distorted
relative size ratio. Modeled cabinets and captured-IR cabinets in the room are
procedurally drawn from dimensional bounds; they do not use a padded cabinet
PNG as their physical envelope. The 1.3 candidate replaces that global scan
with `CabCameraFraming.h`, `CabLayoutView::stackEnvelope` and an active-rig
common room camera. Extend those explicit view/camera boundaries for head
focus rather than reintroducing a catalog-wide fit or scaling its knobs
independently of the faceplate.

## Parameter and channel contract

Keep the processor as the authority. The presentation layout must not rename,
reorder or recreate parameters, models, channels or presets.

The source contains 26 serialized amp models, of which 25 are active choices;
Iron Compact remains recallable under its existing ordinal. Each future
faceplate needs a layout or an explicit fully functional fallback. This UI
feature must not remove that saved-project path.

The six native contexts are already defined by `ampNativeContext(mode, lane)`:

| Routing view | Context |
|---|---:|
| Classic | 0 |
| Dual Rig 1 / Rig 2 | 1 / 2 |
| Matrix LOW / MID / HIGH | 3 / 4 / 5 |

Resolve every control with
`ampNativeControlID(context, model, controlIndex, selectedChannel)`, using
`selectedAmpModel`, `selectedAmpChannel` and `selectedAmpNativeRoute`.
Model and channel selectors continue through `setAmpModel`, `setAmpChannel`
and `setAmpNativeRoute`. A visual refresh must not call those setters.

Náströnd has five independent channel memories with 13 controls each;
Niflheimr has five with 14 controls each. The reference-informed heads mix
channel-specific and shared controls, represented by control keys and
`channelMask`. Shared EQ/master controls must remain shared when changing
channels. A reusable graphic position may bind a different key after a channel
change, but a presentation position is never a parameter ID.

| Representative head | Existing panel complexity | Coverage needed |
|---|---|---|
| Brit Edge | 6 controls, one channel, multiple input routes | Simple single-row face and input routing |
| Tight 515 | 11 control descriptors; 9 visible in RHYTHM, 7 in LEAD | Shared EQ/power plus channel gain and switches |
| Liquid Lead | 36 descriptors; up to 23 visible in one channel | Dense layout, graphic EQ and grouped controls |
| Cinder 120 | 42 descriptors across four channels; 15 visible per channel | Channel banks, shared/global controls and compact fascia |
| Special Edition | 32 descriptors across five paths; up to 17 visible | Channel-specific controls, shared power controls and choices |
| Náströnd / Niflheimr | 13 / 14 visible controls in each of five channels | Independent channel memory, character row, Hz and percentage controls |

Preserve display conversions as well as numeric range: most hardware
positions display 0–10 over normalized 0–1 values. Náströnd MID FREQ is
300–1800 Hz with an 850 Hz midpoint; Niflheimr MID FREQ is 150–2500 Hz with a
650 Hz midpoint. Niflheimr BLEND displays a percentage. Software input/output
trims remain dB controls. Reading parameter defaults alone must not replace
channel-specific reset values.

`NativeControlView` already activates a native amp only on a user's gesture.
Keep that behavior for older sessions: opening, resizing, focusing or
refreshing a head must not switch processing engines or change the sound.
Preset restore, A/B copy/recall and host automation must update all visible
representations through the existing parameter state.

## Component and artwork design

Add a presentation-only layout table, such as
`Source/AmpHeadLayout.h`, and an interactive view such as
`Source/AmpHeadControlView.h`. These are proposed files, not existing APIs.

Each layout entry should identify its stable model, usable faceplate region,
control key, control kind, normalized anchor/bounds, group, label placement,
visual knob style and presentation order. Resolve keys through the existing
catalog. Keep presentation order independent of the published parameter order;
Niflheimr already demonstrates that separation in `AmpNativePanel::resized`.

Anchors are relative to the cropped, rendered front face, not the original
image canvas or the complete head including roof/handle. Both the paint
position and hit position must be derived from one transform. The existing
CAB workspace already scales its 1040×748 content with a JUCE component
transform; using that hierarchy avoids manually applying a second scale to
pointer coordinates.

Separate the static enclosure layer from dynamic controls. Draw knob bodies,
pointers, switch positions, channel indications and text using native
components or lightweight vector painting. A generated raster can supply
material texture, but cannot supply exact labels, a parameter value or the
moving pointer. Avoid a generic rectangular control card covering the head.

For dense panels, use an authored two-row/grouped face or an explicitly
selected bank within the enlarged head. Controls should remain recognizable
parts of the head. A detailed section can hold software input/output trim,
band tone and routing functions that do not belong to the amplifier's
physical front. Matrix LOW DI compression and DI / AMP + CAB blend retain
their existing role and prominence; embedding head knobs must not apply a
second blend or put a cabinet on the dry DI branch.

Opening amplifier focus should retain the selected rig and view history.
Back/escape returns to its prior view without selecting a preset, channel,
cabinet or comparison slot. The same enlarged component can serve Classic,
Dual and Matrix; three unrelated implementations would create unnecessary
mapping and state risks.

## Interaction, accessibility and lifecycle

Use JUCE sliders/buttons/choices rather than a single bitmap with anonymous
hotspots. Each control needs a stable component identity, accessible name,
keyboard focus, meaningful value and tooltip. Distinguish repeated controls
by rig and channel in accessibility descriptions without filling the visual
faceplate with development text.

Proposed usability acceptance is a minimum 32×32 logical-pixel pointer target
after the complete editor/workspace transform, with a larger target where
the layout permits. This is a design target to verify, not an existing
product guarantee. If targets cannot remain distinct and readable at 75%
scale, head focus supplies the larger control surface. Preserve keyboard
adjustment, precise text entry and fine adjustment. Reset should follow the
existing explicit channel reset behavior.

Transparent gaps, empty enclosure areas and labels must not intercept a
microphone drag or an adjacent rig's navigation. Room head and cabinet focus
targets need independent keyboard access and visible focus feedback.

Pair every begin/end host gesture, including interrupted drags. Finish an
active gesture before rebinding a model/channel/context, hiding or destroying
a view, loading a preset, changing mode, returning to the room, or closing the
editor/DAW. `NativeControlView` currently manages normal drag callbacks; an
embedded implementation must explicitly audit these interruption paths
rather than assume destruction delivers `onDragEnd`.

## Refresh and audio cost

Keep binding, artwork decoding and geometry changes on the message thread.
Reuse the existing dirty-state mechanism in `UIRefresh.h`. Rebuild bindings
only for model/context/channel changes that require a different bank; a knob
value update should repaint that knob's dynamic area. Hidden views defer
structural work until shown, while an independently visible enlarged window
continues to refresh.

The baseline editor samples its UI at 25 Hz and the room at 12 Hz. This feature
does not require a new continuous whole-window repaint or a 60 Hz room timer.
Hold the existing shared artwork resources through the component lifetime and
release them before the host/JUCE teardown, as the current raster bank does.

The audio path, latency, oversampling and CAB kernel generation are outside
this presentation change. UI-only navigation must produce zero parameter
notifications and identical saved audio state. Benchmark message-thread CPU
separately from callback duration; a lower UI percentage does not establish
audio timing non-regression.

## Delivery sequence and acceptance

1. **Inventory and one representative face.** Confirm clean artwork regions and
   map every existing control key. Implement one original amp and one simple
   reference head using the shared binding path. Verify actual knob movement,
   text values and host recording before expanding asset work.
2. **Channel and dense-panel coverage.** Complete both originals, Cinder 120,
   Special Edition and Liquid Lead, then cover every active model and the
   recallable retired model. Exercise shared controls, per-channel memories,
   routes, choices and toggles.
3. **Integrate navigation and sizing.** Connect Classic, room head targets and
   enlarged views. Preserve CAB navigation, LOW blend, microphone placement and
   product names. Verify all supported editor scales and constrained monitors.
4. **Validate the completed candidate.** Run native UI/host tests and inspect
   generated images from the exact candidate. Assign the next update number
   only when the release scope is decided.

| Acceptance area | Evidence required before calling it complete |
|---|---|
| Mapping | Every exposed faceplate control resolves to an existing parameter; model/channel keys and legal ranges match the catalog. No duplicate active targets for one visual knob. |
| Direct operation | Actual pointer and keyboard interaction changes the intended existing parameter; rendered pointer and value follow host automation and detailed controls. |
| Routing isolation | Classic, both Dual types and all Matrix lanes bind the correct six contexts; editing one context leaves inactive contexts unchanged. |
| Channel memory | All five channels of both originals retain distinct edits; shared reference-head controls retain their shared identities. |
| State compatibility | Released parameter identities/order, old sessions, save/restore, factory/user presets, A/B and private IR references pass existing contracts. Merely viewing/focusing emits zero host writes. |
| Gesture lifetime | One begin/end pair per normal edit; no open or nested gesture after channel/mode/preset/view changes or editor/DAW removal. |
| Geometry | Control art and hit targets align after 75/100/125/150% scaling and constrained-window transforms. No overlaps or clipped values. Head/cabinet and microphone proportions remain coherent. |
| Room and CAB regression | Separate head/cabinet navigation selects the correct rig. Mic A/B drag and Shift-distance gestures, room return and LOW DI / AMP + CAB behavior remain correct. |
| Performance | Same-machine, same-build-profile before/after evidence for idle, drag, host automation, model/channel switching, hidden views, two instances and enlarged windows; report UI CPU, paint work, callback p99 and deadline misses separately. Investigate regressions before acceptance. |
| Host check | Windows Studio One: automation record/playback, project reopen, editor open/close, plugin removal after opening the UI and application shutdown. Automated tests supplement this check. |

Extend the current `Tests/NativeUITests.h`, `Tests/UIRefreshTests.h`,
`Tests/UIRefreshProbe.cpp` and `Tests/CabSceneUITests.h` for the new behavior.
Reuse the latter's `HostEvents` and `navigationUnchanged` patterns to catch
notifications that a final value snapshot would miss. Keep numerical binding,
real pointer/keyboard gestures and visual inspection as distinct evidence.

## Source references

- [Head front geometry and resource ownership](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/CabHeadView.h)
- [Current native panel](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/AmpNativePanel.h)
- [Native control attachments and gestures](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/NativeControlView.h)
- [Parameter identity and channel memories](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/AmpNativeParameters.h)
- [Generated native control catalog](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/AmpNativeCatalog.h)
- [Room and focused workspace](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/CabRoom.h)
- [Current RIGS rendering and layout](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/PluginEditor.cpp)
- [Current camera fit](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/CabLayoutView.h)
- [Captured-cabinet dimensional rendering](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/Source/CapturedCabArt.h)
