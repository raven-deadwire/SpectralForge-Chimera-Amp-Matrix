# macOS application event and stop compatibility

This external diagnostic compares the original S native message helper with the
exact U (`c291d7132ba33561db922e7d5a992ab40212345d`) helper, checked against
blob `bf059e183bba35b944a804d534185b056881c270`. Each normal-Release executable
runs once. It is separate from the required real product gates and does not
reproduce the earlier GUI failures.

The native application subclass observes `sendEvent:` and forwards every event
to the original implementation. It never overrides the AppKit dequeue method,
returns a fabricated nil poll, injects a sleep, or changes the helper's event
deadline. A method-implementation identity check verifies the original dequeue
method is in use.

## One sequence in each process

1. Create the native application and initialize JUCE. A zero-budget helper call
   initializes its one-time logging without running either event-loop phase.
2. Post one uniquely tagged `NSEventTypeApplicationDefined` event through
   `NSApp postEvent:atStart:NO`. Post 12 JUCE callbacks followed by a FIFO fence.
   Neither set of actions is ever reposted.
3. Service 20 ms helper slices until both queues deliver, within a fixed 2 s
   delivery limit. Require exactly one native event delivery through
   `sendEvent:`, return from its superclass dispatch, all 12 callbacks in FIFO
   order, the fence, and main/message-thread execution. Record actual event and
   fence delivery timestamps; a late return cannot turn late delivery into a pass.
4. Freeze progress evidence. Post one JUCE callback that calls
   `stopDispatchLoop()`. Make exactly one `dispatchFor(20)` call and require the
   callback, the quit flag, and a false return. A zero-budget call afterward
   must also return false. No stop-phase retry or drain is allowed.
5. Freeze the overall outcome before stopping the periodic native wakeup event
   source and tearing down JUCE. Preserve the actual process result even if a
   later variant fails. Missing reports, crashes and process timeouts are failures.

Both variants use the same predicates. The candidate differs by one CF boolean
and three explanatory comments. The controller verifies exact source/helper
hashes, JUCE 8.0.8 source identities, and target compile commands. It builds only
the two core/events probes with normal Release optimization and debug symbols;
no sanitizer or product, artwork, full test-suite, or publication target is built.
Both binaries, dSYMs, UUIDs, source, flags, raw results and scoped verdict are kept.

## Interpretation

A pass demonstrates application-defined AppKit queue delivery, JUCE FIFO
progress, and the helper's explicit stop return under this finite workload. This
is not a hardware keyboard/mouse event, a visible-window/modal-input test, a
general fairness proof, a reentrant-callback test, an original failure
reproduction, a hard 20 ms return-time guarantee, or release approval. The first
phase uses repeated bounded slices to service a single posted workload; it does
not rerun or retry an action or process.

Apple documents the queue and event APIs used here:

- [NSApplication postEvent](https://developer.apple.com/documentation/appkit/nsapplication/postevent%28_%3Aatstart%3A%29?language=objc)
- [NSApplication sendEvent](https://developer.apple.com/documentation/appkit/nsapplication/sendevent%28_%3A%29?language=objc)
- [NSEvent custom event creation](https://developer.apple.com/documentation/appkit/nsevent/otherevent%28with%3Alocation%3Amodifierflags%3Atimestamp%3Awindownumber%3Acontext%3Asubtype%3Adata1%3Adata2%3A%29?language=objc)
