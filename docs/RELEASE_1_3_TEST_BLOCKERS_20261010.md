# 1.3 candidate test blockers — October 10, 2026

Checkpoint: 2026-10-10 01:16 KST. PR #35 head
e05ca7ee9398a063615e6795f8754caa8406bfd6. Read later runs before presenting this
as current status. This record diagnoses failed gates; it does not change
thresholds, DSP or release status.

## Installer status

Compilation succeeded. Product CTest passed 38/39 on Windows and 35/36 on
macOS/Linux. The Windows candidate passed 36/39. Setup generation,
installation verification and delivery packaging were skipped. Existing
artifacts are evidence/previews, not the completed installer.

- [Product run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620)
- [Windows candidate and job log](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956736/job/113892726803)
- [Product Windows job log](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620/job/113892920066)
- [Product macOS job log](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620/job/113892920047)
- [Product Linux job log](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620/job/113892920141)

## Deterministic channel-balance failure

ChimeraIntegratedProcessorTests fails in originalChannelLevelProbe with:
K-weighted channel spread exceeds the bare/driven cabinet target.

| Measurement | Observed spread | Existing exclusive limit | Result |
|---|---:|---:|---|
| Bare CAB path | Approximately 2.33724 dB | Less than 2.5 dB | Pass |
| Driven CAB path | 1.8139 dB | Less than 1.6 dB | Fail by 0.2139 dB |

The values agree across the three OS runs. Fenrir is the loudest driven path
and Ragnarök the quietest. This is not a timeout or UI-lifecycle abort.

The probe loads originalPresetStart, preset 38 Thall Rhythm, as its fixture.
That production preset changed from bundled V30/SM57 to the modeled Ruin 4×12
with Chimera Strike Mic A, position 0.36, distance 6 cm and mic gain −3 dB.
The probe function and both OriginalAmpDefinition/OriginalAmpDSP files are
unchanged relative to 6782e4b6ead29bf4a31243b2e05a535c38e16745.

The fixture response changed, and processor startup-state changes also exist.
This narrows the investigation; it does not prove that all differences are
caused by the CAB, or justify deleting the balance gate. Compare a fixed
bundled CAB and the new modeled CAB in the same current processor with
identical PRE/POST, stimulus, initialization and five channel settings.

The existing --original-channel-levels CLI calls the probe with
enforceBalance=false. Its exit code zero is **not** proof that the strict
balance gate passes. Reproduce with the unchanged full
ChimeraIntegratedProcessorTests CTest target. A shorter strict diagnostic
entry point can be added separately if needed, preserving all thresholds.

## Candidate-only CAB timing failures

ChimeraOriginalCabIntegrationTests and ChimeraCabExpansionIntegrationTests
fail the existing CAB-only p99 versus block-period gate. At 96 kHz / 64
samples, the period is 666.667 microseconds.

| Candidate path | p99 |
|---|---:|
| Original, mono | 954.5 microseconds |
| Original, stereo | 1,172.8 microseconds |
| Expanded, stereo | 1,147.7 microseconds |

These are the three-CAB/six-microphone integration cases. The observed
callback C++ new/delete counts were zero; this counter is not a universal
malloc or lock tracer. Finite/kernel/transition checks passed.

The same tests passed in Product Windows at the same source and in the
earlier CAB-specific run. Runner contention or variance is a possibility,
not an established cause. Repeat the same protocol with a paired controlled
observation before changing DSP or any acceptance limit.

## Next work

1. Reproduce the strict channel gate and compare the fixed old CAB with the
   new production modeled path. Preserve the existing limits.
2. If the new rig needs calibration, use explicit visible rig/CAB settings
   and verify bare and driven balance plus factory levels. Do not alter a
   shared amp solely to hide a changed fixture.
3. Reproduce CAB timing with the same source/protocol and document runner
   conditions, callback p99 and deadline misses.
4. Rebuild the installer only after the actual candidate gates pass, then
   validate and deliver its real Setup file.

CAB-only workflows do not run the integrated Náströnd channel-balance test.
Niflheimr-only tests cover a different amp. Their passing results do not
supersede these failures. See the
[full roadmap](DEVELOPMENT_ROADMAP_2026_2027.md) for independent work that can
continue while these release items are resolved.
