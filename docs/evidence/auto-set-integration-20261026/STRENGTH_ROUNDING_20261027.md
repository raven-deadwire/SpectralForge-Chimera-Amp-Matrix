# Input strength rounding follow-up (2026-10-27)

## Failure at the previously tested head

At `bbcb65046cd755b305e18a63f2b09d2f53da826a`, macOS Graphical EQ CI
failed `ChimeraAutoSetTests`: `Half strength did not halve the input gain change`.
The other 11 tests in that job passed. Linux and Windows passed at that SHA;
their results do not certify this follow-up.

- [macOS failure](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40292040229/job/121637145578)
- [preserved CI evidence](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40292040229/artifacts/12700220821)

The input parameter has a 0.01 dB interval. Normalisation can return a small
error around a legal value (the macOS log also shows the default as `-0.0`).
A small negative error around zero changes rounding of a 50% move toward
10.5 dB from 5.5 to 5.0 dB. The test oracle previously interpolated values
converted through normalised snapshots, whereas the controller used the raw
APVTS value; those round trips need not have identical errors.

## Fix and reproduction

`scaledInputGainDb` first recovers both endpoints in integer hundredths of a
dB, interpolates in double precision, then rounds to half-dB steps with ties
away from zero. Real 0.01 dB settings are preserved as interpolation inputs;
100% still returns the original analysis proposal. Gate continues to use the
actual applied gain. Locks, edit ownership, attack protection and Undo are
unchanged.

Added `inputStrengthRounding` to the native Auto Set suite. It checks 12
explicit expected cases with each of -0.000003, 0 and +0.000003 dB conversion
error, plus full-strength preservation. Cases cover 25/50/75%, positive and
negative ties, legal adjacent 0.01 dB settings and range endpoints.
Existing controller/strength matrix expectations also recover the parameter
interval before evaluating their expected half-dB result.

The exact new regression function was extracted into a dependency-free C++20
harness and compiled with GCC `-O3`. Using the previous controller expression
in the helper: **FAIL**, exit 1, `Input strength rounding changed at a parameter
conversion boundary`. With the new helper: **PASS**, exit 0 (37 checks).
`git diff --check` passed.

The full JUCE native suite was not rebuilt locally in this run: the restored
workspace lacks its build dependencies and the configured package mirror was
unavailable. Full Linux/Windows/macOS CI on the new head is still mandatory.
The earlier 3/3 local and platform CI passes must not be inherited.

## Integration boundaries

PR #156 remains the prerequisite and must merge to main first. Retarget #182
to main afterwards, recheck the final diff and CI, and merge only on success.
The prior macOS CAB timing failure passed on its rerun at `bbcb650`; this does
not prove that all timing variability is resolved.
Real guitar/bass DI listening, actual DAW Read/Touch/Latch/Write acceptance and
release approval remain pending. Notification tracking is not atomic
arbitration of arbitrary simultaneous host writes.
