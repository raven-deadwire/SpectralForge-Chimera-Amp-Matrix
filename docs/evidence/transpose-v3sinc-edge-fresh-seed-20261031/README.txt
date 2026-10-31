Preregistered fresh-seed validation of corr40-split-v3-sinc-edge against corr40-split-v3-sinc
(docs/TRANSPOSE_V3SINC_EDGE_FRESH_SEED_20261031.md). Synthetic inputs only.
Script: Tools/TransposeNext/edge_validation.py (registration commit 6a9b089; unchanged since).
Renderer: Tools/TransposeNext/build.py with JUCE 8.0.8; hashes in preregistration.json.

preregistration.json        seeds, pass rules E1-E5, development-data note, source/binary hashes
*-started.json, *.log       one start marker and log per phase
sweep{1,2,3}.json.gz        k*fc +- k Hz sweeps (k = 1, 2, 3), 3 rates x 4 shifts x 51 points, both engines, net residual
residual.json               16 fresh tones x 4 shifts x 3 rates, both engines, net residual
target-summary.json         spec v2 verdicts (all inputs, fresh-only) for both engines and the equivalence summary
latency-targets-{v3-sinc,v3-sinc-edge}.json.gz/.log   latency_targets.py results (792 renders each) and logs
block-invariance.json       BlockInvariance v3-sinc-edge, 432 cases x 13 block patterns
cpu.json                    report only: thread CPU per callback, 48 kHz, shared VM
edge_judge.py / verdicts.json   applies E1-E5 to the files above (reads the gzipped sweeps directly)
