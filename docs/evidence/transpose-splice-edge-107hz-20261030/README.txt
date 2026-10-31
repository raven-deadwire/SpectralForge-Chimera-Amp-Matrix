Cause analysis of the ~107 Hz high-band residual (docs/TRANSPOSE_SPLICE_EDGE_107HZ_20261030.md).
Synthetic tones only. Repository engine and product Source/ unchanged; the fix exists only as a patch.

edge-reject-experiment.patch   experimental engine change (CorrelationPitch.h, TransientSplitPitch.h,
                               TransposeProbe.cpp): setEdgeReject (default off; transient path only in the
                               final variant), engine name corr40-split-v3-sinc-edge. Off = bit-identical.
JumpTrace.cpp                  per-splice jump trace of both split paths (needs the patched headers for
                               v3-sinc-edge; v3 / v3-sinc work with the repository headers too)
sweep.py / sweep-base.json     100-115 Hz, 0.25 Hz steps, 3 rates x 4 shifts, v3-sinc
fine.py / fine-*.json          fc +-1 Hz, 0.04 Hz steps: base = v3-sinc; edge = rule 1 only, both paths;
                               edge2 = rules 1+2, both paths; edgeT = rules 1+2, transient path only
fineK.py / fineK{2,3}-*.json   2fc and 3fc neighbourhoods (base, edgeT); pass a unique tag per process
resid_sets.py / resid-sets{,T}.json   seed 70711 and 86603 residual tone sets, v3-sinc vs edge (both / transient)
latency-targets-edge-{bothpaths,transient}-known-seeds.json.gz   latency_targets.py, known seeds, edge variants
                               (compare with docs/evidence/transpose-upshift-splice-noise-20261028/
                               latency-targets-v3sinc-known-seeds.json.gz)
Note: a first 2fc/3fc run with four processes sharing temp file names was corrupted and discarded.
