# macOS tuner wake correction candidate

The fd1f222 macOS focused run 38875904343 failed 44.1 kHz / 256 / 30.867706 Hz settling at 316.972334 ms. Its final accurate estimate was published at 577.523125 ms and observed at 588.941417 ms; the third required stable observation arrived at 616.972334 ms. Compute was 1.85–3.02 ms, while several worker service and synthetic feeder gaps spanned tens of milliseconds. Product run 38875904365 separately failed 44.1 kHz / 256 / 27.5 Hz settling at 310.900625 ms. These failures are retained; QoS alone was insufficient.

The candidate uses a 10 ms DISPATCH_TIMER_STRICT source while the macOS tuner is active. Its handler only signals the existing interruptible worker event. Estimation remains on the tuner worker; the audio callback, FIFO, estimator, freshness/generation logic, route gates and feeder remain unchanged. Windows/Linux and the existing normal-QoS diagnostic reference keep their original wait. This is a timer delivery correction, not a realtime priority or process-wide App Nap override. Strict timers still do not guarantee deadlines under OS load.

The worker owns creation/cancellation. Disable cancels the timer and waits for the cancellation handler (which follows the last event handler) before releasing its context. Stop wakes the worker and joins it after timer cancellation. Failed timer creation retains timed-wait service; macOS tests reject a missing timer. Active/disabled/stopped timer state is checked beside the existing QoS, stale input and lifecycle tests.

Apple references:
- https://developer.apple.com/documentation/dispatch/dispatch_timer_strict
- https://developer.apple.com/documentation/dispatch/dispatch_source_set_cancel_handler

Requires fresh native macOS focused and Product results. No pass or release eligibility is inferred from the implementation or Windows tests.
