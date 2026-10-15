# Independent original A versus PR75 S8T

Experimental controller only. Production `Source/`, original tests, thresholds, IRs,
latency geometry and publisher files are unchanged. No merge or release is authorized
by a successful collection job. `release_approved=false` remains unconditional.

## Fixed identity

- A: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`, original pre-NEON source.
- PR73 source: `29865abb4da3ab78592c970a263ee62af63fc673`.
- Exact S8T patch: PR75 `2df0b1e7c1a1e7022ed77a577bf0effb99a672fa`,
  `docs/evidence/pr73-neon-structure-20261015/snapshot-candidate/S8T.patch`.
- Patch SHA-256: `7aaef8ab7367c44083009121c6b2ac66d5b6378bc25b731b2b3720f5afbfcde0`.
- Resulting header SHA-256: `f95f02d204497aa8c1f18bcb7ad66954b2174780db2570d4a47bd7dba3dbeba4`.
- B: deterministic local experimental commit `20c2be7ab2bf4f72683dbf163055b8a7467901e3`,
  created only inside the isolated candidate checkout. This is not a product branch.

The controller verifies ancestry, both clean source trees, the one-header-only A/B
diff, original harness equality, pinned JUCE and identical normalized probe compiler
commands. Xcode 16.4 / AppleClang 17 and native macOS arm64 are required. The actual
hardware, runner allocation, toolchain, binaries and all command receipts are recorded.

## Preserved experiment

Reuses PR74's `Tools/CabNeonAB` CMake, probe generator, raw parser, estimator and
analysis unchanged, with SHA-256 guards. The execution body retains its exact schedule:
six AB quartets alternating ABBA/BAAB and six interleaved AA quartets for each
control/profile mode; AA uses the same A binary in both logical positions. All builds
finish before gates/measurement. No parallel benchmarks, reruns, outlier deletion,
priority changes or threshold relaxation. One runner allocation executes every arm.

96 kHz, 32/64 samples, mono/stereo, three engines, three rigs/six mics; each route keeps
1,200 worker/automation callbacks and 400 forced six-slot-publication callbacks.
This totals 1,843,200 measured callback records (96 trials × 12 routes × 1,600).

Primary population is the deterministic forced-publication phase. Report worker and
all-callback populations separately. Spectral MAC is instrumented wall stage time;
thread CPU and wall p99 measure the **complete CAB processing callback**, not a
standalone MAC kernel. This harness does not measure the entire PluginProcessor,
audio-device callback or DAW scheduling. Stage clocks are off in control and on in
profile; do not pool them or subtract them as an exact observer-overhead estimate.

Preserved performance rule: per-quartet log-ratio effects, 10,000 deterministic
bootstrap resamples of the six quartets, lower 95% bound above both 2% and the largest
absolute AA quartet effect, and both execution orders improving. Every one of the
12 primary routes must pass callback CPU and spectral MAC criteria, with identical
profile spectral-call schedules and no established control CPU/wall-p99 regression.
Miss counts and CPU-over-budget counts are descriptive; sparse misses cannot prove
a rate reduction. Existing p99 deadlines remain 333.333… / 666.666… µs at 32/64 samples.

Both arms rerun the original five CAB CTests, 66 deadline routes, 144 realtime
equivalence routes, 2,645,088 frozen floats plus 388,800 fade floats, immutable spectra
and zero added latency. Eligibility additionally requires **both** arms' original
gates; collection completion is not performance or product approval.

## Reproduction

Use a clean checkout of this controller commit, and fresh detached source checkouts.
Run from the directory containing `controller`, `parent`, and `candidate`:

```sh
export DEVELOPER_DIR=/Applications/Xcode_16.4.app/Contents/Developer
export PYTHONDONTWRITEBYTECODE=1
git clone https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix.git parent
git -C parent checkout --detach c2263e4c90bffb9a56cc41285ebf8207e641b3aa
git clone https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix.git candidate
git -C candidate checkout --detach 29865abb4da3ab78592c970a263ee62af63fc673
python controller/Tools/CabNeonAB/test_analysis.py
python controller/Tools/CabS8TAB/test_contracts.py
python controller/Tools/CabS8TAB/prepare.py --candidate candidate
python controller/Tools/CabS8TAB/run.py --parent parent --candidate candidate --build builds --output evidence
```

Existing output/build directories are rejected to preserve previous populations.
The Actions workflow performs this in one `macos-15` allocation and uploads failed
populations too. Audit the downloaded ZIP independently:

```sh
python controller/Tools/CabS8TAB/audit.py native-artifact.zip audit
```

Pending until new native evidence exists: A/S8T performance, native exact-bit/latency
revalidation, original deadline gates and artifact integrity. Full product validation,
real DAW/device/listening acceptance and release approval remain separate pending
requirements even if this CAB experiment demonstrates improvement.
