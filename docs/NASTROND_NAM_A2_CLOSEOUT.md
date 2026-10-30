# Náströnd TONE3000 A2 NAM: development closeout

[한국어](NASTROND_NAM_A2_CLOSEOUT.ko.md)

Closed 2026-10-29 with the merge of PR #216. This summarises what was built,
what the released models measure, what was learned, and what is left open.
The per-experiment evidence is in [`Tools/Nam/reports`](../Tools/Nam/reports).

## Outcome

Five NAM A2-Full captures of the Náströnd product channels, with TONE3000
presets, are **release-approved under release profile v3**
([release-approval-20261029](../Tools/Nam/reports/release-approval-20261029.md)).
Approval rests on two things:

- every channel passes the v3 numbers on validation and on the reserved test
  signal;
- the owner reported listening acceptance for all five channels.

The reference-fidelity profile v2 is **not** met. The high-gain channels' full
ESR is 0.017 to 0.022 against v2's 0.01. That limit is out of reach for the
model size TONE3000 accepts (see [Findings](#findings)).

NAM weights, presets and audio are not in the repository. The release zip is
`Nastrond-A2-TONE3000-Release.zip`, SHA-256
`ea96c4aec88baa0b23f6a7af00c14d1efe46bbb79e5a3fcef621c7f713afd957`. The model
and preset hashes are in the approval report. Keep the zip outside the
repository. It was built in an ephemeral session, and this document is the
durable record of its hashes.

## Released models

Each channel is an amp-head capture at its product defaults, with no cabinet,
48 kHz and A2-Full (8 channels). Every number below is measured with
`quality_profile.fidelity` on the synthetic held-out signals.

| Channel | Character | Validation full ESR | Test full ESR | Test active median / p95 / worst | Test quiet RMS / peak dBFS | Test level RMS / peak dB |
|---|---|---:|---:|---|---|---|
| Fenrir | early bite, lean lows | 0.0184 | 0.0198 | 0.0113 / 0.093 / 0.198 | −88.8 / −81.9 | −0.12 / −0.16 |
| Surtr | distributed compression | 0.0166 | 0.0190 | 0.0109 / 0.071 / 0.304 | −76.8 / −69.6 | −0.02 / +0.21 |
| Nidhoggr | uneven drive | 0.0202 | 0.0224 | 0.0141 / 0.080 / 0.376 | −77.7 / −71.0 | −0.07 / +0.09 |
| Fimbulvetr | broad body, bloom | 0.0068 | 0.0082 | 0.0062 / 0.026 / 0.088 | −81.9 / −75.4 | −0.04 / +0.90 |
| Ragnarok | late saturation, deep hit | 0.0213 | 0.0186 | 0.0153 / 0.069 / 0.212 | −70.3 / −63.9 | −0.10 / +0.01 |
| **Mean** | | **0.0167** | **0.0176** | | | |

Player checks against TONE3000 `ef6f178`, built offline as `ChimeraTone3000Tool`:

- **A2 gate:** every file passes the player's A2 import check as A2-Full.
- **Parity:** the player's engine matches the Python model to −133 dBFS or better.
- **Presets:** each preset restores bit-identical audio with its 6-band POST EQ.
- **Calibration:** output is set to −20 dBFS RMS through the channel EQ; Normalize is off.
- **Silence:** silent input gives −152 dBFS or less.

Where each model comes from:

| Channel | Run | Report |
|---|---|---|
| Surtr, Nidhoggr, Fimbulvetr | 16,320 s data, `--keep-passed` | [continued-16320-20261027](../Tools/Nam/reports/continued-16320-20261027.md) |
| Ragnarok | channel-normalized loss with peak penalty | [channel-loss-20261028](../Tools/Nam/reports/channel-loss-20261028.md) |
| Fenrir | Fenrir-only, block-normalized loss | [fenrir-quiet-pass-20261029](../Tools/Nam/reports/fenrir-quiet-pass-20261029.md) |

## Quality profiles

Both profiles live in `Tools/Nam/quality_profile.py`.

- **v2 (`nastrond-tone3000-fidelity-v2`): reference fidelity.** It is unchanged
  and still reported.
  - Full ESR below 0.01.
  - Active-window ESR median / p95 / worst at most 0.005 / 0.01 / 0.02.
  - Quiet residual at most −70 dBFS RMS and −60 dBFS peak. Zero-history samples
    are reported but not judged, because no bare A2 can predict them.
  - Level within ±0.5 dB RMS and ±1 dB peak.
- **v3 (`nastrond-tone3000-a2-release-v3`): the release gate**
  ([release-profile-v3-20261028](../Tools/Nam/reports/release-profile-v3-20261028.md)).
  - Same measurements as v2; only the active and full-ESR limits change, per
    channel class.
  - High-gain channels (Fenrir, Surtr, Nidhoggr, Ragnarok): full ESR ≤ 0.025;
    median / p95 / worst ≤ 0.02 / 0.12 / 0.40.
  - Fimbulvetr: full ESR ≤ 0.01; median / p95 / worst ≤ 0.0075 / 0.03 / 0.20.
  - Quiet and level limits are unchanged from v2.
  - Both splits must pass, and release also needs listening acceptance.

The v3 limits were set after the results were known, from the measured A2
floor. They certify "as close as A2 gets", not "indistinguishable from the
native DSP". Three margins are small:

| Margin | Value | Limit |
|---|---:|---:|
| Nidhoggr test worst window | 0.376 | 0.40 |
| Nidhoggr test full ESR | 0.0224 | 0.025 |
| Fenrir validation quiet peak | −60.2 dBFS | −60 dBFS |

## Path

Mean full ESR of the five channels:

| Stage | Validation | Test | Report |
|---|---:|---:|---|
| First v2 run, 5,000 steps from scratch | 0.098 | | [train-v2-20261024](../Tools/Nam/reports/train-v2-20261024.md) |
| 50,000 steps | 0.032 | | [train-v2-50k-20261025](../Tools/Nam/reports/train-v2-50k-20261025.md) |
| Low-LR finish | 0.025 | | [finish-ab-20261025](../Tools/Nam/reports/finish-ab-20261025.md) |
| 1,920 s of data | 0.021 | 0.0225 | [final-candidate-20261026](../Tools/Nam/reports/final-candidate-20261026.md) |
| 3,840 s | 0.0181 | 0.0193 | [continued-3840-20261026](../Tools/Nam/reports/continued-3840-20261026.md) |
| 7,680 s | 0.0168 | 0.0180 | [continued-7680-20261027](../Tools/Nam/reports/continued-7680-20261027.md) |
| 16,320 s, `--keep-passed` | 0.0161 | 0.0174 | [continued-16320-20261027](../Tools/Nam/reports/continued-16320-20261027.md) |
| Released set (Ragnarok and Fenrir retrained for level and quiet) | 0.0167 | 0.0176 | [release-approval-20261029](../Tools/Nam/reports/release-approval-20261029.md) |

The released set's mean is slightly higher than the 16,320 s run's. Two
channels traded a little full ESR to pass a limit:

- **Ragnarok** test peak level: +1.52 dB became +0.01 dB. Validation ESR rose
  8% (0.0197 to 0.0213); test ESR was unchanged (0.0187 to 0.0186).
- **Fenrir** validation quiet peak: −53.3 dBFS became −60.2 dBFS. ESR rose about
  8% on both splits.

## Findings

What moved the error:

- **Training budget.** Ten times the steps halved the error.
- **Data.** More data helped, with each doubling worth less than the last: test
  ESR fell 14%, then 7%, then 3.6%. The extrapolated floor at A2 width is about
  0.017.
- **Model width, which TONE3000 does not accept.** Width 16 cut Fenrir's ESR by
  34% and width 32 by a further 13%, on 480 s of data
  ([capacity-diagnostic-20261026](../Tools/Nam/reports/capacity-diagnostic-20261026.md)).
- **A per-crop peak penalty** fixed Ragnarok's level ([peak-penalty-20261027](../Tools/Nam/reports/peak-penalty-20261027.md),
  [peak-weight-20261027](../Tools/Nam/reports/peak-weight-20261027.md)). The
  older per-sample overshoot penalty did nothing: the peak limit is set by a
  few attack samples.
- **A block-normalized loss** fixed Fenrir's quiet window
  ([fenrir-retrain-20261029](../Tools/Nam/reports/fenrir-retrain-20261029.md),
  [fenrir-quiet-pass-20261029](../Tools/Nam/reports/fenrir-quiet-pass-20261029.md)).
  It normalizes the error per 512-sample block, so quiet decays inside loud
  crops count.

What did not:

- **Longer model memory.** The native DSP has no memory beyond 132 ms
  ([history-diagnostic-20261026](../Tools/Nam/reports/history-diagnostic-20261026.json),
  [memory-diagnostic-20261024](../Tools/Nam/reports/memory-diagnostic-20261024.md)).
- **Loud-input crops, amplitude-weighted loss and channel-normalized loss.**
  None reduced full ESR ([loud-crops-ab-20261026](../Tools/Nam/reports/loud-crops-ab-20261026.md),
  [finish-ab-20261025](../Tools/Nam/reports/finish-ab-20261025.md),
  [channel-loss-20261028](../Tools/Nam/reports/channel-loss-20261028.md)).
- **Weighting tail crops harder.** Fenrir's quiet window did not move
  ([fenrir-quiet-20261028](../Tools/Nam/reports/fenrir-quiet-20261028.md)).

Constraints:

- **TONE3000 loads only A2,** width 3 or 8 with the fixed layer tables
  ([tone3000-a2-only-20261028](../Tools/Nam/reports/tone3000-a2-only-20261028.md)).
  A wider model would play only in other NAM players.
- **Loud, heavily driven notes carry most of the error.** Notes at −6 dBFS input
  or more have 3 to 6 times the error of quieter ones, and about 80% of that
  error is below 5 kHz.

Corrections on record:

- **Fenrir's quiet failure.** It was first attributed to the hard cut at the
  end of the synthetic signal. A faded ending did not remove it
  ([faded-heldout-20261029](../Tools/Nam/reports/faded-heldout-20261029.md)).
  The cause is the low-level release after the input stops, which A2 models
  poorly.

## Reproducing

The tools are in `Tools/Nam`; the workflow is described in
[NASTROND_NAM_TOOLCHAIN](NASTROND_NAM_TOOLCHAIN.md). The release followed these
steps:

1. **Build** `ChimeraNamRender` and `ChimeraTone3000Tool` from
   `Tools/Nam/CMakeLists.txt`, with JUCE and the TONE3000 player at the commits
   in `Tools/Nam/ci-sources.json`.
2. **Capture** 480 s parts with `capture_channels.py --train-seed N`. The
   validation, test and audition seeds are fixed.
3. **Grow the training set** with `combine_captures.py` or `append_captures.py`.
   The renderer accepts at most 600 s per input.
4. **Train** with `train_a2.py --recipe legacy --center-training-output`,
   warm-starting from the best models with `--keep-passed` and an LR warmup.
   For a single-channel fix, add `--train-channels`, `--target-limits`,
   `--block-norm-samples` or `--peak-weight`.
5. **Package and verify** with `package_a2.py` and `ci_pipeline.compare_package`.
6. **Approve** with `approve_release.py`. It refuses unless every channel passes
   v3 on both splits, and it records the owner's listening statement.

Practical notes:

- **Data size.** The 16,320 s set is 18 GB and exceeds 16 GB of RAM. Training
  reads it memory-mapped, and single-channel runs read only their channel.
- **Denormals.** Weights decayed toward 1e-30 made CPU training 4.5 times
  slower. `train_a2.py` now flushes denormals to zero.
- **Speed.** About 1 hour per 10,000 packed steps on 4 CPU threads.

## Open items

- **Listening record.** The owner's listening acceptance was recorded as stated,
  without its method or material.
- **Real-world testing.** No GUI or DAW session test has been done, and no
  recorded instrument DI was used in any measurement.
- **Rerun first if anything changes.** If the DSP headers frozen at `4ef21c16`
  change, the captures and models no longer describe the product, so re-capture
  and re-qualify. Likewise rerun the package checks for any TONE3000 player
  newer than `ef6f178`.
- **Raising fidelity beyond v3** needs a wider model than A2, so a separate,
  non-TONE3000 build.
