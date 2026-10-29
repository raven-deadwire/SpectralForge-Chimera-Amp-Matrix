Preregistered re-validation of the corr40-split-v3-sinc high-band residual with a floor-corrected
rule (docs/TRANSPOSE_V3SINC_RESIDUAL_REVALIDATION_20261029.md). Synthetic tones only.
Script: Tools/TransposeNext/residual_validation.py (unchanged since the registration commit 23682c8).
Renderer: Tools/TransposeNext/build.py with JUCE 8.0.8 at main bba4a9d; hashes in preregistration.json.

preregistration.json         seed 86603, rules R1-R4, development data note, source/binary hashes
run-started.json             registration hash and start time of the single run
run.log                      per-tone progress (v3-sinc net residual per shift) and verdicts
residual-revalidation.json   summary (per rate: R1-R4, medians, report-only items) and all 192 rows
                             (rate, case, f0, tilt, shift, ideal floor; per engine residual, net, excess, harmonic loss)
