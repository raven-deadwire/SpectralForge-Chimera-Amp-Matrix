# PR73 NEON structural investigation — integration HOLD

2026-10-15. This follows PR74's completed independent performance audit. It does not repeat, replace or reinterpret the original 1,843,200-callback population. PR73 remains Draft; no merge, package or release action is included.

## Findings confirmed in native extracted-kernel code

The scalar source is **not scalar machine code**. AppleClang 17.0.0 (`clang-1700.0.13.5`, Xcode 16.4) on Apple M1 emits an 8-bin loop: two 128-bit groups, six paired vector loads, two paired stores, six FMLA and two FMLS operations. PR73 emits a 4-bin loop with six individual vector loads, two stores, three FMLA and one FMLS.

| Property | Original auto-vectorized loop | PR73 manual NEON loop |
|---|---:|---:|
| Bins per back edge | 8 | 4 |
| Static instructions per iteration | 24 | 18 |
| Instructions normalized to 8 bins | 24 | 36 |
| Load/store instructions per 8 bins | 6 LDP + 2 STP | 12 LDR + 4 STR |
| FP operations per 8 bins | 6 FMLA + 2 FMLS | 6 FMLA + 2 FMLS |
| Logical bytes per bin/partition | 24 read + 8 write | 24 read + 8 write |
| Stack accesses inside main loop | 0 | 0 |

These are emitted static instructions, not retired instruction counts or cycle estimates. LDP/STP transfer twice as much data as a single 128-bit LDR/STR; memory **instruction count** increases while logical memory **volume** stays unchanged. Cache misses, external bandwidth, micro-ops and physical-core utilization were not measured. Therefore the table supports a structural explanation, not a precise allocation of PR74's +40.00% callback cost to one instruction category.

The auto-vectorized loop interleaves two independent real/imaginary groups. The manual loop exposes fewer independent accumulators per iteration and more back-edge/address work per bin. Both retain the same two-step FMA/FMS ordering for each bin; there is no deinterleave, lane extraction, shuffle, precision conversion or additional multiplication explaining the regression.

## Epilogue and register pressure

The extracted PR73 function includes a second auto-vectorized remainder path plus runtime checks even though production bin counts make the remainder small. The whole standalone function grows from 105 to 157 static instructions. It saves/restores x19–x24 using a 48-byte stack frame; the original extracted scalar function needs no such frame. These are function-level GPR saves/restores, **not vector spills in the hot loop**. Product inlining can change their relevance and is inspected separately below.

A pragma-only twofold unroll produces a 29-instruction 8-bin main loop, not the original 24-instruction loop; most paired memory operations are not recovered. Fourfold unrolling expands the standalone function to 235 instructions. Unrolling alone therefore does not automatically restore the compiler's original code shape.

## Isolated timing is inconclusive

[Initial native run 38725479052](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38725479052), controller `604efbb67a474f92f34412c3daf165cf3113378c`, completed 30 candidate/working-set comparisons using six ABBA/BAAB and six interleaved A/A quartets each. None met the prespecified improvement rule. A/A variability was substantial in several short microbenchmark populations; no outliers were discarded. These results must not be substituted for PR74's full callback regression evidence.

The initial exact-bit probe passed **9,612,800 compared float samples**, including production bin sizes, misaligned bases, odd tails, varied finite exponents, signed zeros and subnormals. The full output buffer comparison includes guard values. This is not exhaustive floating-point validation and does not cover NaN-payload behavior.

## Minimal changes and discriminating tests

| Hypothesis | Minimal change | Discriminating evidence |
|---|---|---|
| Lost 8-bin interleaving/back-edge efficiency | Add NEON unroll count 2 | Main-loop instructions, paired memory operations, same-bit kernel comparison |
| Remainder proof and unnecessary code expansion | Compute `vectorBins=bins & ~3`; loop while `i<vectorBins` | Whether redundant vector remainder/checks/GPR frame disappear |
| Combined main-loop and remainder costs | Bounded loop plus unroll count 2 (`bound8`) | Actual product codegen, unchanged frozen output/latency/deadline gates |
| Excessive unroll/register demand | Unroll count 4 control | Code growth and stack traffic; not automatically a preferred candidate |
| Existing compiler vectorization is already preferable | Remove Apple manual branch, retain exact scalar source | PR74's existing scalar control; conservative fallback rather than a novel improvement |

All changes retain per-bin and per-partition arithmetic order. Splitting accumulators across partitions, reassociating sums, fast-math, changing FMA policy, padding the spectra or adding latency is outside this experiment.

## Actual production methods in the CAB integration translation unit

[Run 38725825004](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38725825004), controller `ca324e93ee3575cf39285872e253bd6a3b91deff`, emitted assembly using the real `OriginalCabIntegrationTests.cpp` CMake command. This is the same production header in a CAB test translation unit, not a shipping plug-in executable disassembly.

**A stronger difference appears only in the member-function context:** PR73 reloads the `bins` member (`ldrsw ..., [x20,#28]`) twice per four bins, after its NEON stores, and recomputes the imaginary-plane address. The extracted free-function benchmark passes `bins` by value and cannot expose this cost. The production hot loop contains 23 instructions per four bins (46 per eight), versus the original 24 per eight. Array bytes remain unchanged, but PR73 additionally performs metadata reads.

| Actual emitted function | Scalar A | PR73 B | Bounded C4 | Bounded + unroll C8 |
|---|---:|---:|---:|---:|
| `sumPartitions` static instructions | 320 | 424 | 428 | 546 |
| Static stack-access instructions in entire function | 17 | 29 | 29 | 33 |
| `process` static instructions | 368 | 368 | 368 | 368 |
| `processMono` static instructions | 219 | 219 | 219 | 219 |

`accumulate` is inlined into `sumPartitions` in all four outputs. `sumPartitions` remains out-of-line in both mono/stereo processing in all four. Therefore a newly introduced `accumulate` call or lost `sumPartitions` inlining is **not supported** as the cause in this translation unit. The `sumPartitions` frame grows from 128 to 176 bytes for A→B; the larger GPR working set and stack traffic are real, but the hot MAC loop has no vector-register spill.

The bound-only hypothesis was falsified as a fix on this compiler: C4 retains member reloads and redundant remainder code. C8 still has four member reloads per eight bins and grows code further. Neither is accepted as an optimization. This is a useful negative result rather than a reason to repeat or select favorable timings.

C8 passed all **5 original CTests, 66 deadline rows, 144 realtime routes, 2,645,088 frozen output floats + 388,800 fade-mix floats, immutable spectra and zero added latency**. All eight extracted kernel variants passed **13,457,920 compared floats**. No bounded microbenchmark comparison met the improvement rule (0/12).

The one-shot C8 callback screen completed every route and retained **38,400 raw callbacks**, with **48 wall misses in control mode** and **37 in separately instrumented profile mode**. Passing p99 gates does not imply zero misses. These counts are descriptive; they are not compared with another runner's counts or used as evidence of improvement.

This evidence motivated the next minimal hypothesis: copy `bins` to a local scalar once on Apple ARM64, then separately test 2× unrolling and disabling re-vectorization of the scalar remainder. That directly tests member reloads instead of assuming a masked bound eliminates them. Native assembly and LLVM IR are used to inspect the compiler's alias treatment.

## Verified compiler-alias mechanism and final experimental candidate

[Snapshot native run 38726972242](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38726972242), controller `8666aa182be57764de687eda21dba7b3c47dc2f7`, completed on Apple M1 with the same compiler flags. LLVM IR confirms the relevant difference: auto-vectorized scalar stores carry `!tbaa` identifying **float**, plus alias-scope/noalias metadata. The explicit NEON stores in the real vector loop carry **no such TBAA/alias-scope metadata**. The separately auto-vectorized remainder stores still have metadata; those must not be mistaken for the explicit NEON loop.

This conservative alias treatment is consistent with the integer `bins` reloads seen after NEON stores. A direct source counterfactual confirms the member-read mechanism: S4 copies `bins` once to a local scalar and removes the per-vector member reloads without changing FMA/FMS, buffers or partition arithmetic. S8 additionally unrolls twice. S8T also disables vectorization/interleaving of the scalar remainder, eliminating the redundant vector remainder machinery.

| Actual `sumPartitions` | PR73 B | S4 local snapshot | S8 snapshot + unroll | S8T + scalar-only remainder |
|---|---:|---:|---:|---:|
| Static instructions | 424 | 407 | 498 | **272** |
| Entire-function stack-access instructions | 29 | 29 | 31 | **12** |
| Stack frame bytes | 176 | 160 | 160 | **96** |
| `bins` member reloads inside main vector iteration | 2 / 4 bins | **0** | **0** | **0** |
| Main vector loop instructions per 8 bins | 46 | 36 | 29 | **29** |

The original automatic loop still has 24 instructions per eight bins; S8T has not recovered all paired loads/stores. Smaller function size and removal of member dependencies are **verified code-generation improvements**, not a measured percentage CPU improvement. No hardware-counter causal percentage is claimed.

S8T is an independent **experimental candidate**, preserved as `snapshot-candidate/S8T.patch` against pinned PR73. Its changes are limited to an Apple ARM64 local bin-count copy, twofold unroll hint, and scalar-tail vectorization/interleaving disable. The Windows and other-platform path is retained. All production files in this diagnostic branch remain unchanged.

S8T passed the original **5/5 CTests, 66/66 deadline rows, 144 realtime equivalence routes, 2,645,088 exact output floats + 388,800 fade-mix floats, immutable spectra and zero added latency**. These are original tests and thresholds; no tolerances or timing limits were relaxed. The preceding 13,457,920-float extracted test covers the earlier eight kernel variants, **not S8T**; S8T's numeric evidence is the full-product frozen test above.

The final one-shot screen retained 38,400 callbacks: control-mode wall misses **14/19,200**, separately instrumented profile-mode misses **16/19,200**. All screen processes passed their unchanged p99 assertions. These counts are neither zero nor a proven reduction: the C8 screen used a different allocation and must not be used for a cross-run speedup/miss comparison.

### Decision and next discriminating experiment

- Structural investigation: complete for the observed compiler/code paths.
- Confirmed contributors: lost original eight-bin code shape; missing alias metadata leading to repeated member loads/address dependencies; redundant vectorized remainder and larger GPR/stack/code footprint.
- Not established: vector spills in the hot MAC, new accumulate calls, cache misses or memory-bandwidth saturation, or numerical cost allocation of PR74's slowdown.
- Candidate S8T output/latency/existing deadline contracts: PASS on this native run.
- Full-callback independent performance versus scalar A: **NOT YET DEMONSTRATED**. The completed PR74 population is not repeated or replaced.
- Required next acceptance experiment: pinned scalar A versus the S8T-derived product, same allocation/compiler, original full callback ABBA/BAAB and A/A protocol, fixed-workload phases, both clock modes, unchanged gates, all failed populations retained. Include PR73 B only as a secondary restoration control if needed; the acceptance target remains A.
- PR73 and S8T integration: **HOLD**. Full product/DAW/device acceptance remains separate; release approval is false.

## Provenance and scope

- Scalar: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
- PR73: `29865abb4da3ab78592c970a263ee62af63fc673`.
- Production A/B diff: only `Source/ModeledCabConvolution.h`.
- First artifact: `11968481365`; SHA-256 `9834b7e59b8a176fdec3a3f5a48a485fb96fff86d24d8d56722b46495cf71a87`.
- Initial artifact manifest: 20 files independently rehashed after download.
- Bounded/product artifact: `11968398089`; SHA-256 `224fe7b142d9988c0849ed3be51d8486570e0ffad6cc2eb4ca97972a1b439882`; 151 manifest entries independently rehashed.
- Preserved manifests refer to complete downloaded archives; the repository carries compact evidence subsets and exact assembly excerpts.
- Snapshot artifact: `11969560298`; SHA-256 `faf9d16b423d73caf329cbfdda21758578e9210ce930e84f78da186ac509b95b`; 145 manifest entries independently rehashed.
- Native flags: O3, gnu++20, arm64, minimum macOS 12.0; no fast-math/target-CPU override.
- Compilation/reporting tooling and experiment details: `Tools/CabNeonCodegen/README.md`.
- Compiler pragma semantics: [Clang language extensions](https://clang.llvm.org/docs/LanguageExtensions.html); [LLVM vectorizers](https://llvm.org/docs/Vectorizers.html). Native assembly, rather than compiler documentation alone, is the evidence for generated code.

Only instruction-level facts are confirmed here; exact hardware causal shares remain unmeasured. Full product, actual DAW/device/listening acceptance and release approval remain separate. `release_approved=false`.
