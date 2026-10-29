Preregistered fresh-seed validation of corr40-split-v3-sinc against corr40-split-v3
(docs/TRANSPOSE_V3SINC_FRESH_SEED_20261029.md). Synthetic inputs only.
Script: Tools/TransposeNext/v3sinc_validation.py (unchanged since the registration commit 144e801).
Renderer: Tools/TransposeNext/build.py with JUCE 8.0.8; hashes in preregistration.json.

preregistration.json                 seeds, pass rules P1-P6, source/binary hashes (pushed before rendering)
*-started.json                       one marker per phase (registration hash, start time)
target-summary.json                  P1-P3: spec v2 verdicts (all inputs, fresh-only) for v3 and v3-sinc, equivalence
latency-targets-{v3,v3-sinc}.json.gz/.log   latency_targets.py results (792 renders each) and logs
pdc-summary-v3sinc-L2.5-snr10.json   P4: pdc_study evaluate, v3-sinc, fixed L = 2.5 ms, event SNR >= 10 dB
pdc-results-v3sinc.json.gz, pdc-run.log     pdc_study run results and log
residual.json                        P5: 12 fresh bright sustains x 3 rates x 4 shifts, excess over ideal, v3 and v3-sinc
residual_diag.py / residual-diag.json       report only: ideal floor and absolute residual per case, medians by f0 band
marker-echo.json.gz                  report only: marker tail re-read on fresh random markers, v3 and v3-sinc
block-invariance.json                P6: BlockInvariance v3-sinc, 432 cases x 13 block patterns
