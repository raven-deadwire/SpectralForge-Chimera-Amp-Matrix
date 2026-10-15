# Original A versus PR75 S8T — HOLD

[한국어 전체 검증 보고서](README_KO.md) · [Native run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38775107197) · [Raw artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38775107197/artifacts/11996322105)

One Apple M1 (Virtual), 3-core / 7 GB allocation; native macOS arm64,
Xcode 16.4 / AppleClang 17.0.0. Six ABBA/BAAB quartets and six interleaved
identical-binary A/A quartets per mode preserved PR74's original protocol.
96 trials / 1,843,200 callback observations; 96 kHz, 32/64 samples,
three engines, mono/stereo, three rigs / six microphones.

**Performance improvement NOT DEMONSTRATED.** Zero of 12 fixed-publication
routes passed the callback CPU improvement rule, and zero of 12 passed the
spectral MAC rule. All 12 profile routes had identical spectral-call schedules.
No control CPU/wall-p99 regression crossed the prespecified A/A envelope either.
The absence of a regression flag is not a non-inferiority or equivalence result.

Both A and S8T passed the original 5/5 CAB CTests, 66/66 deadline routes,
144 realtime-equivalence routes, 2,645,088 frozen float + 388,800 fade-mix
exact-bit comparisons, immutable spectra and zero added processing latency.

All-callback stage-clock-off wall misses were 174 → 162 at 32 samples and
46 → 72 at 64 samples, with 115,200 callbacks per arm at each buffer size.
CPU-over-budget counts were 154 → 153 and 42 → 68 respectively. These
include asynchronous worker exposure and do not establish a causal miss-rate
change. Four diagnostic p99 assertion failures were retained without rerunning
or deleting those populations. Original one-shot deadline passes do not erase them.

The measured complete callback scope is CAB processing (three rigs / six mics),
not the complete PluginProcessor, DAW or physical audio-device callback.
Full product/DAW/device/listening acceptance remains PENDING.

Native controller: `025bb927e8e4fc7177b476ccea00dc12a49b0b41`.
This evidence-only commit leaves the tested controller and product sources intact.
Patch/source identities, every timing route, A/A envelopes, confidence intervals,
command receipts, artifact digest and expiry are preserved in the accompanying
reports, manifests, provenance and audit results. The audit programs also preserve
the distinction between collection success, compatibility and performance approval.

PR73 and S8T product integration remain **HOLD**. Draft verification PR only;
no merge, public package or release. `release_approved=false`.
