# Expired queued-input regression

The first reader-expiry fix at `1a4a16d48d6ca21c8a3386beaa354d3744e667bf`
passes the three-platform worker suite. An additional controlled test found
that resuming a paused worker could timestamp queued, expired samples as newly
received and briefly revive the old reading. `control.log` reproduces that
failure with the first correction and the new test; `fixed.log` passes it.

The callback now stamps the end of each published input block once, before
the existing SPSC publication. The worker reads the stamp at the end of its
FIFO snapshot while it still owns those slots. Only block-end slots need a
timestamp; FIFO snapshots always end at a published block boundary. Delayed
queue consumption and estimation therefore cannot renew the input's age.
Expired input is discarded before pitch analysis, and readers independently
expire the published observation after 250 ms.

The extra callback work is one monotonic timestamp call and one preallocated
array store per nonempty active block. It adds no lock, allocation, per-sample
timestamp or pitch analysis. The 128 KiB timestamp array is allocated with the
tuner off the callback. The regression queues a final coherent block while
the worker is paused, checks expiry during the pause, waits for a completed
worker iteration, and checks that the old queued input remains hidden.

This is regression evidence, not a driver/DAW realtime qualification. Previous
raw observations and failures are retained; acquisition, accuracy and callback
criteria are not relaxed.

## Later mixed-window correction

This test covered old queued input with no fresh block after the gap. A later
case found that one fresh block could still revalidate retained or mixed queued
history. See `../132-tuner-gap-20261014/` for the controlled failure and the
worker-side snapshot/history invalidation. The earlier logs remain unchanged.
