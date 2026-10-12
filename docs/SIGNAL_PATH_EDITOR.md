# Signal-path navigation and editing

Updated **2026-10-12 KST**. Basic navigation and AMP/RIGS startup are delivered
in 1.3.1-beta.1. The owner's new feedback requires a simpler presentation.
The October follow-up below is planned and not implemented by this document.

## Product behavior

The default strip contains **five primary editing areas** in actual order:

| Order | Visible block | Action |
|---|---|---|
| 1 | PRE | Open the pedal board |
| 2 | RIGS | Open the current Classic/Dual/Matrix rig view |
| 3 | TONE EQ | Open the EQ before POST |
| 4 | POST | Open the effects rack |
| 5 | FINAL EQ | Open the EQ after POST and stereo utilities |

Use the existing top-level utility controls for Gate, Transpose, Tuner and
Click. Do not repeat these, INPUT/OUTPUT meters, or DOUBLER/CLICK as permanent
blocks in the basic strip. Current sound-changing utility settings must
remain visible and accessible in their own controls.

Place a clearly labelled Details control outside the audio blocks. The first
expanded view shows meaningful amp/CAB lanes and merge points. Further detail
can expose utility processors, detector/tuner taps, gain stages and alignment.
Keep presentation hierarchy separate from real DSP order. This is navigation;
opening a block must not toggle bypass or change any parameter.

Show selected and enabled/bypassed state clearly, with readable text and
keyboard focus. Use actual model names inside the rig view rather than
compressing every technical stage into the compact row. Do not use PATH+
or invented future processors as if they were audio stages.

Open a new editor on **RIGS / amplifier view**, not PRE. Restore the saved
routing mode, selected models and sound. Classic displays its existing amp
surface; Dual/Matrix initially use their RIGS overview. Later head focus
opens the selected lane without changing the mode. Opening or recreating the
editor, selecting a navigation icon and resizing must not write audio
parameters. Project/preset restoration and deliberate knob or bypass edits
retain their normal state and parameter behavior.

The compact strip may fold the rig section into one RIGS block. Expanding it
must reveal Classic's single rig, Dual's two paths or Matrix's crossover and
LOW/MID/HIGH paths. Show the LOW clean-DI branch separately from AMP/CAB and
its actual blend point. The later multiband dynamics module is a separate
global processor, not another label for Matrix.

## Stages and timing

| Stage | Window | User can do | Completion condition |
|---|---|---|---|
| Startup and basic navigation | Delivered in 1.3.1-beta.1, 2026-10-12 | Start on AMP/RIGS and navigate implemented areas | Preserve shipped mode/state and correct lane focus |
| Compact presentation and hierarchy | 1.3.1 follow-up, 2026-10-12–11-01; first installer 10-23 | Use five primary blocks; open rig topology or technical details only when needed | Actual order remains accurate; readable labels and keyboard focus; no audio writes from navigation |
| Room path integration | 1.3.4, 2027-02-22–03-19 | Show weighted AMP/CAB sends and shared Room; open its controls | Preserve LOW DI isolation and the actual post-rig gate/wet-return boundary; display implemented topology only |
| Constrained order editor | 1.4.0, 2027-03-22–04-30 | Move supported stages earlier/later using commands or a placement menu | Correct audio graph, click-safe change policy, stable automation identities, order save/restore and basic undo |
| Drag interaction | 1.5.0, 2027-05-03–05-28 | Drag with valid drop targets, preview placement, cancel, use keyboard and undo/redo | Calls the already tested order command; no DSP rebuild during hover; controls remain accessible |
| Advanced polish | 1.9.0, 2027-09-13–10-08 | Expanded modulation/routing presentation and qualified advanced placements | Separately measured branch/latency/tail behavior; no unrestricted cyclic graph |

Do not postpone undo, preset recall or stable IDs until the drag release.
They are required when order changes first become possible. An unavailable
future processor may be documented in the roadmap, but must not appear as an
active audio block in the shipping product.

## First editable regions

| Region | Initially supported | Fixed boundaries |
|---|---|---|
| Input gate / transpose / tuner | Use existing utility controls; inspect topology in Details | Preserve input/tap relationships; no duplicate basic-strip nodes |
| PRE board | Reorder installed pedals within their current supported owner/board | Maximum five pedals; no implicit change of lane ownership |
| Amp and CAB | Select models, use controls, inspect existing bypass and routing | Amp-to-CAB relationship and LOW DI routing remain explicit |
| Room send/return, from 1.3.4 | Inspect the weighted sends and shared room; open its controls | Keep send taps and wet return fixed in the first order editor; wet return follows the optional post-rig gate |
| Common pre-POST block | Reorder Tone EQ, clean dynamics and multiband when implemented | Stay after rig merge, the optional post-rig gate and Room return, and before POST in the first release |
| Existing POST slots | Reorder supported modules, including delay/reverb order | Preserve each existing instance ID and declared transition policy |
| Final block | Set Final EQ, output trim and limiter | Final EQ → trim → limiter; no audio gain stage after limiter |

The first order editor is a set of useful serial regions. Moving processors
across rig splits, adding arbitrary parallel buses or drawing feedback loops
is outside its initial scope. An advanced whole-wet compressor placement can
be introduced later as an explicit supported preset/position, with tail and
level consequences visible.

## State and automation

Give each processor instance and EQ band a persistent identity. Save the
order separately. Moving a block must not exchange two parameter banks or
redirect existing automation to another model.

New state fields and parameters are append-only. Old state loads the old
audio order with new processors bypassed. UI page, selected graph node and
viewport are presentation state and must not silently change a preset's
audio content. Keep A/B, undo and full-state signatures consistent with
the restored routing.

Tone EQ and Final EQ each have twelve total nodes. Dynamic mode can use a
maximum of four active nodes combined across both instances in the initial
release. Display the shared capacity, and refuse an extra activation clearly
without disabling another node. Band position in the graph is not a host
parameter ordinal.

## Real-time graph changes

Prepare routes and resources outside the audio callback; publish a bounded,
validated change at an appropriate boundary. Keep ownership and cleanup off
the callback. Dragging a preview must not repeatedly instantiate processors.

Use a documented fade/crossfade policy for supported live serial changes and
measure its overlap cost. Delay/reverb bypass and relocation are different
operations. Tail-preserving bypass does not imply fully seamless tail
relocation. The first reorder release guarantees its tested click-safe
transition; do not promise complete preservation of a tail across every new
route. Changes that alter graph latency may wait for transport stop, with an
explicit pending state, until safe live PDC behavior is verified.

Report the actual latency to the host and align internal dry/wet and parallel
paths. Off lookahead/oversampling is a configuration, not proof that the whole
plugin has zero latency.

## Detector paths

Start with bounded, named sources: module input, clean DI tap and rig-merge
tap. Treat sidechain wiring separately from the audible path and prevent
accidental dependency cycles. A clean DI detector used on a post-gain gate
must be aligned with the processed signal, including when Transpose is active.
External DAW sidechain and M/S each require independent mono/stereo, missing
input, bus-layout, automation and restore acceptance before exposure.

## Visual and interaction details

- Keep the basic strip to the five defined editing areas. Use readable labels,
  selected-state highlighting and an accessible active/bypass indication.
- Group secondary details without losing real branch relationships. Matrix
  LOW must still show clean DI alongside AMP/CAB and their actual blend point
  when the rig topology is expanded; do not flatten it into a false serial path.
- Keep actual processing order distinct from tab order. The amp is the startup
  view even though PRE remains before it in the signal path.
- Show valid drop positions before committing; an invalid drop leaves sound
  and saved state untouched.
- Give room heads and cabinets separate focus targets without intercepting
  microphone drags or obscuring the existing LOW DI blend.
- Reuse native bindings and the head-control sizing plan. Dense heads have an
  enlarged view and a complete detailed-control fallback.
- Use dirty/visible-only updates. Static path rendering does not require a
  new whole-window 60 Hz timer.

## Acceptance

Inspect Classic, both Dual modes and all Matrix lanes against actual processor
routing. Verify that opening/recreating the editor, navigation-only clicks,
resizing, cancelling a drag and closing a view produce no audio parameter
writes. Verify supported reorders with existing
automation, preset/A-B roundtrips and undo, and test invalid/stale route changes.

Check Mono/Stereo, representative sample rates/buffers, enabled Transpose,
active delay/reverb tails, wet/dry blends, latency-changing quality modes and
editor removal/host shutdown. Use meaningful existing tests and real pointer/
keyboard/host checks; do not present static screenshots as interaction proof.

This specification follows the [full reordered roadmap](DEVELOPMENT_ROADMAP_2026_2027.md).
The initial strip and embedded head controls are delivered in 1.3.1-beta.1.
The simplified hierarchy and later order editor remain planned follow-up work.
Inspect actual source and native interaction before marking either complete.
