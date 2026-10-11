# Exact 781d cold CAB click observation

This controller runs the original full `ChimeraCabPanelStateTests` exactly once
from `781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333`, on `macos-15` (the failing job's
runner label), using normal Release optimization plus debug symbols and frame
pointers. It is diagnostic evidence, never release acceptance. A pass is NOREPRO.
The workflow uses the existing manual `build.yml` path on its isolated branch.

## Preserved behavior

The production `Source/` tree is untouched. The CAB fixtures, initial cold panel
construction, visibility calls, renderer, CF run-loop mode, positive CF duration,
`returnAfterSourceHandled=false`, nonwaiting AppKit poll, modal filtering, stop
checks, and order of native operations remain the same. The test still posts
the original button command once. The original `ready()` → 2000 ms deadline →
`dispatchFor(10)` loop is byte-identical. The existing postfailure fence and fixed
10 ms observation still execute only after the original failure is latched.
No prewarming, click retry, handler substitution, or predicate re-evaluation is
introduced. All original cases and snapshots execute in their original order.

## Observation

`ColdClickTrace` allocates and touches a fixed 16 MiB append-only mapped record
buffer before the first test case. Each producer reserves a unique slot, writes
numeric fields, then publishes the slot with a release store. No record is ever
overwritten; overflow increments a drop counter and makes the diagnostic
incomplete. The mapping remains alive through process exit so in-flight worker
publications cannot outlive storage. There is no synchronous file logging,
formatting, mutex, queue operation, or allocation in the timed observation path.
The map is flushed only during session teardown, after the original main result.

Records cover native CF / nextEvent / modal filter / sendEvent / autorelease
entry and exit, outer dispatch slices, JUCE FIFO post/source/callback boundaries,
native fixture construction, first scene entry, button trigger/listener,
predicate calls, and the already-latched failure. The full postfailure scope
includes the existing diagnostic formatting; use its nested `dispatchBegin` /
`dispatchEnd` interval to measure the actual fixed observation pump.
The session-begin record also observes the existing `NSApp` pointer and
`isRunning` state; it never creates the singleton or calls `finishLaunching`.

Two passive CF observers bracket existing observers by minimum/maximum order.
Their activity records can distinguish time spent before timers, before sources,
before waiting, after waiting, or on run-loop exit. They do not name the private
AppKit/QuartzCore observer inside such an interval. All record times are Mach
absolute ticks with an explicit timebase, and all records include the OS thread
ID. `trace-spans.json` links begin/end records by their unique sequence token.

The external Python observer reads published slots while the owned CTest child
runs. If the first scene click is still active and a CF/nextEvent/sendEvent stage
is observed for at least 100 ms, it validates the child's parent PID, UID and
executable path, then launches `/usr/bin/sample` once for five seconds at 10 ms
intervals. This never dispatches application messages or pauses the process in
a serial debugger. Live reads only trigger this best-effort sampler; stable
post-exit records are the authoritative timeline. Sampling may perturb timing,
and its output is an aggregate call tree without per-stack timestamps. A sample
that straddles more than one stage cannot prove which individual stage contained
every stack. If no qualifying interval occurs, zero samples is expected and the
unchanged CTest result still stands.

## Provenance and interpretation

`prepare.py` verifies the clean checkout's exact Git SHA and hashes every file
whose observations or assertion boundaries matter. It saves original and
instrumented copies and the complete patch. JUCE must resolve to its pinned
8.0.8 tag and its queue header must match the known normalized hash before queue
observations are inserted. Queue ownership, locking, four-message batch and
wakeup operations stay unchanged. The observed JUCE commit and both queue-file
hashes are retained. The controller is required to have exactly these six files
changed from 781d and to be a direct child of that source.

The artifact preserves the executable, dSYM/UUID, compiler flags, both source
forms, controller files/hashes, environment, raw CTest exit, JUnit, screenshots,
binary trace, decoded records, spans, sampling receipt and sample. No ASan or
renderer workaround is applied. An ordinary original test failure stays a
failed workflow even if all trace collection succeeds. A collector failure is
reported independently and cannot turn the original failure into a pass.

The question is whether the posted cold-view command remains behind a long
JUCE callback, behind a CF observer before its source, or pending while AppKit
work is entered. The earlier R profile showed backing-store/Metal queue waits;
that passing older-source aggregate is a hypothesis for this new source, not
its failure stack. This controller must preserve that distinction.
