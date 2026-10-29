Naming: the candidate was rendered under the provisional name corr40-split-v4 / v4 (engine fields and
labels inside these files). main has since merged a different v4 (low upshift read floor), so the
candidate is now corr40-split-v3-sinc / v3-sinc; its output is bit-identical to these runs.

Upshift splice-noise study (docs/TRANSPOSE_UPSHIFT_SPLICE_NOISE_20261028.md). Synthetic inputs only.
Renderers: Tools/TransposeNext built from this commit with the JUCE objects of build.py; v2/v3 output
is bit-identical to the pre-registered renderer (SHA256 5625cb1f...).

steady_floor.py / steady-floor-v3.json   12-harmonic sustain, 1-8 kHz 2 ms RMS max: output vs input and vs an
                                         ideally shifted tone; near-splice vs elsewhere (SpliceTrace v3).
residual.py / residual-v3-v3sinc.json        bright sustain (harmonics to 8 kHz, 1/h and 1/sqrt(h)): 1-8 kHz energy
                                         between harmonics re harmonic energy, output and ideal; 44.1/48/96 kHz.
variant-experiments.json                 48 kHz (v/: whole-engine variants; *_audio_only: output reads only)
                                         and 44.1/48/96 kHz exact Hermite / sinc runs, made with the scratch copy in
experiment-knobs.patch                   (INTERP=1 Hermite, 2 exact 16-tap sinc; AUDIOONLY; TFADE; WIDE). Not shipped.
latency-targets-{v3,v3sinc}-known-seeds.json.gz  latency_targets.py, spec version 2, seeds 8139/20261024, long 1.
marker_echo.py / marker-echo-{v3,v3sinc}.json.gz  1-8 kHz peak 15-60 ms after each marker re 0-15 ms, output vs input.
block-invariance-v3sinc.jsonl            BlockInvariance (then "v4"), 13 block patterns x 6 cases (max abs difference).
cpu-v3-v3sinc.json                       thread CPU per callback, 48 kHz, 5 alternating runs (shared VM).
Run the scripts from any directory; they import Tools/TransposeNext from the repository path.
