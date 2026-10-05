# Development checkpoint evidence — 2026-10-05 KST

This is development evidence for the 1.2 branch, not installer or release
acceptance. Public 1.1.2 source and release feed remain unchanged.

- `screenshot-provenance.json`: twelve owner-supplied settings screenshots,
  mapped to the four named presets by the owner. Hashes establish exactly which
  screenshots were read; no uploaded images or private IR assets are bundled.
- `original-core-tests.log`: independent Náströnd core control, memory, gain,
  stereo and C++ allocation checks.
- `original-integration-tests.log`: real JUCE development panel, six-context
  state, 84 development parameters and twelve rate/oversampling routes. The
  panel was also rendered and visually checked locally.
- `original-cmake-ctest.log`: fresh CMake-built OriginalAmp targets, both passing.
- `integrated-final.log`: actual production factory recall, all 38 full-chain
  preset levels, +6 dB input stress, signature state/audio roundtrips, performance
  control preservation and gain-reference regressions.
- `preset-review.csv`: all 38 presets, musical role, review action, baseline and
  candidate levels. Unchanged sounds are retained after the full-chain audit.
- `gain-review.json`: isolated PRE/native AMP synthetic saturation measurements
  against the supplied settings; tone and full-DI equivalence are not implied.
- `source-hashes.json`: source and fixture identities for this checkpoint.

The local integrated build recompiles `PluginProcessor.cpp`, `PluginEditor.cpp`
with the candidate source and links the previously built JUCE/framework objects.
Its generated version metadata and embedded manual are not release artifacts.
CI must build complete current-source binaries for each platform.

Reproduce from a normal JUCE development environment:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ChimeraIntegratedProcessorTests ChimeraOriginalAmpTests ChimeraOriginalAmpIntegrationTests
ctest --test-dir build -C Release -R 'Chimera(IntegratedProcessor|OriginalAmp)' --output-on-failure
```

The normal integrated test calls `loadFactoryPreset` and tests actual recall.
`--measure-gain` and `--measure-presets` are diagnostic iteration modes that can
apply the current header-defined snapshots with a cached DSP library. Final
acceptance uses the normal test entry point, not either diagnostic mode.

PRE/AMP probes use 48 kHz, 4x, 150/300/600 Hz, −54 and −24 dBFS peak sine inputs,
1 second settling and a 100 ms coherent measurement. Harmonics 2–16 are divided
by the fundamental and weak/strong ratios are combined geometrically. The
required ratio is at least 90% of the reference, and output growth is at most
reference +1.5 dB. Envelope sweeps are bypassed only for this isolated probe;
full-chain measurements retain the real filter, cabinet, POST and effects.

Missing evidence remains explicit: instrument-DI listening comparison, complete
Náströnd production routing/UI/state integration, exact hardware matching, and
new-version real-host/installer/signing validation are not established here.
