# PR42: completed native verification at 47345c81

**All six source-specific CI workflows completed successfully on their original attempt. The current release gate remains BLOCKED: 21 PASS, 107 NOT_EXECUTED, `ready=false`. PR42 stays Draft.**

The build, automated tests, installer checks and package-byte audits described here are complete. The unexecuted release requirements remain visible; this record does not authorize a merge or publish a release.

## Source and change

- Repository: [raven-deadwire/SpectralForge-Chimera-Amp-Matrix](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix)
- PR: [#42](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/42)
- Tested source: `47345c81db8aefbf9ae51a2684d546119bb9ae32`
- Tested tree: `80525e0dda220cbc87c229b3e5234d2ddc38fe78`
- Parent: `8097ec69594ff728164c77f59e9ea1e39e384643`
- Package version: `1.3.0-beta.1`; product/installer version: `1.3.0`; build ID: `47345c81db`.

The convolution history is stored in banks selected by the cursor residue. For a cursor `C = q*S + r`, a bank contains the history read in partition order, replacing repeated strided history-address calculations with contiguous bank access. The partition accumulation order remains the reference order, including the placement of partition zero. Each microphone and each fading state retains its private history.

The source change preserves FFT and IR geometry, history capacity, sound-model calculations, accumulation order, partial-block handling, reset behavior, zero reported latency, 50ms fades, worker workload, automation and strict timing thresholds. The new reference test checks this exact-output contract against the frozen old history layout.

All **179 Source/Tests files** in this source are byte-identical to the native-tested `cded293f` candidate. The full source has 780 tracked files. The six-file integration consists of the convolution header, existing realtime test file, new frozen reference header, one header-path filter in each CAB workflow, and the prior native comparison archive. No generator, QoS, FFT-size or relaxed-threshold experiment was incorporated.

Source references: [convolution header](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Source/ModeledCabConvolution.h), [realtime regression test](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Tests/CabRealtimeRegressionTests.cpp), [frozen reference](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Tests/CabResidueBankReference.h).

## Completed native runs

All runs below used source `47345c81` and attempt 1. No CI rerun was requested to obtain these results.

| Workflow | Result | Audited scope |
| --- | --- | --- |
| [Product, three OSes — 38091545238](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545238) | SUCCESS | 117/117 CTests; 198/198 CAB p99 conditions; native packaging, Windows installer/MSIX/Defender, assembly and gate production |
| [Windows Candidate — 38091545225](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545225) | SUCCESS | 41/41 CTests; 66/66 CAB p99; its own Setup and VST3 lifetime evidence |
| [Dedicated CAB — 38091545218](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545218) | SUCCESS | 5/5 CTests and 66/66 p99 per OS: 15/15 and 198/198 total |
| [CAB Panel — 38091545278](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545278) | SUCCESS | 17/17 CTests per OS, 51/51 total; exact-source full logs and residue guard |
| [Niflheimr technical — 38091545248](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545248) | SUCCESS | 6/6 synthetic numerical and renderer contracts |
| [Niflheimr DI contracts — 38091545240](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545240) | SUCCESS | 9/9 synthetic renderer/route/tool contracts |

The total is **239 successful source-specific CTest executions** across these workflows. This counts repeated executions of overlapping tests, not 239 distinct tests, and excludes older reference runs. Synthetic DI contracts do not supply listening approval or real-DAW evidence.

### Timing populations and retained counterexamples

Each normal CAB population contains 66 conditions, 105,600 raw callbacks and 198 paired records. Every condition retains 1,600 measured callbacks, the original automation and 1,200 worker/automation plus 400 forced-publication phase layout.

| Population | CTests | Strict p99 | Individual misses |
| --- | ---: | ---: | ---: |
| Product Windows | 41/41 | 66/66 | 0 |
| Product Linux | 38/38 | 66/66 | 5 |
| Product macOS | 38/38 | 66/66 | 5 |
| Windows Candidate | 41/41 | 66/66 | 1 |
| Dedicated Windows | 5/5 | 66/66 | 3 |
| Dedicated Linux | 5/5 | 66/66 | 0 |
| Dedicated macOS | 5/5 | 66/66 | 46 |

Across these seven separate populations, **462/462 strict p99 conditions pass**, with **739,200 callbacks, 1,386 paired records and 60 individual misses**. The count includes every observed miss; passing p99 does not mean every callback met its deadline. No callback populations or quantiles were pooled across jobs. The three separately instrumented dedicated profiles pass 36/36 conditions over 57,600 callbacks and retain ten additional misses; these profiles and the fixed old baselines are excluded from normal totals.

The Product Linux misses are expanded-v2 integration 96k/64/mono worker callbacks 424, 426, 427, 431 and 432. All five also exceed the same-callback CPU budget; the maximum is 961.680us wall and 962.938us CPU. Product Mac has five worker misses, four with paired CPU above budget; the other is expanded extended 96k/32/stereo callback 188 at 583.792us wall and 196.958us CPU. Candidate Windows retains one expanded extended 96k/32/mono worker miss at callback 471: 450.5us wall and 186,392 native thread cycles. The dedicated runs retain all 49 of their separate misses, including forced-publication counterexamples.

Selected Product p99 values, computed separately within each native execution:

| Product platform | Expanded 96k/32/stereo; budget 333.333us | 6x10 96k/64/mono; budget 666.667us | 6x10 96k/64/stereo; budget 666.667us |
| --- | ---: | ---: | ---: |
| Windows | 110.8us | 124.8us | 173.1us |
| Linux | 166.902us | 239.598us | 267.690us |
| macOS | 211.125us | 132.292us | 213.750us |

The formerly failing dedicated Mac focal condition passes at 181.458us in its current dedicated run. The formerly failing Windows Candidate 6x10 mono condition passes at 62.8us in the current Candidate run. These are source-specific observations, not a causal speedup estimate against different hosted machines. In particular, the Product Windows CPU model was not collected; it is not filled from the separate Candidate runner.

Windows provides valid native thread cycles, which were not converted to microseconds. Mac and Linux provide native thread CPU duration. There are no reported clock errors in the audited normal populations. Mac/Windows worker-low observations are native; Linux reports a requested-only low priority with the non-realtime request ignored. No effective Linux priority change is claimed. The seven normal populations record 5,882 canceled builds in total, and callback allocation reports remain zero.

### Sound and state invariants

The new guard passes **2,645,088 exact float samples and 388,800 fade-mix samples per execution**, with 18 indexing cases, 44,288 indexing reads, 80 streams, 40 reset cases, six independent microphone histories, two full wraps and fixed/1/17/63/max/3 partial chunks. The original 144-route oracle and eight six-mic direct-FIR routes remain intact and pass, as do the v1/v2/v3 exact model-bit guards. Zero reported latency and 50ms fades remain part of the checks.

Full native LastTest logs were compared with the original CTest inventory and JUnit names, statuses and durations. Truncated JUnit output was preserved; timing and exact-output checks use the full logs. This provides numerical and lifecycle evidence within the tested scope, not a subjective sound-quality approval.

## Actual packages and installer verification

These are the **Product run 38091545238** files. The five actual package bytes match the independently acquired assembly checksum list, and the three update assets match the corresponding filenames, architectures, sizes and hashes.

| Product file | Bytes | SHA256 |
| --- | ---: | --- |
| Windows Setup EXE | 117,887,024 | `66a18b59847e9eae74ab4e1b72e1597c3009e9f0f2d329c8d6c76289eee7582d` |
| Windows portable ZIP | 128,308,392 | `ef03429d194f5ff42b4ef0adf3c01e6bc1eebe871163aa891b2ddebe490b1fbf` |
| macOS universal PKG | 388,181,620 | `161c91665fdc09b5915fc0bf35e2853fdac702471f4e91e537da596ad1fe273c` |
| Linux x86_64 DEB | 119,459,268 | `8fc09993b7229bc96b0c7af0a1173869507608508ff34cc9b51210d2b00ca7ae` |
| Linux x86_64 tar.gz | 130,668,846 | `eb52096c82444b8ed5e075d01eea19398c4ad835703b9606a30e2cd37165f8a7` |

The separate Windows review MSIX is **63,793,296 bytes**, SHA256 `f904d355eeaf6168365fe2628f60887acc91c9bd44aa68127dc64fc951914018`. It is not one of the five assembly checksum entries.

Windows Product verifies 23 native installer assertions and nine complete install/repair/remove logs: selectable Unicode paths, remembered VST3 repair, retirement of only the old owned plugin when a path moves, shortcuts/application launch, static MSVC runtime, user preset/IR preservation, VST3-only and standalone-only selections, and the companion fixtures. The native installer receipt uses the same payload-manifest hash as the fully checked portable package. The portable package contains 33 manifest-listed files plus its manifest, including actual x86_64 Standalone, VST3 and reference-renderer PE files; its Verification.txt is byte-identical to the Product full LastTest.

MSIX has five native PASS records. Its embedded application is byte-identical to the same-run portable application. The native deployment test repacks a CI-isolated unsigned identity and uses `Add-AppxPackage -AllowUnsigned`; it does not install the distributed Review identity or certify publisher signing or the Store. It preserves the external user IR library on removal and does not claim a virtualized VST3 installation.

The Product Defender custom scan records 54 input files, no detections and unchanged input hashes. Actual Setup, portable and MSIX hashes match that inventory. The native status records `RealTimeProtectionEnabled=false`; this result verifies the explicit custom scan and does not claim enabled real-time protection. Files remain unsigned and neither Store certification nor browser reputation is asserted.

The Mac PKG has 36 manifest-listed payload files plus its manifest. Actual Standalone, VST3 and AU binaries contain both arm64 and x86_64 slices. Native code-seal and expanded-payload checks pass. Tests execute on the native Mac runner; packaged x86_64 execution, actual system installation, Developer ID and notarization are not certified.

The Linux DEB has 24 manifest-listed files plus its manifest; the portable archive has 28 regular files including its installer and dependency instructions. Actual x86_64 ELF headers, control/dependency metadata and all payload hashes match. The native builder's package extraction and dependency checks pass. Desktop audio, DAW scanning and installation on other distributions remain outside this evidence.

All three Product platforms package their own full LastTest bytes. Candidate Windows has a **separate** Setup: 117,889,183 bytes, SHA256 `9af62f77363b4809ff04c9821f2cb8f5f5ab68384f489084cc1fc3f1d385a5ac`. Its six transferred parts reassemble to the exact receipt/Defender hash, with its own 23 installer assertions and nine logs. Candidate VST3 lifetime tests pass seven scenarios and nine final module unloads; the synthetic native host is not Studio One itself. Neither this Setup nor older 8097 packages were substituted into Product assembly.

The redundant combined Release-Candidate ZIP container was not independently downloaded. The byte audit covers all five constituent package files and their assembly manifests. The candidate-source record explicitly says `published=false`, `publisherSigned=false` and `macOSNotarized=false`.

## Current release gate and concrete remaining work

The final gate is from **this source and Product run**, not a carried-forward result from 8097. Four independently acquired native bundles—Windows, Linux, Mac and assembly—match the bundles embedded in the final artifact byte for byte. Recomputing the existing policy from those raw inputs reproduces the entire gate JSON, Markdown and all 128 individual check reports exactly.

- Artifact: [11684948778 in Product run 38091545238](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38091545238/artifacts/11684948778)
- Artifact ZIP SHA256: `3f75a6241a04a9666372677e8f535f9ec61841daa4b97f139e195791c724d390`
- Native `release-gate.json`: 83,996 bytes, SHA256 `d7d20814bf2763af645743c34ba555ff712d95272b97725d45a2ece7cce71c05`
- Required checks: **128 total, 21 PASS, 107 BLOCKED**, all 107 with reason `NOT_EXECUTED`; no NOT_APPLICABLE substitutions.
- Hard gates: **63 total, 21 PASS, 42 BLOCKED**.
- Producer infrastructure errors: **none**.
- Verdict: **BLOCKED**, `ready=false`.

The consolidator correctly produces a valid blocked verdict while its infrastructure job succeeds. A green build badge therefore does not authorize a release. Existing waivers and acceptance policy were not weakened.

The current CI map owns 21 required IDs. The 107 remaining checks fall into two different gaps:

| Gap | Required IDs | What is needed next |
| --- | ---: | --- |
| Legacy A1–A11 and A14 numbered checks | 86 | Define each retained assertion, fixture, metric/limit and producer. The reviewed policy/map provides stage titles and numbers but not those individual definitions. Installer, documentation, recall, fallback and JSON checks may be automatable once defined. Hartke/Marshall import checks also need an identified appropriate IR fixture and expected result. |
| B, E2 and I2 evidence | 21 | Defined same-DI/reference inputs, renders, measurements and scoring; a listening result; audio/state roundtrip; actual Windows-host and Studio One 8 evidence; the specified 30-minute soak; reference calibration/equivalence; real-input/UI-audio timing/live-removal evidence; and the RC decision contract. |

These are not all hardware-only or manual tasks. Recorded DI fixtures can support automated rendering and measurements, and suitable host/soak scenarios may be scripted. Conversely, synthetic renderer tests do not supply a listening judgment or actual Studio One acceptance, and a CI job that lasts more than 30 minutes is not the specified continuous soak. Existing A12/A13 passes cannot be silently relabelled as different legacy IDs.

The [static definition audit](cab-release-requirements-47345c81.md) lists the exact 107 IDs, their stage titles, mapped ownership and hard-gate counts. Its complete JSON is embedded in the aggregate archive. Suggested next evidence is explicitly inferred from the policy titles; absent per-ID thresholds and the meaning of TD_EQ were not invented. The separate deferred transpose checks `I2.PITCH_LIVE` and `I2.PITCH_DI` belong to the follow-up profile and are not part of these 128/107 counts.

Sources: [release policy at 47345c81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Validation/release-policy.json) and [CI producer map](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Validation/ci-check-map.json).

## Evidence, independent review and limits

The accompanying [archive index](cab-native-47345c81-index.json) binds the four evidence archives by byte size, SHA256 and Git blob ID:

- [Windows evidence](cab-native-47345c81-windows.json)
- [Mac evidence](cab-native-47345c81-macos.json)
- [Linux evidence](cab-native-47345c81-linux.json)
- [Aggregate, packages and release-gate evidence](cab-native-47345c81-aggregate.json)

Each embedded record preserves its exact original UTF-8 bytes, including original line endings and BOM where present, with an independent size/hash record. The [record selection](cab-native-47345c81-records.json) contains the finalized source/provenance/route/miss audits, indexes, audit code, full test logs, small native installer/security receipts, independent cross-reviews and the complete final gate extraction. The archive commit is an evidence-only descendant of the tested source; PR42's tested source branch remains at `47345c81`.

The archives do **not** embed every raw callback CSV, native executable, binary installer or the large VST3 trace. Their exact artifact/file hashes and detailed route/paired-outlier evidence are retained. The corresponding full Actions artifacts have the recorded 90-day retention, expiring 2027-01-08. Individual native package bytes were acquired and verified; dedicated diagnostic executable verification is limited to recorded build/before/after hash receipts because those executable bytes are not in the artifacts. Technical/DI verification uses the complete native job logs and scoped checks; their full artifact containers were not downloaded.

Independent review checked the Linux Product source/full-test/miss/package bindings, Windows Product source/full-test and focal raw callback/cycle bindings, the Mac result wording, and the root evidence tools. The assembly auditor was strengthened to require a clean tested worktree before importing policy code. No native acceptance rerun followed these evidence-tool reviews.

Audit-only reader issues are preserved separately from native outcomes. The first local Mac XAR reader appended extra bytes at the Payload boundary; the final reader verifies the compressed TOC and every stored/extracted checksum and exact offset/length, while the original error and reader remain preserved. A Windows cross-review parser initially mishandled CRLF; normalization was confined to in-memory parsing and the original bytes remained unchanged. The original truncated JUnit outputs and preflight notes are retained. Two optional intermediate Windows Markdown reports failed exact UTF-8 decoding; their byte bodies are excluded, with their original hashes, error records, indexes and JSON counterparts retained. All 154 files of the separate final native gate and all four original release bundles are included.

The Product Setup directory also retains a 90,177,536-byte partial file that is an exact prefix of the completed, API-hash-matching ZIP. Its existence is not explained by the normal recorded helper path. The partial and completion record are retained, and the total number of underlying HTTP requests is not asserted. The completed ZIP, extracted Setup, native installer receipt, Defender inventory and assembly checksum all agree. This observation is not a native build/test failure or a reason to discard a timing sample.

The prior fixed A/B comparison remains at [the original native archive](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Validation/cab-residue-bank-native-8fa4e85b.json). Its failed Mac reference profile, all 233 individual misses, eight Windows and fourteen Mac normal-route p99 regressions remain recorded. Current CI passes do not rewrite those historical observations or establish uniform performance improvement.

**Disposition:** the intended source change and current-source automated validation are complete. Keep PR42 Draft and unpublished until the missing release definitions and source-bound acceptance evidence are completed. PR41 is outside this work.
