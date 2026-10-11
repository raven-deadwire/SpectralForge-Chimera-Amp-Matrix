# Unified CAB panel

Current expansion: see [CAB speaker/microphone v2](CAB_EXPANSION_V2.md).
The inventory and validation below describe the earlier v1 checkpoint.

Integrates the catalog from PR #30 (`2636fffe0c3c6da7bf7ad99f0a503ddf25ff2e6e`)
with PR #31's original spatial engine, including its swap-overlap optimization
(`a5db0bb8c7755ec3d03bf69b9513bc611bc0f0e8`). Both retain PR #29's schema-11
shared IR assets. This remains a draft prototype, `release_approved=false`.

## Available sound sources

| Selection | Actual audio | Spatial controls |
| --- | --- | --- |
| Captured IR catalog: 9 dynamic, 3 ribbon, 8 condenser identities | Selected factory or personal file; an identity does not create audio | Stored geometry is descriptive metadata only |
| Original Attack dynamic, Body ribbon, Detail condenser | Three independently authored v1 role responses | Guitar 4x12/bass 4x10 design, one of four units for each mic, position, distance, shared rear and tweeter |
| Chimera Strike | Catalog identity; a user-supplied IR can carry this label | No independent Strike response implemented; not mapped to Detail condenser |

Mic A and B independently enable the original response. The capture chooser
retains its file and reference while a model is active and labels it as a stored
capture. Selecting a capture switches only that slot back. Failed imports leave
its active mode unchanged. The library and capture details disclose capture-only
support and Strike's missing independent response. Original geometry controls are
disabled for inactive modeled slots; shared enclosure controls require at least
one modeled slot. Blending, level, polarity, signal delay and cuts remain common.

The original definitions remain reduced-order, linear acoustic approximations,
not measured hardware emulations. There are **three implemented role responses,
not twenty microphone emulations**. Selecting a unit means selecting one of the
four drivers in the chosen cabinet, not a new commercial driver model.

## Compatibility and regression coverage

- Source values 0–3, all existing IDs/ordinals, the original 39 parameter ranges,
  defaults and AU hint 7 are unchanged. Total parameter count remains 4824.
  Future Strike DSP support must use an explicit versioned, append-only
  extension; repurposing Detail or extending the existing three-choice mic
  range would change old normalized automation and is not compatible.
- Classic, Dual Blend, Dual Crossover and Matrix retain the same audio routes.
  Original-only, captured-only and mixed captured/modeled project recall use the
  production processor. Matrix low DI and oversampling checks remain.
- Original bytes stay in the SHA-256 asset table even when a modeled slot is
  active. Deleted-file recall, comparison transitions across source modes,
  invalid-import retention, duplicate names and equal-basename replacement are
  covered. Existing 4 MiB import and 64 MiB state limits remain unchanged.
- The integrated UI test exercises both microphones, every spatial control,
  host-to-UI automation, factory/personal capture reselection, active-source
  labels and retained metadata. Every spatial control is checked for a measurable
  change in production audio. Synthetic screenshots contain no private captures.
- The CAB workflow includes all eight suites from both branches, with exact PR
  head checkout on Linux, Windows and macOS, and uploads logs and synthetic UI.

## CPU evidence and limits

PR #31 run 37769476219 at `3e1daa9` passed macOS and Windows but failed Linux:
96 kHz/64-frame stereo CAB p99 was 1440.02 us against an unchanged 666.67 us block
period; mono was 689.709 us. This is a real unresolved failure at that source,
not a pass inferred from successful platforms. The subsequently integrated
`a5db0bb` retains the full response, zero added processing latency and 50 ms fades,
and bounds model swaps to one transitioning microphone per rig. It restores
uniform convolution to avoid synchronized large-tail bursts.

Subsequent exact-head [run 37771812929](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37771812929)
at `a5db0bb8c7755ec3d03bf69b9513bc611bc0f0e8` passed all seven parent-branch
suites on Linux, Windows and macOS. Artifact ZIP SHA-256 digests were checked
against GitHub's artifact records before reading `LastTest.log`.

| Parent PR #31: 96 kHz / 64 / stereo | p99 (us) | Block period (us) | Deadline misses |
| --- | ---: | ---: | ---: |
| Linux | 523.767 | 666.667 | 1/1600 |
| Windows | 288.600 | 666.667 | 0/1600 |
| macOS | 662.708 | 666.667 | 14/1600 |

All twelve cases pass the existing **p99** criterion. This is not a zero-miss
guarantee; macOS has little margin in the tightest case. The six-path impulse
residual remains below 4e-9 there, with watched C++ allocation/deletion counts 0.
These results prove the latest parent optimization, not the integration branch.
The complete parent timing rows and artifact identities are retained in
`Validation/cab-integration-parent-ci.json`.

The strict test still requires p99 below one block period for all 12 combinations
of 44.1/48/96 kHz, 64/256 frames and mono/stereo. Six mic paths and simultaneous
publication are retained. Audio-thread allocation/deletion, impulse equivalence,
transition continuity, worker teardown and latest-request convergence checks are
also retained. Exact-head integration CI results belong to the integration PR's
validation record; none are inferred from either parent PR. Runner CPU contracts
do not establish target-hardware headroom or commercial DAW/music acceptance.

## Integration validation status

Local Release validation used GCC 13 and JUCE 8.0.8. The final working-tree
source hashes and complete measured timing rows are recorded in
`Validation/cab-integration-local.json`; the embedded build revision was
`3a6136b`, so this is a local working-tree result, not exact-head CI.

- `ChimeraIRCollectionTests`: PASS, including all 20 identities and capability
  disclosures.
- `ChimeraCabPanelStateTests`: PASS, including integrated UI bindings, audio
  response changes, routing, mixed-source recall, retained IR bytes, legacy
  migration and frozen automation contracts.
- `OriginalCabModelTests`: PASS, including 864 boundary cases and all three
  sample rates, built directly with the same optimized C++20 configuration.
- `ChimeraOriginalCabIntegrationTests`: FAIL at the unchanged CPU p99 gate.
  The six-path response, transition/convergence and watched C++ allocation
  checks completed without a failure; callback new/delete counts were zero.
- The other four CAB workflow suites have not been run on this integration
  branch locally. Their parent-branch results do not substitute for that run.

At 96 kHz / 64 frames / stereo the local integration p99 was **3472.32 us**
against **666.667 us**, with 606/1600 deadline misses. To check whether this was
unique to integration, the exact `a5db0bb` source/test units were rebuilt with
the same Release flags and shared JUCE objects on the same host. That baseline
also failed: **3671.99 us**, 387/1600 misses. Cabinet DSP/model sources and the
CPU test are byte-identical to that parent. These observations do not establish
an integration-specific regression or isolate the host cause, and neither
failure is waived. No timing threshold, response length or test load was reduced.

UI components, attachments and software-rendered synthetic screenshots were
checked locally. Xvfb could not bind its display socket in this environment;
this is not native-window validation. The CTest exit status was separately
captured as 8 (one of three selected suites failed), independently of the
Xvfb wrapper's cleanup error.

**Acceptance remains BLOCKED.** Integration Linux/Windows/macOS CI has not run.
Automatic approval review rejected pushing `feature/cab-integrated-panel`
because the integration/testing request did not explicitly authorize remote
source upload. No remote integration branch or PR was created. The next step
requires approval to push this reviewable branch, open a draft PR and run its
eight-suite exact-head matrix. Parent CI success does not close this gate.
