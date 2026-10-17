# 1.3.2 tuner freshness and repaint correction

The integrated source includes PR #69 and the owner-approved expiry from #68.
This follow-up expires a displayed observation after 250 ms even if the analysis
worker itself is stalled. A controlled regression restores the former getters:
it fails while the corrected reader passes the identical worker-stall fixture.
In this first correction, the input-consumption timestamp is published after estimation; it is not renewed
merely because a delayed computation completes. The callback adds no locks,
allocations, timestamp calls or pitch analysis. The later input-arrival correction
in `../132-tuner-input-age-20261014/` closes the queued-input resumption case;
these first-correction measurements remain historical evidence.

The worker now uses normal priority rather than background/utility priority.
The opaque tuner panel repaints its moving reading region, and does not schedule
full-panel redraws while idle. Legacy offset migration checks the parameter's
canonical default, retaining an independent near-zero check for ARM rounding.

The local native A/B run passed all 32 acquisition/step-settling routes, 108
kernel comparisons and lifecycle checks. The separate suite passed 576 pitch
and 18 weak-fundamental/decay/DC cases. Existing limits (290 ms and 0.1 cent)
were retained. Raw baseline/candidate rows and source fingerprints are included.
These are single executions; their maxima are observations, not comparative
statistical claims or driver/DAW qualification.

The signal-path workflow runs the two worker tests as an independent three-OS
job, retaining complete CTest logs and source identity. UI/graph integration and
paired callback observations remain in the native-navigation jobs. Callback
peaks are not clamped, hidden or treated as a pass; fresh same-source platform
results and the existing release acceptance remain required.
