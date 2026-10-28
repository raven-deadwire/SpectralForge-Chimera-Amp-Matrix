Target spec version 2 (T1 split) checks. No new renders.
reevaluation.json: latency_targets.py --evaluate-only on
  fresh-seed-41421-v2-split   rows from docs/evidence/transpose-pdc-snr10-fresh-seed-20261026 (latency-targets-v3-snr10.json.gz), default (spec v2)
  fresh-seed-41421-legacy-t1  same rows with --legacy-t1
  fresh-seed-26457-old-rows   docs/evidence/transpose-v3-block-invariant-20261024/fresh-seed-corr40-split-v3.json.gz (no per-event SNR -> scored as version 1)
di-selftest.json: di_validation.py selftest (synthetic pipeline check, not T6 evidence), renderer SHA256 5625cb1f...
