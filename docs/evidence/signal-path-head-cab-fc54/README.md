# HEAD/CAB signal-path development — verification record

**Product source:** `fc54ab94d40e525006c474d3f1f93ce6bffa688b`  
**Product tree:** `65b2a39cda2e592f3e187dccf9216f36d5eb8360`  
**Development PR:** [#60](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/60)  
**Disposition:** **BLOCKED / Draft. October 14 inclusion held.**

This evidence-only branch preserves observations and original GitHub records without changing the product PR head. The evidence commit is not a tested product source. The source above contains the six-stage compact workflow **PRE → HEAD → CAB → TONE EQ → POST → FINAL EQ**, plus **ROUTING**. HEAD and CAB open distinct existing workspaces for the current Classic, Dual or Matrix mode. Existing top utilities, footer Tuner and complete detailed routing remain.

## Result that controls the decision

The required [SignalPath workflow38341744958, attempt1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38341744958) failed on macOS. Its first Classic scenario failed after9.29s with:

`Compact presentation hid the actual automated routing snapshot`

[The original macOS job log](ui/macos-original-job-115114298383.log) and [derived analysis](ui/macos-derived-observation-report.json) are preserved. Source inspection narrows the failed assertion to the post-gate `merge → gate` edge missing from the cached compact snapshot after utility automation. The nearby dispatch overrun is a diagnostic clue; it does not establish whether production refresh/event delivery or fixture scheduling caused the failure. No rerun, deadline relaxation or speculative source change was used to replace this result.

The other six macOS CTests passed. Windows passed all seven CTests; Linux passed all eight, including the normal native-focus test. Those successes do not qualify the failed platform.

## What was completed

| Check | Observed result |
| --- | --- |
| Offline source/release contracts | 32 commands PASS;17 unittest suites,353 executed PASS and4 existing platform/environment skips. |
| Local native SignalPath | First test execution PASS,112.56s;49/49 expected PNGs passed strict integrity/decode validation. |
| Linux SignalPath CI | 8/8 CTests PASS; original ZIP size/digest/CRC, exact-source manifest, JUnit and53/53 PNGs directly validated. |
| Windows SignalPath CI | 7/7 CTests PASS in the original job log. Archive contents were not directly inspected after the environment outage. |
| macOS SignalPath CI | 6/7 CTests PASS; SignalPath FAIL. Uploaded artifact is partial and its contents were not directly inspected. |
| Final required-CI qualification | BLOCKED by required macOS failure. |
| Final Python CI/gate verifier | Not executed after the local runtime became unavailable. |
| Installed product / actual DAW / screen reader / public approval | Not completed or granted by this work. |

Native coverage exercised six compact stages plus ROUTING, four routing configurations, four scales, click/keyboard/accessibility actions, focus under automation, detailed/compact selection, both Gate positions,48 factory presets and existing project/A-B/PRE/editor restoration semantics. The local and Linux visual reviews are representative samples. Some Linux screenshots contain transient native tooltips; the originals were not retouched.

At **2026-10-13 04:02:27–04:02:28 UTC (13:02 KST)**, the exact-source API snapshot contained **9 workflows:6 successful,1 failed,2 in progress**; **25 created jobs:20 successful,1 failed,4 in progress**. Product and Update continued running. Their later result is outside this point-in-time record. The downstream assemble/consolidate jobs were not yet created; this record does not claim27 completed jobs. All captured runs used attempt1 and the stated source. PR metadata was refreshed at04:05:23 UTC.

## Evidence index and provenance

- [CI metadata review](ci/metadata-observed-summary.json), [original runs](ci/original-runs.json), [original PR](ci/original-pr-60.json), and [capture manifest](ci/capture-manifest.json).
- [Offline-contract observed summary](ci/offline-contracts-observed-summary.json).
- [Linux direct-inspection observation](ui/linux-derived-observation-report.json).
- [Windows original job log](ui/windows-original-job-115114297965.log) and [derived observation](ui/windows-derived-observation-report.json).
- [macOS original job log](ui/macos-original-job-115114298383.log) and [derived observation](ui/macos-derived-observation-report.json).
- [Original artifact API metadata](ui/original-artifact-api-metadata.json).
- [Local native observation and build/runtime limitation](native/local-derived-observation.json).
- [Root source-scope and native visual observation](review/root-derived-observation.json).

The CI directory contains20 original API response bodies, two explicitly derived observed summaries and a capture manifest. The job logs preserve the decoded text returned by the GitHub connector. Structured artifact metadata is serialized as JSON. Derived reports are observations and analysis, not reconstructed original local files.

Local raw logs, manifests, JUnit and image-integrity reports were written and read before the execution service disconnected. The service subsequently returned `environment_offline`. The additional six local CTests never started, and final local disk/process state and raw-file export could not be verified. Their absence from this branch is explicit; no unavailable original log or hash inventory has been fabricated.

## Scope and next acceptance

The original comparison baseline was `e7b00cb09c5431d86c03282392eafddbe8edbfdd`. PR#55 independently advanced to `c3e9d436fbdc9b8778b592f333c93037b012b8c5`; the development branch merged that existing base to reconcile the earlier source/workflow mismatch. This task did not write to or merge #55.

The feature differs from c3 in eight UI/test/documentation files, with production edits only in SignalPathView and PluginEditor. DSP, routing authority, parameter/preset definitions and serialization, workflows, installers, policies and receipts are inherited unchanged. Upstream caption/display-name changes are distinct from this UI feature.

Hold October14 inclusion. Keep the work in the conditional **October21 test → October22 candidate → October23 beta.3** slot. First diagnose and fix the demonstrated macOS failure, freeze the new complete source, and rerun required exact-source CI and release contracts. Then inspect matching packages/payloads/evidence/update feeds and finish actual installed DAW, screen-reader, upgrade/repair/removal and personal-data preservation acceptance. Public approval remains bound to the selected version/SHA and existing policy; it has not been granted.
