# Central horn grille: native rendering regression

## Reproduction and interpretation

The macOS CAB job in run `38035118608`, source
`d828230f8fd2f37eb851c263da339ae44ec35aa0`, failed
`ChimeraCabDriverVisualTests` with `native=1 scale=200 kind=0 wires=27 holes=0`.
The remaining 16 CAB tests passed. Artifact `11663407553` preserves the log and
scene screenshots. Its `cab-visual-horn-behind-grille-detail.png` was written
by the **software, 700 pixels/metre** case before the native failure: it must
not be presented as the failed native image.

`scale` in that test was camera density in pixels/metre, **not a percentage**.
The original matrix had two image backends, three camera densities
(200/400/700 px/m), and four HF kinds, with no independent device-scale sweep.

The assertion counts the separately rendered foreground mask, not dark
pixels in the horn photograph. A wire has alpha > 100 and an aperture has
alpha < 8; both populations must exceed 8 pixels in the central horn ROI.
Every RGB channel of the final image must match rear-overlaid-with-mask
within 3 levels. Thus zero holes is a loss of near-transparent raster
apertures, not confusion between a dark horn throat and a grille opening.
It does not mean that the original texture file lost its apertures, or that
the whole foreground became fully opaque.

## Production correction already in PR #39

`d4b8347e3f3009005739a68e71962eb6431a493f` scopes nearest-neighbour
(`lowResamplingQuality`) sampling to the authored foreground grille mask.
Interpolated minification mixed adjacent wire coverage into the aperture
pixels. The scoped graphics state restores the existing quality for other
artwork. No alpha threshold, wire count, aperture count or blend tolerance
was changed. No test was excluded.

The existing independent ARGB copies of repeating material crops remain in
place; their parent-atlas ownership regression is a separate issue. The
original horn mouth, plate, bolts, continuous foreground mask and shared
`frontHardware` order (horn first, grille second) remain intact. There are no
new paint-time image allocations, DSP changes or parameter changes in that
production correction.

At that source, CAB run `38050250366` passed 17/17 on each of Windows, Linux
and macOS. This covers the original 24 horn cases, not the expanded matrix
below. Product run `38050250439` still failed two Windows CAB timing tests
(`OriginalCabIntegration` and `CabExpansionIntegration`, p99 buffer-period
gate); its horn visual test passed. Those failures are not erased by the
separate CAB job's success.

## Expanded regression evidence

`hornGrilleContracts` retains all original cases and adds independent
graphics transforms at 100/125/150/175/200%: **120 cases per platform**.
Pixel ROIs are transformed into device coordinates. The same thresholds and
maximum per-channel error are applied at every scale; there is no platform
exception or retry. The test also requires all 120 cases to have executed.

Before each assertion, the test writes actual rear/horn, foreground RGBA mask
and final composite crops as `cab-visual-horn-{backend}-ppm{density}-dpi{percent}
-kind{kind}-{layer}.png`. Alpha counts/range and maximum blend error are
written to `LastTest.log`, including successful cases. These files are already
covered by the CAB workflow's always-upload screenshot glob.

A diagnostic `medium-mask-witness` is also saved for kind 0 at 100% for each
backend and camera density. It renders the same authored mask using the
pre-fix medium interpolation, with the same tile phase, density and central
ROI. The outer clip and frame lie outside this ROI. Its wire/aperture counts
show the backend's filtering effect directly. It is diagnostic only and is
never used as the acceptance reference. Compare its transparent pixels with
the production `mask`, and compare `rear` with `front` to inspect horn depth
and foreground layering. Crops preserve the original sampled pixels; they
are not generated illustrations.

## Release integration and acceptance

The fix and regression coverage belong to PR #39 against
`release/1.3.0-preparation` (PR #35). Check the final source SHA in all three
native CAB jobs; an earlier source's pass does not certify later code.
Native/software offscreen device transforms are automated renderer coverage,
not a claim of physical multi-monitor DPI or commercial DAW validation.

Passing this visual matrix does not approve the installer, callback timing,
instrument DI listening or DAW lifecycle. Preserve exact-source Product and
Candidate results, installation/repair/removal/security receipts, and the
existing release-policy acceptance checks separately. The publication
authorization is already recorded; `release_approved` remains false until
the outstanding evidence is actually satisfied.
