# Native dynamics navigation regression — 2026-10-22 KST

Scope: PR #111 on top of PR #110. Technical UI/compatibility qualification only;
this does not approve a beta, installer, release tag, DI listening or installed DAW use.

## Comparison and original failure

- PR #110 source: `3a942a125d652a234d0f481fca3af2bdf6897635`.
- PR #111 original source: `ffab682bef3468001f98624bda3051796341ab51`.
- Original failing run: [39529062384](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39529062384),
  macOS job `118990867494`; Windows and Linux passed.
- Prior passing run: [39398055175](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39398055175),
  macOS job `118545594461`.

The PRE panel, editor navigation, signal-path buttons, native UI tests and native
message pump have identical Git blobs in the two original sources. PR #111 changed
the multiband synthesis and two help strings, not the PRE navigation implementation.
The machine-readable blob comparison and original log excerpts are in
`Validation/NativeDynamicsNavigation/`.

The failed macOS run recorded a 960.427 ms dispatch (958.203 ms in AppKit polling)
and an 840.068 ms dispatch (839.579 ms in CF service), both requested for 55 ms.
These inclusive native-stage measurements do not distinguish OS scheduling from
callbacks executed inside CF/AppKit, and do not by themselves prove a UI defect.
The old log did not record command delivery or board visibility at the failure.

## Causal test and correction

JUCE 8.0.8 `Button::triggerClick()` posts a command message. The original `tab()`
helper dispatched for 55 ms and returned without checking whether that command
had run. A native dispatch can consume its slice before reaching the posted click.
The next assertion then observes the previous page; elapsed time is not a command
receipt. The macOS JUCE message source also processes messages in batches of four.

The diagnostic revision `e44e9f8e27e7e517e598e5fa3decd27301fd9e55` keeps that old
55 ms behavior, adds a click receipt and state dump, and adds a queue regression.
The regression queues 32 real message-thread callbacks of 10 ms before PRE
navigation. It uses the ordinary native event pump and the real product callback.
It neither invokes a product timer manually nor forces a repaint, selection or
parameter change. Its result is recorded separately from the normal UI suite.

On macOS, diagnostic [run 39716544989 / job 119619896459](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39716544989/job/119619896459)
passed the normal common and native suites, then reproduced the exact original
`Reordered PRE compressor has no visible spectrum entry` failure under backlog.
At failure: `clicks=0`, `preSelected=0`, `boardVisible=0`, `boardShowing=0`,
`entryVisible=1`, `entryShowing=0`, `entryEnabled=1`, `ownerAt0=1`, `modelAt0=7`.
The stable reorder/model was intact; the PRE navigation command was not delivered.
Native overruns of 528.239 ms and 301.412 ms accompanied the pending command.
The regression identifies a test synchronization defect, not a multiband-DSP
regression or demonstrated product-state restoration failure. It does not assign
an OS/kernel cause to the historical 840 ms CF interval. Artifact `12399193264`
and `macos-before-fix.log` preserve the pre-fix evidence.

The corrected helper posts exactly one click and observes its `Button::Listener`
receipt. It uses the already established 2,000 ms native readiness budget, fails
on a missing/destroyed button or overdue/duplicate delivery, and records longer
delivery times. It never retries a click, calls `onClick` directly, suppresses
dispatch-overrun diagnostics or changes spectrum criteria.

The receipt-only revision `dbf0ac0e2b4ef2db4dcdc4b09073ec732e77c01c` passed the
ordinary macOS UI tests and the disconnected-handler control, but correctly
failed the queue test at the unchanged two-second deadline. In [run 39717249554 /
job 119622845063](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39717249554/job/119622845063),
four requested 10 ms calls consumed 606.407, 367.406, 407.227 and 846.941 ms.
The last three started another AppKit poll **after CF had already exhausted the
slice**; those polls cost a further 191.023, 162.334 and 589.536 ms. The command
was still pending (`clicks=0`). This result remains a failure, not a waived limit.

The final pump correction rechecks the existing deadline immediately after CF
service. If it is exhausted, it returns without beginning an additional AppKit
poll. Events remain queued; subsequent slices with budget still service AppKit
through the same modal filter and `sendEvent` path. CF/AppKit overruns remain
logged. No command is forced through, dropped, replaced or retried, and neither
the 32 × 10 ms backlog nor the two-second readiness limit changes. The queue
test records deferred-poll telemetry as well as its immediate UI-state checks.
This is a test-harness scheduling correction; shipping plug-ins retain their
host event loop. `macos-receipt-only-failure.log` preserves the intermediate fail.

After native dispatch returns, the synchronous product click handler has returned
as well. The tests immediately assert the selected editing stage, visible PRE
board and usable spectrum entry **before** any additional settling. The redundant
test-side overview reset was removed. A delivered click with a deliberately
disconnected UI handler must fail the selection assertion: command completion
alone cannot conceal a real UI-state defect. PRE spectrum and POST detail entry
use the same command receipt, while their original owner/source/parameter checks
remain in place.

## Follow-up: short native slices after the deadline correction

The deadline correction above made every idle slice of 10 ms or less skip AppKit:
`CFRunLoopRunInMode` waited `min(10 ms, remaining)`, so an idle CF wait itself
reached the deadline and the AppKit poll was always deferred. Many native suites
pump with `settle(1)`, `settle(2)`, `settle(10)` or `dispatchFor(10)` (Graphical EQ,
common dynamics, CAB microphone/scene, amp head, native panels). The 1.3.2
signal-path macOS job passed at `ffab682` and failed at `f7d16e1`, where only
test-harness files changed; the dynamics workflow on the same SHA passed.

An idle CF wait now uses at most half of the remaining slice, so a slice that
was not overrun still reaches its AppKit poll before the deadline. A CF source
that actually exhausts the slice still defers the poll exactly as before; the
queue backlog, two-second readiness limit and overrun logging are unchanged.
The specific failing signal-path test was not visible from this environment
(job logs unavailable); this follow-up is qualified only by the new macOS run.

## Validation boundaries

The dedicated workflow now runs common dynamics, native dynamics and native
dynamics with queue backlog on Windows, macOS and Linux. The queue variant also
runs the disconnected-handler negative control, then the complete original
PRE/POST/LOW coverage. The original six workspaces, five PRE compressors, three
POST compressors, stable reordered owners, inactive-source/Freeze isolation,
parameter/gesture invariants, real IN/OUT FFTs and 75/100/125/150% layouts are kept.

All product `Source/` files and released parameter fixtures are unchanged from
`ffab682`. This includes 5,026 existing plus 55 appended parameters (5,081 total),
their order/IDs/version/defaults, project/preset/A-B restoration, host latency,
minimum-phase multiband synthesis and bypass behavior.

Local real-C++ numerical revalidation passed all **224 impulse pairs and 1,728
transition records** at 44.1/48/96/192 kHz. Source hashes and maximum static error
are recorded in `comparison.json`. No new CPU-performance claim is made.
The dedicated multiband workflow separately reruns numerical/sanitizer and native
parameter/state/bypass contracts. Only CI results for the final correction SHA
may qualify that SHA; diagnostic or prior results cannot be substituted.

Final run URLs, source SHA and per-platform outcomes belong in PR #111's validation
record after those runs finish. Actual instrument DI listening and installed DAW
qualification remain incomplete. No publication or merge is part of this change.
