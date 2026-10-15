# PR73 instruction-level follow-up

This diagnostic is separate from PR74's completed 1,843,200-callback comparison. It never merges PR73 or approves a release. Production files in this branch remain the scalar parent inherited by PR74. Candidate headers are generated in an evidence directory, and the full-product experiment modifies only a disposable worktree.

## Fixed identities and compiler contract

- Scalar parent: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
- PR73: `29865abb4da3ab78592c970a263ee62af63fc673`.
- JUCE: `d6181bde38d858c283c3b7bf699ce6340c050b5d`.
- Native Apple Silicon, AppleClang, `-O3 -DNDEBUG -std=gnu++20 -arch arm64 -mmacosx-version-min=12.0`; no fast-math or altered FP contraction option.

`run.py` extracts the exact accumulate bodies rather than maintaining a second hand-written reference. It emits ARM64 assembly and optimization remarks for the scalar source, PR73, two NEON unroll factors, two scalar interleave factors, and bounded-loop candidates. The extracted functions are deliberately noinline: they isolate code generation but do not reproduce product inlining.

`probe.cpp` checks bit identity for 20 bin counts including all production FFT sizes, four base alignments, short/odd tails, finite varied exponents, signed zeros and subnormal values. Output sentinels must also remain unchanged. It is not a NaN-payload or exhaustive floating-point-domain proof. Existing frozen full-product tests remain authoritative.

Six ABBA/BAAB and interleaved A/A quartets compare hot/streaming kernel working sets. The identical scalar function/binary is used for both A/A arms. Raw samples, output hashes, compiler flags and manifests are retained. The PR74 conservative quartet analysis is reused. Short microbenchmarks can have large A/A envelopes and cannot demonstrate full CAB callback improvement. No failed population is discarded or rerun to obtain a pass.

## Minimal candidates

- `neon8`: add only `#pragma clang loop unroll_count(2)` to PR73.
- `neon16`: use unroll count 4; diagnostic for excessive code expansion.
- `scalar_ic1/2`: keep original arithmetic and specify vector width 4 / interleave 1 or 2.
- `bound4`: replace `i+4<=bins` with `i<vectorBins`, where `vectorBins=bins & ~3`.
- `bound8`: bounded loop plus unroll count 2.

All candidates preserve the two-step FMA/FMS expression, partition accumulation order, scalar remainder, spectrum layout and buffers. The bounded-loop changes are Apple ARM64-only. They do not introduce alignment assumptions, prefetch, allocation, locks, FFT/IR changes, latency, parameter/schema changes or relaxed deadlines.

## Actual product inspection and gates

`products.py` emits assembly for the **actual OriginalCabIntegrationTests.cpp translation unit** using its CMake compiler command. It compares scalar A, PR73 B, bound4 C4 and bound8 C8 under the same path, flags and dependencies. It preserves full assembly, inlining/vectorization/unroll remarks and exact header hashes/patches. Only assembly emission and reporting flags differ from the recorded command.

C8 is a preselected mechanism probe, not a winner selected from noisy timing. The disposable product worktree builds the original CAB integration and realtime regression tests and executes all five unchanged CTests, 66 deadline routes, frozen output bits, 144 realtime routes and zero-latency assertions. A subsequent single control/profile screen preserves all raw 1,600-callback populations for every 96k/32/64 engine/channel route. That screen is **not** an independent A/C performance acceptance test and must not be compared directly with PR74 timings from another runner allocation.

The report distinguishes collection completeness, gate results, and optimization eligibility. `performance_demonstrated`, `integration_eligible` and `release_approved` remain false. A failed safety/deadline gate disqualifies the candidate even if collection completes.

## Run

```sh
python Tools/CabNeonCodegen/run.py --output evidence
# Only the newly added bounded candidates need new timing when initial candidates were already collected:
python Tools/CabNeonCodegen/run.py --output evidence --new-candidates-only
python Tools/CabNeonCodegen/products.py --kernels evidence --output product-evidence
python Tools/CabNeonCodegen/audit.py extracted-artifact --output audit.json
```

Use fresh output directories. The workflow uploads source hashes, raw measurements, generated candidate patches, assembly and logs, but excludes dependency and product build directories. Full product, DAW/device acceptance and release decisions remain separate.

## Member-alias follow-up

Actual CAB translation-unit assembly showed two `bins` member reloads after NEON stores per four-bin iteration. The by-value extracted kernel cannot model that behavior. The bounded candidates did not remove it and are not accepted optimizations.

The `--snapshot` product mode therefore generates three new, independent Apple-only variants: S4 snapshots `bins` once per accumulate call; S8 also unrolls by two; S8T additionally disables vectorization/interleaving only for the scalar remainder. Production metadata, buffer sizes, arithmetic and partition order are unchanged. It emits full assembly for S4/S8/S8T and LLVM IR for original A, original B and selected S8T. Only the preselected S8T receives full correctness/deadline gates and a one-shot callback screen. S4/S8 code-generation evidence alone is not a correctness/performance acceptance.

```sh
python Tools/CabNeonCodegen/run.py --output evidence --prepare-only
python Tools/CabNeonCodegen/products.py --kernels evidence --output product-evidence --snapshot
```

Prepare-only preserves exact source material and the environment without repeating the completed extracted-kernel timing populations. Compare the actual production method assembly, rather than assuming a by-value kernel reproduces member alias analysis. The `C8-build.log` filename is retained by the common runner; `report.json.candidate` identifies the actual selected candidate.
