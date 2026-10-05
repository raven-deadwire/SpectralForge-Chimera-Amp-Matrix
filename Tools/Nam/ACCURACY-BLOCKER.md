# Náströnd A2 accuracy: measured blocker

Subsequent user decision: keep TONE3000. The new project profile in
`quality_profile.py` preserves active-window ESR limits and assesses quiet
decay by absolute residual. The historical proof and numbers below remain
valid for the original all-relative gate; they are not the revised policy.

The existing accuracy gate cannot be fully satisfied by a bare TONE3000 A2
model at 48 kHz, regardless of additional training. This is an error in the
initial architecture/gate pairing. It is not a failure of the user's request
and does not authorize lowering the threshold or claiming release readiness.

The delivered v0.2 files remain unchanged. No further training job or public
release was started after this blocker was proven.

## Evidence

The pinned A2 shape has a 6,347-sample receptive field (132.23 ms). Its output
at a sample depends only on that many input samples and fixed weights. When
all of those samples are zero, every trained model of this shape produces one
constant. The original amp's recursive filters can still be decaying then.

For each existing 4,096-sample evaluation window, let Z be the subset whose
entire input receptive field is zero. Even allowing the model an independently
optimal constant for that subset leaves at least:

`ESR >= sum((reference[Z] - mean(reference[Z]))^2) / sum(reference^2)`

This generously permits zero error outside Z and a different constant for
every window. A real A2 has fewer freedoms. Consequently, a lower bound above
0.02 proves that no choice of weights can satisfy that window's fixed gate.
It does not estimate achievable accuracy on active playing passages.

The native validation window starts at sample 1,132,747 (23.5988958 s), contains
4,096 samples and has 2,497 zero-history samples. All three witnesses remain
eligible under both the original raw-energy cutoff and the delivered package's
calibrated -80 dBFS window-energy cutoff.

| Channel | Best possible ESR lower bound | Required maximum | Packaged native window RMS |
| --- | ---: | ---: | ---: |
| Surtr | 0.172024 | 0.020000 | -77.99 dBFS |
| Nidhoggr | 0.048412 | 0.020000 | -75.31 dBFS |
| Fimbulvetr | 0.182548 | 0.020000 | -78.38 dBFS |

The separate synthetic test also contains a Nidhoggr witness with lower bound
0.049446 and calibrated native RMS -73.12 dBFS. No weights, input, source DSP,
reference scale, window cutoff or threshold were modified for the audit.

The unmodified target engine rendered all five v0.2 NAMs on both 24-second
inputs. Its output peak-to-peak spread across the zero-history samples was
exactly zero for every model/input pair, corroborating the finite-support
calculation. A new native Surtr validation capture was bit-identical to the
stored reference, using the original renderer hash. Bound arithmetic and the
zero-history boundary were checked independently.

Machine-readable evidence: [reports/a2-memory-audit-v02.json](reports/a2-memory-audit-v02.json).

## Target restrictions

Player revision: `ef6f178ae1ac6412b55fec6f058d86e640e5aaaf`.
NAM core revision: `1f42f88535884450104b8711d7595019afa0495b`.

- `a2_fast.h` fixes 23 kernel/dilation pairs and the 16-tap output head.
- `a2_fast.cpp::is_a2_shape` requires those exact pairs, one layer array and
  3 or 8 channels. Longer-memory WaveNets do not satisfy the predicate.
- `ProcessorModelLoader.cpp::namConfigIsA2` accepts a bare A2 or a
  SlimmableContainer containing only A2 models. That container chooses a tier;
  it does not concatenate model memories or provide gain/tone conditioning.
- The loader runs NAMs at the 48 kHz chain base rate even when file metadata
  declares another rate. Relabelling the sample rate cannot extend memory.
- Additional player oversampling phase-interleaves convolutional models;
  it does not extend their history duration at the base rate.

This proof concerns the bare NAM comparisons currently required by the
workflow. It does not establish impossibility for a redesigned NAM-plus-IIR
chain, a different supported architecture, or a player with longer memory.
Those would be different product/validation contracts and must be explicit.

## Decisions needed before claiming the original goal is reachable

1. **Keep TONE3000 compatibility.** Continue reducing errors on active audio,
   but explicitly revise how very quiet decay is judged: use an absolute
   residual criterion alongside relative ESR. A suitable numeric boundary
   must be specified and checked, not selected just to make the current files
   pass. This changes the original gate and needs the user's decision.
2. **Keep the exact original gate.** Change to a longer-memory or recurrent
   NAM architecture and a player that actually accepts it. Five independent
   channel files remain the deliverable. Longer memory only removes this
   proven obstacle; further learning and independent tests are still needed.

Neither choice has been silently applied. The active-passage errors in v0.2
are also above target; changing the tail metric alone would not qualify it.
The earlier sustained-input history probe did not reveal this blocker because
its common suffix remained nonzero. Its small lower bound cannot be generalized
to the zero-input decay windows above.

## Reproduce

```sh
python Tools/Nam/audit_a2_memory.py \
  --data /absolute/path/to/nastrond-five-a2-data-v01 \
  --models /absolute/path/to/nastrond-five-a2-run-v02 \
  --package /absolute/path/to/Nastrond-TONE3000-Test-v0.2 \
  --tool /absolute/path/to/ChimeraTone3000Tool \
  --player /absolute/path/to/tone3000-plugin \
  --out /absolute/path/to/a2-memory-audit.json
```

Use the captured input/reference arrays whose SHA-256 digests match the
manifest. The audit verifies them and runs the actual target engine with EQ
and Normalize disabled. It performs no optimization or checkpoint selection.
