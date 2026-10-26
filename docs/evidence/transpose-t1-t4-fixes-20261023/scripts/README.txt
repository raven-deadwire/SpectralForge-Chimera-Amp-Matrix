Experiment builds (not the committed engine):
- high-guard-sweep.txt, high-guard-flam.txt: commit 62f4a0b + scripts/experiment-build-*.patch (env TN_HIGH_GUARD, TN_HIGH_HOLD), engine corr40-split-rt.
- splice-fade-sweep.txt: corr40-split-v2 before setSpliceFade/reportAtDetection were fixed, with env TN_SPLICE_FADE read in TransientSplitPitch (temporary, not committed).
All other files use the committed corr40-split-v2 build (build.json).
