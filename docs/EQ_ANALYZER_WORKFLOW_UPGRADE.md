# EQ analyzer and editing workflow upgrade

Updated: **2026-10-12 KST**. Status: **PLANNED; not implemented by this change**.
The owner reports poor spectrum presentation and insufficient implementation
of the FabFilter reference workflow in the public 1.3.1 beta.
Dates are owned by [the development roadmap](DEVELOPMENT_ROADMAP_2026_2027.md).

## Verified starting point

Source: [main 3817246f](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/3817246f779a50f19b5378ec9661a2275e6b91b2),
the merge of released product 7b20cd9d. This is a code audit, not a new
execution or visual-acceptance claim.

| Area | Already delivered | Remaining work |
|---|---|---|
| EQ engine | Two independent 12-node banks; freely positioned Hz/gain/Q; seven filter types; bypass and automation | Dynamic mode, selectable HP/LP slope and advanced channel/phase processing |
| Editing | Node Hz/dB drag, wheel Q, numeric entry, single selection and overlapping-band selector | Plot-based add/remove, multi-selection, band audition and EQ Undo/Redo |
| State/compare | Stable host IDs, project/preset recall, whole-product A/B and Copy including EQ | Clearly scoped EQ-local comparison/copy and edit history |
| Analysis | Per-bank input/output FFT, log frequency axis, calibrated stereo power average | Resolution/ballistics controls, freeze/peak hold, peak-preserving display and visual hierarchy |
| Layout | Two 560 by 412 panels with roughly 484 by 212 plots | Large selected-bank editing view; optional comparison instead of compulsory split width |

Current implementation sources:
[GraphicalEQ.h](../Source/GraphicalEQ.h),
[GraphicalEQPanel.h](../Source/GraphicalEQPanel.h) and
[PluginEditor.cpp](../Source/PluginEditor.cpp).
Current analysis uses a 2048-point Hann FFT, a fixed -90 to 0 dBFS display,
25 Hz UI updates and nearest-rounded-bin lookup at each display position.
It has no temporal smoothing, peak hold/freeze, spectrum fill or user
resolution/speed/range/tilt controls. At 48 kHz the FFT bin spacing is
23.4375 Hz; interpolation alone cannot create missing low-frequency resolution.

The existing tests check filter response, stereo FFT calibration, parameters,
state and basic native interaction. Their success does not establish the new
visual or editing acceptance below.

## October follow-up: required quality scope

Target: **1.3.1 follow-up beta, provisionally beta.2**.
First installer October 23; validation candidate October 30; scope review
November 1. The released beta.1 remains the historical base.

October 16 is the scope/capacity commitment checkpoint. Commit the October 23
minimum: five-block navigation, focused EQ view, clear filled IN/OUT spectra,
time smoothing, resolution selection, snapshot Freeze and persistent-slot
add/delete with Undo/Redo. Publish the remaining October 30 work explicitly:
multi-selection, audition, EQ-local comparison, peak hold, range/tilt and zoom.
Review variance on October 23; name any changed completion dates before
continuing to advertise the full October 30 candidate.

### Focused layout and legibility

- Give the selected Tone EQ or Final EQ most of the available graph area.
  Switch banks directly; retain an optional two-bank comparison when space permits.
- Show occupied bands, including deliberately bypassed bands, plus the selected
  editing target. Keep truly unused band slots out of the default plot; retain
  a complete band list for precise access.
- Place frequency/gain/Q, type, enable and audition controls beside the selected
  band or in one compact inspector. Distinguish EQ gain scale from spectrum dBFS.
- Keep the summed EQ response clearly in front of the analyzer. Use an
  understated filled spectrum and legible outlines with an IN/OUT legend.
  Essential distinctions must remain readable without relying only on color.
- Preserve direct numeric entry and exact parameter ranges. Resizing must
  not alter sound or write audio parameters.

### Analyzer behavior

| Control or behavior | Required result |
|---|---|
| IN / OUT visibility | Independently show either or both spectra at the selected EQ's actual input/output taps |
| Resolution | Provide Fast / Normal / Detailed choices with documented FFT sizes; evaluate 2048 / 4096 / 8192 as starting candidates |
| Time response | Time-based attack/release smoothing; user-selectable decay speed, consistent across UI frame rates |
| Freeze and peak hold | Freeze holds the current IN/OUT snapshot; peak hold accumulates live maxima with duration/reset controls; show each state explicitly |
| Display range and tilt | Adjustable spectrum range and display-only tilt; values must not change the audio response |
| Frequency mapping | Log-axis aggregation/interpolation that preserves local maxima and peak locations; no decorative invented detail |
| Readout and zoom | Frequency/dB readout and usable low-frequency zoom, without changing actual band limits |
| Drawing | Clear response curve, restrained spectrum fill, readable grids, stable motion and no persistent clutter |

IN and OUT refer to the selected EQ. Final EQ OUT is before the later output
trim/tuner mute; it must not be labelled as the final master-output meter.
Maintain stereo power averaging so opposite-polarity channels do not disappear.
Freeze holds both displayed spectra and the current peak overlay until resumed;
it does not keep accumulating a new maximum. Resume from fresh analysis rather
than replaying queued history. Peak hold is separately switchable and resettable. Held data belongs to its
EQ bank; switching banks shows that bank's own display state. A resolution or
sample-rate change clears held data and resumes live analysis with a visible
state change. IN/OUT visibility toggles only hide/show the respective traces.

Higher FFT resolution changes analysis-window and display response time.
It must not add audio-path latency. Keep capture bounded and nonblocking,
compute FFT/aggregation outside the audio callback, and suspend unused work
when the editor or analysis view is hidden. Evaluate a dedicated analysis
worker if the new work cannot fit the message-thread budget. Do not raise
whole-window repaint frequency simply to make the analyzer appear smoother.

### Editing and comparison

| Operation | Required behavior |
|---|---|
| Add a band in the graph | Activate an explicitly unoccupied persistent slot; bypassed bands remain occupied and cannot be overwritten; show the 12-slot capacity |
| Delete a band | Disable the slot and mark it deleted/unoccupied in one undoable command; do not renumber host parameters or neighboring bands |
| Multi-selection | Select multiple bands and apply a coherent drag/bypass/delete operation with one undo transaction |
| Precision | Retain numeric Hz/dB/Q input; provide fine adjustment and axis-constrained movement |
| Band solo/audition | Momentary audition with bounded transitions and an obvious active state; release on mouse-up, focus loss and editor close |
| EQ Undo/Redo | One gesture equals one edit transaction; host gestures stay balanced; external automation must not flood or be replayed as UI history |
| EQ-local A/B and copy | Scope clearly to the selected EQ bank and preserve the existing whole-product A/B/Copy behavior |

Track slot occupancy separately from audio enable/bypass. Append per-bank
occupancy metadata without changing existing parameter IDs. New clean banks
start unoccupied. For older states without occupancy metadata, conservatively
retain every existing slot as occupied and reachable; only an explicit delete
makes one reusable. Automation must still reach every original slot, and an
externally enabled or edited slot becomes discoverable even if it was deleted.
Undo restores both the slot metadata and its previous audio state.

Each bank's EQ-local A/B pair and Undo/Redo history live for the current processor
session and survive closing/reopening its editor. They are not added to factory
presets or project sound state in this first stage. Loading a project, preset
or whole-product A/B state rebases both local snapshots to the newly loaded
bank and clears local history. Switching Tone/Final views preserves each bank's own local snapshots and
history. Local copy/switch must never restore the other EQ bank or unrelated
rig parameters. Host automation does not create new UI
undo entries; an edit transaction must not rewind unrelated automated state.

Graph creation changes the presentation of the existing 12-slot engine; it
does not add unbounded bands. No released ID, ordinal or parameter meaning is
reassigned. Restore old sessions with the same audio and defaults. Selection,
zoom, analyzer preferences and temporary audition are not new saved sound
changes. Define persistent preferences separately from sound state.

## November: Dynamic EQ and remaining EQ controls

Target: **1.3.2, November 2–27**; first test installer November 20.

- Dynamic mode on supported Bell/Shelf nodes, with Threshold, Range,
  Attack/Release, detector audition and actual gain-reduction display.
- Preserve the initial shared capacity: **at most four dynamic nodes across
  both 12-node banks combined**. Refuse a fifth clearly without disabling another.
- Show static response, configured dynamic range and current applied change
  separately. The graph and meter must describe the actual DSP.
- Add measured HP/LP slope choices and peak-to-band editing based on the
  improved analyzer. Freeze/inspect a real peak, then create or edit the
  corresponding persistent EQ slot.
- Validate low-note bloom, palm-mute build-up and harsh upper mids using
  matched-level, repeatable guitar/bass input.
- The common clean compressor, gate/expander and live limiter retain their
  separately defined signal roles; this document does not count them as EQ bands.

Begin with internal detector sources. External DAW sidechain and per-band
L/R or M/S belong to the independently tested 1.5 host/channel extension.
Do not expose inactive buttons for future processing.

## Later reference functions and decision dates

| Scope | Scheduled stage |
|---|---|
| Independent four-band dynamics, band solo and per-band GR | 1.4, March 22–April 30, 2027 |
| Six-band extension, external sidechain, L/R and M/S, Studio true-peak/LUFS | 1.5, May 3–May 28, each with its own performance/host acceptance |
| Spectral dynamics and linear-phase prototypes | 1.7 research window, July 5–July 30 |
| EQ Match, EQ Sketch and cross-instance/rig spectrum comparison | Requirements checkpoint July 9; feasibility/adoption decision July 30 |
| Qualified advanced EQ integration | 2.0 or a named 2.x follow-up assigned at the July 30 decision |

The graph/analyzer, ordinary Dynamic EQ and spectral dynamics are different
deliverables. FabFilter documents linear-phase requirements for its spectral
mode; this is a reason to measure latency, pre-ringing and branch alignment,
not a Chimera latency claim.

## Acceptance of the October candidate

1. **Display accuracy:** sine/sweep/noise, low-frequency tones, opposite-polarity
   stereo and known EQ gain establish peak position, level calibration and
   correct IN/OUT tap labels at each selected resolution.
2. **Motion and usability:** actual guitar/bass playback shows readable attack,
   sustain and decay; inspect a short moving capture and real edits at native
   100/125/150% scales on Windows/macOS/Linux.
3. **Editing/state:** add/remove, multi-selection, audition release, undo/redo,
   bank comparison, host automation and project/whole-product A/B restore
   retain the existing audio and parameter identities.
4. **Cost/lifecycle:** compare analyzer/UI cost and full-product callback
   timing with analysis visible, hidden and closed. Publish the workload,
   averages/tails/misses and chosen refresh/FFT settings.
5. **Delivery:** the installer report identifies included and pending
   requirements. A static image alone is not analyzer-motion acceptance,
   and a compilation result alone is not real-host acceptance.

Use existing tests where they cover the changed behavior and add only
meaningful coverage for new interactions and measured display behavior.
No product test or acceptance threshold is changed by this specification.

## Official functional references

Checked October 12, 2026. Study behavior and interaction; author Chimera's
own implementation, visual treatment and measured performance.

- [Pro-Q 4 analyzer](https://www.fabfilter.com/help/pro-q/using/analyzer)
- [Display and workflow](https://www.fabfilter.com/help/pro-q/using/eqdisplay)
- [Band solo](https://www.fabfilter.com/help/pro-q/using/solo)
- [Undo/Redo and A/B](https://www.fabfilter.com/help/pro-q/using/undoredo)
- [Spectrum Grab](https://www.fabfilter.com/help/pro-q/using/spectrumgrab)
- [Dynamic EQ](https://www.fabfilter.com/help/pro-q/using/dynamic-eq)
- [Spectral dynamics](https://www.fabfilter.com/help/pro-q/using/spectral-dynamics)

