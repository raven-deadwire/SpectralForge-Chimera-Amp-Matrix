# Amplifier-head UI review — 11 October 2026

Development: PR #44, stacked on the Signal/EQ integration PR #40. This review
does not change the 1.3.0 release branch, audio processing, parameter IDs or
project schema.

## Complete knob hardware audit — current follow-up

After the Niflheimr size fix, the user requested the same scrutiny for every
other amplifier. All 26 catalog heads were compared with the actual reference
hardware. The renderer now distinguishes skirted, bell, knurled, scalloped,
chickenhead and raised-bar grips rather than changing only a common round
body's material. Modern Bass chooses B7K and DB751 hardware per control.
Fourteen heads receive larger allocations and revised row positions; the
recent Niflheimr size is retained. The enlarged view preserves the existing
minimum drawing area and adjustment-only numeric values.

The [complete knob reference audit](AMP_HEAD_KNOB_REFERENCE_REVIEW_1_3_1.md)
records all model mappings, reference links, version limits and deliberate UI
adaptations. The prior shell artwork and independent layout boundaries remain
in place. Exact-source native CI evidence is recorded in PR #44 after the
combined render, layout and interaction verification.

## Complete enclosure recomposition — current correction

The user rejected the appearance of `9995669df7a0058497f214d7380534cd80ebadcb`
despite its passing native tests. The earlier visual review missed missing
upper corner volumes and a second upright/backward roof shape on bass heads.
Its native captures remain historical evidence, not visual acceptance.

All 26 catalog entries now use newly composed complete RGBA enclosure images,
including the recalled legacy Iron Compact. Each image contains its own roof,
continuous upper corners, casing, appropriate handle and feet. Solid Punch,
Modern Bass, Metro Clean and Taste Punch remain without a top handle. Cinder
retains its red strap and casing. The complete source image is projected once
with uniform scale: no extracted handle, synthetic roof, added feet, cleanup
patch or independent stretching remains. Physical widths and support baselines
continue to establish relative sizes in the cabinet scene.

The source art is ImageGen-created production artwork, not a native screenshot
or a claim of exact reproduction of a commercial amplifier. Its generation
instructions, unchanged PNG hashes and provenance are recorded beside the
assets. `AmpHeadShellMetadata.h` explicitly maps each control coordinate frame
and the original heads' five channel windows. The control frame is an authored
coordinate mapping and is not a physical measurement of the front body.

Enlarged heads retain live JUCE knobs, readable labels and interaction-only
values. Náströnd and Niflheimr have taller blank plates for their two-row
controls. Classic, Dual, Matrix and focused CAB overviews paint the selected
bank's actual knob positions and channel lights using the same hardware
materials; they do not write parameters. An overview state cache avoids
repainting or rebuilding control bindings on idle timer ticks. Cinder's label
and scale ink stays light so it remains readable across the dark brushed-steel plate.

The first complete-shell software capture review caught two additional issues:
Brit Edge's route selector crossed the right gold piping, and dark Cinder labels
lost contrast in the centre of its brushed plate. The selector is shifted left
and widened so the longest route fits its unchanged 16 px font, and Cinder
retains light label/scale ink. The layout contract now includes Brit Edge's
actual selector and route-label plate bounds and uncompressed text width.

The first native room capture then exposed a separate integration omission:
`CabRoomOverview` still painted the bare shell directly, so Dual/Matrix room
heads had no overview knobs. The room now owns one cached overview painter per
lane, refreshed with the corresponding bank and drawn in the shell's existing
opacity layer. A component-image regression covers both modes and all five
visible rigs: knobs differ from the bare shell, value changes affect only the
correct lane, channel lights switch, idle images stay stable, and painting
emits no host writes or gestures. Navigation checks alone had not caught this.

The appearance contract independently decodes every source, checks alpha
bounds and resource identity, compares production paint with a single raw
image projection at three sizes, and checks source immutability and continuous
corner landmarks. The workspace contract also checks actual knob bounds and
label glyph extents inside independently reviewed blank control plates across
all 61 channel states and four workspace sizes. These tests supplement visual
inspection; a passing contract alone does not establish visual acceptance.

New native evidence must identify this correction's exact source and CI run.
The expected 252 PNGs per platform distinguish complete blank-shell renderer
captures from live-control workspace and navigation captures. Software-only
renders, ImageGen artwork and native-peer JUCE snapshots must be labelled
separately. Real Studio One interaction and full product acceptance remain
outstanding.

## Niflheimr knob proportion follow-up

The user identified undersized Niflheimr knobs in the complete-shell result
`7a2ea6a9`. The rebuilt blank plate became substantially taller while the
old 3.2%-of-front-width dial allocation remained. Tests for containment and
label readability had not caught the resulting poor knob-to-panel proportion.

Niflheimr now uses a 5.6% dial allocation. Both rows move upward and farther
apart inside the existing blank plate; no artwork is resized or regenerated.
At a 1440 px workspace this raises the visible knob body from about 32 to
56 px. The same model style applies in compact, room and focused-CAB views.
Labels, adjustment-only values, all 14 controls and five channel memories
remain in place. Existing four-size plate/overlap, readout and interaction
checks must pass for the follow-up source before it is treated as verified.

## Historical handle/roof patch — rejected and superseded

The user identified a further visual defect after the all-model knob review.
The native `71ae9690` captures show duplicated metal handle mounts smeared
across the projected roof, clipped mount fragments left on the front edge,
and carrying straps compressed into a common rectangular proportion. Passing
interaction tests did not establish that this part of the artwork was correct.

The common renderer sampled the centre of the source's upper edge, including
the carrying-handle hardware, and stretched that narrow strip over the roof.
It also cut the handle at the first broad opaque row and ignored the extracted
handle's original aspect ratio. This affects every view using `CabHeadView`,
including compact Classic heads, Dual/Matrix rooms, focused CAB and enlarged
head controls.

The correction separates 22 source-authored handles and their complete mounting
shoes from the casing, preserving each source aspect ratio. The four source
designs without top handles stay handle-free; Cinder keeps its original red
strap within the existing roof budget. The old mounting footprint and its
shadow are cleared using the same casing's local lighting and grain.

Hardware-free casing patches supply detached roof textures. Removing the
source rail's row/column lighting gradient before tiling prevents repeated
highlight bands; the roof receives one overall light gradient. The painted
roof joins the rounded shell shoulders and excludes rack ears. Short shadows
under each mounting shoe replace the large dark oval beneath the whole strap.
Existing physical enclosure dimensions, front-control anchors, knob materials
and interaction-only values are unchanged.

The shared appearance contract checks all 26 entries at three head widths,
source aspect, complete mounts, cleanup boundaries and unchanged surrounding
artwork. It includes independent pixel landmarks for the old Vintage Valve and
Náströnd shadows. Both the head-control and CAB suites run it. The head-control
suite adds 52 production-component captures (26 models at 240/1120 px) to its
existing 200 workspace/interaction/navigation PNGs, giving 252 images per OS.

Software previews and native-peer component snapshots are recorded separately.
The previous 600-PNG native run remains evidence for its exact historical
source; it does not validate this roof/handle correction. Real Studio One and
full product acceptance remain outstanding.

## Raised hardware and all-model expansion

The subsequent user review found that the first live knobs still read as flat
2D dials beside the photographic chassis. The renderer now separates the
mounting shadow, exposed fluted cylinder, raised cap, gasket and bevel. Metal
uses a fixed asymmetrical studio reflection and machining detail; rotation
moves only the engraved marker. Materials are assigned per model. Shared,
bounded supersampled textures avoid generating images on each value update.

All 25 active amps now open live model-specific front controls; retired Iron
Compact retains project-recall support without re-entering the active menu.
Explicit layouts follow each cropped fascia, selected channel masks and existing
parameter keys. Non-knob choices/toggles are placed below the enclosure, keeping
long option labels legible. Named channel controls are generalized, and Náströnd
receives its 13-knob/five-channel layout and bounded baked-knob cleanup.
The original heads’ physical channel lenses are aligned separately to each
cropped artwork, with scaled live faces inside the preserved metal bezels.

Expanding model switching exposed a stale-popup risk: a persistent route menu
could deliver its old selection after a new model was bound. Recreating the
route selector and channel buttons on rebinding cancels their queued callbacks;
the tests require no subsequent parameter writes. All controls retain the prior
interaction-only value display and high-contrast precise-entry editor.

The all-model contract checks 26 catalog entries, 61 channel states and 675
visible bindings at four workspace sizes. Native CI and visual conclusions must
refer to this extension's exact source, not the earlier two-model passes below.

## Previous knob materials and interaction-only values

Follow-up to the native review and the user's visual references:

- Niflheimr uses a fluted dark-metal body, muted nickel rim, engraved scale
  and warm ivory pointer; Brit Edge uses the same hardware construction with
  a brass cap and dark pointer matching its gold fascia. These are live JUCE
  vector controls, not painted controls or generated mockups.
- Knob centres and their labels align. Labels use bold 12–15 px type,
  scaling with the head width, with light ink on Niflheimr and dark ink on
  Brit Edge. The cleaned original enclosure artwork is retained.
- Idle controls show their parameter name, without a permanent numeric value.
  Dragging temporarily replaces that name with the value in the same space;
  releasing restores the name without moving the knob. Keyboard adjustments
  show the value for 800 ms. Automation alone does not show the readout.
- Double-click, Enter or F2 opens precise entry in the same readout. Commit,
  cancel, hiding or context changes return to the idle presentation. Text entry
  survives normal workspace refreshes. Existing host gesture and parameter-bank
  protection remain in place. The detail panel keeps its permanent values.
- Native Windows review also caught white editing text on Brit Edge's pale
  brass background. Precise entry now has an explicit dark background, cream
  text and contrasting selection on both heads, with editor insets sized to the
  compact label. The contract checks normal and selected text contrast.

The focused contract additionally checks actual value-label visibility, name
visibility, complete glyph widths, stable knob geometry, keyboard expiry,
precise-entry persistence/cancellation and context interruption. Idle and active
captures cover all four workspace sizes, plus `2500 Hz`, `100%` and text entry.
Native validation must refer to the follow-up commit's own CI run; historical
results below do not validate this follow-up.

## Evidence reviewed

- Source: `306fc797ecee93cf4aec1a60aa50ac3ecbf7c35f`.
- Run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38119085085
- Windows artifact `11694820871`, macOS `11695020182`, Linux `11694870365`.
- Downloaded all three ZIPs and matched their SHA-256 values to GitHub's
  artifact digests. Each contains 26 PNGs, native-test JUnit/logs and build identity.
- These are production JUCE component snapshots attached to native peers.
  They are not desktop/DAW screenshots and are not artwork mockups.

## Findings and changes

| Finding in the original captures | Correction |
| --- | --- |
| Niflheimr has dark crescents and bottom-row fragments around its live knobs. The elliptical mask does not cover the complete baked disk/shadow. | Replace the complete control field, including upper chrome rims, with cached, edge-blended clean metal from the same fascia band, retaining the jack, light, switch and ornate perimeter. |
| At the 1000-pixel popup width, MID FREQ displays `405 ...`; the maximum `2500 Hz` is even wider. The label/value look uses a small 10 px font. | Widen the seven-column control field, retain at least 32 px knob allocation, increase labels/value font to 11 px and reserve more width for values. |
| Physical channel buttons show only 1–5. The current name appears in the caption, but other voices require hovering or trial selection. | Add five named channel buttons, synchronized with the existing physical switches and parameter banks. |
| Brit Edge's five-input selector has no functional label. | Add INPUT ROUTE above the existing selector without changing route order or DSP. |
| Room heads only acquire an outline on hover/focus; the destination is unclear beside cabinet targets. | Add an OPEN AMP caption on head hover/keyboard focus. |

The Niflheimr 14-control order remains Gain/Bass/Mid/Mid Freq/Treble/Presence/
Depth and Mass/Split/Fang/Fold/Thrust/Blend/Master. Brit Edge retains Presence/
Bass/Middle/Treble/Loudness I/Loudness II on its gold fascia.

## Navigation and verification boundaries

- Classic: compact head -> amplifier workspace -> Back/Escape returns to editor.
- Dual: each room head -> its own RIG 1/2 bank; cabinet targets remain separate.
- Matrix: LOW/MID/HIGH heads -> corresponding amplifier bank. Opening the head
  must not change LOW DI/AMP blend, parameters or host gestures.
- Focused CAB: head -> amplifier workspace -> Back returns to the same focused CAB.
- DETAILS and A/B continue to use the existing detail surface and global comparison.

The original editor-75/100/125/150 PNGs really use those editor sizes. However,
their amplifier popup is display-fitted independently: Windows 1000x678,
macOS 1000x642, Linux 1202x678 at every launch scale. They are not evidence of
four distinct amplifier-workspace sizes or OS DPI settings.

The revised contract additionally inspects 1000x642, 1180x650, 1440x780 and
1680x910 workspaces, including maximum frequency/percentage strings, component
containment, non-overlap and actual value-label font widths. It captures all
five named channels and Brit input routes. Room-head and focused-CAB tests now
retain production callbacks and inspect the actual opened workspace and bank;
the previous room test only checked substituted callbacks.

Local compilation and the new three-platform native CI must be checked for
the corrective commit. The old source's passing run does not validate these
changes. Real Studio One drag/automation, save/reopen, shutdown and complete
product acceptance remain separate requirements.
