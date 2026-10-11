# macOS queued-callback fairness diagnostic

This is a controlled scheduling workload, not a reproduction of the original
CAB GUI failure or Signal crash, and not release acceptance.

Two small executables compile the same probe, the exact 7c782fda native helper,
and pinned JUCE 8.0.8 core/events. The candidate helper differs only in
`CFRunLoopRunInMode`'s final argument: `true` becomes `false`. No product, DSP,
artwork, full test suite, native capture, or publication target is built.

The probe creates an NSApplication subclass before initializing JUCE. During one
armed call to the real `dispatchFor(10)`, its AppKit poll records the delivered
callback count, blocks for 20 ms without pumping, and returns nil. Before arming,
it posts 12 inexpensive callbacks followed by one FIFO fence using the actual
`MessageManager::callAsync`. Both executables use exactly the same predicate.
A preceding `dispatchFor(0)` initializes the helper's one-time log only. One
identical, unarmed 10 ms CF call then drains a separately posted startup fence;
fence completion, message-thread identity and no stop are required. Its elapsed
time is reported separately, with no latency gate or retry. The measured 13
callbacks have not been posted yet during either setup operation.

## Required outcomes

| Evidence | Original (`true`) | Candidate (`false`) |
| --- | --- | --- |
| Successful posts | 13 | 13 |
| Armed AppKit polls | Exactly 1 | Exactly 1 |
| Callbacks before first poll | Exactly 4, FIFO 0–3 | Exactly 12, FIFO 0–11 |
| Fence before first poll | False | True, delivered within 10 ms |
| State at dispatch return | Same as first poll | Same as first poll |
| Stop / wrong thread | False / false | False / false |
| Marker | `FAIL_FENCE_DEFERRED_BY_FIRST_APPKIT_POLL` | `PASS_FIFO_FENCE_BEFORE_FIRST_APPKIT_POLL` |
| Process exit | 1 | 0 |

The verdict is copied before interception is disabled or JUCE tears down.
Pending callbacks retain their state. Cleanup cannot turn the original failure
into a success. The runner inspects both JSON records and real process exits;
an arbitrary exit 1, crash, missing result, timeout, unexpected baseline pass,
or candidate failure cannot count as the required controlled red/green pair.
Each executable runs once. There is no retry or threshold adjustment.

## Scope and limitations

The injected 20 ms AppKit cost models the observed scheduling hazard without
depending on another multi-second native GUI stall. The probe tests FIFO work
spanning JUCE's four-message batches. It does not prove fairness among all native
sources, preempt a slow CF callback, test actual clicks or modal filtering, or
establish that the two-second CAB deadline is met in the original scene.
A loaded runner or unrelated first CF source may make the result inconclusive;
the original must exhibit the exact four-callback signature to qualify.

The manual-only controller checks out source
`7c782fda9e2d7cb255e70b6d1d1857fc78300c46`, verifies audited helper and JUCE source
hashes, appends the two diagnostic targets, builds only those targets, executes
the pair once, and preserves identities, logs, outputs and a scoped verdict.
It has no push, pull-request, schedule, publication or release trigger.
