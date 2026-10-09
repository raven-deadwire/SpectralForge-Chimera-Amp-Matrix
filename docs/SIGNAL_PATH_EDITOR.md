# Signal-path navigation and editing

Updated 2026-10-10 KST. Planned from the owner's connected-icon SIGNAL PATH
reference and request to open Chimera on the amplifier head.

## Product behavior

A persistent strip shows the signal path and the selected editing area.
Clicking an amp opens its controls; clicking a cabinet opens that rig's
speaker/microphone controls; selecting PRE, POST, EQ or dynamics opens its
corresponding editor. Show real enabled/bypassed state and meaningful model
names. Navigation must not modify audio state.

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
| Startup correction | Immediate 1.3 follow-up | Start on the amp/RIGS view | The prepared follow-up correction must pass UI validation with existing state/mode preserved |
| Navigation strip | 1.3.1, 2026-10-10–11-01 | Inspect actual path and click each implemented stage; see active/bypassed/selected state | Display is derived from actual routing, no fictional editable blocks, keyboard navigation and correct lane focus |
| Constrained order editor | 1.4, 2026-11-30–2027-01-08 | Move supported stages earlier/later using commands or a placement menu | Correct audio graph, click-safe change policy, stable automation identities, order save/restore and basic undo |
| Drag interaction | 1.5, 2027-01-11–02-05 | Drag with valid drop targets, preview placement, cancel, use keyboard and undo/redo | Calls the already tested order command; no DSP rebuild during hover; controls remain accessible |
| Advanced polish | 1.9, 2027-05-10–06-04 | Expanded modulation/routing presentation and qualified advanced placements | Separately measured branch/latency/tail behavior; no unrestricted cyclic graph |

Do not postpone undo, preset recall or stable IDs until the drag release.
They are required when order changes first become possible. An unavailable
future processor may be documented in the roadmap, but must not appear as an
active audio block in the shipping product.

## First editable regions

| Region | Initially supported | Fixed boundaries |
|---|---|---|
| Input gate / transpose / tuner | Inspect and use existing controls/bypass | Retain input/tap relationships |
| PRE board | Reorder installed pedals within their current supported owner/board | Maximum five pedals; no implicit change of lane ownership |
| Amp and CAB | Select models, use controls, inspect existing bypass and routing | Amp-to-CAB relationship and LOW DI routing remain explicit |
| Common pre-POST block | Reorder Tone EQ, clean dynamics and multiband when implemented | Stay after rig merge and before POST in the first release |
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

- Use recognizable stage/model names, a selected-state highlight, connected
  arrows and an accessible active/bypass indication.
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

This specification implements the navigation/editing portion of the
[full reordered roadmap](DEVELOPMENT_ROADMAP_2026_2027.md). It does not claim
that the strip, embedded knobs or order editor already exist in 1.3.

