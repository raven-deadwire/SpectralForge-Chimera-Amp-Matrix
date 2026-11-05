# Auto Set — phase 13 (Dual blend and crossover)

Phase 13 lets Auto Set set how the two rigs of the **Dual** routing combine:

- **Dual BLEND:** how much of rig 2 is heard against rig 1.
- **Dual CROSSOVER:** where the low rig hands over to the high rig.

The other Auto Set steps that touch the amp and the cabinet (phases 8–12) set
the Classic routing only. In Dual routing, this step is the one that shapes
the rigs. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE12.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual blend and crossover**
step, on by default.

- **When it runs:** only when the routing is Dual as Auto Set starts. It adds
  one pass of about 6 seconds, after the cabinet mic pass and before the Tone
  EQ pass. With Classic or Matrix routing there is no pass, and the row says
  "kept (Classic or Matrix routing: no Dual blend)".
- **The results:** a **Dual blend** row shows BLEND (as "% rig 2") or
  CROSSOVER (in Hz), before and after. It also shows how far the combined
  sound sits from the Tone EQ target, before and after, and the level change.
  The Final EQ and output pass that follows restores the loudness. The row's
  SHOW button opens the routing split.
- **Strength:** scales the change: before + strength × change.
- **Lock → Dual blend:** keeps both settings and skips the pass.
- **UNDO AUTO:** restores them.
- **What it leaves alone:** the Dual type (Blend or Crossover). Auto Set never
  switches between them.

## Measurement

The pass hears each rig on its own through two new sample taps:

- **What the taps carry (`Tap::lane1` and `Tap::lane2`):** each rig's output
  after its amp, cab, level, polarity and mute, before the Dual blend weight.
- **How they are filled:** the routing engine fills them only while Auto Set
  listens in Dual routing, and only for blocks that actually played Dual.
  During a routing change the old routing keeps playing for a few blocks.
- **Ring sharing:** `lane2` uses the amp tap's ring, so the two are never
  listened to together.

If the routing or the Dual type changes while the pass listens, nothing is
changed.

## Rules (`analyseDual`, `AutoSetDual.h`)

Both modes compare the combined sound with the Tone EQ target. They use the
same fit as phases 8 and 9: one-octave smoothing, 80 Hz – 6.3 kHz. A setting
changes only when the fit improves by more than 0.5 dB.

### Blend

- **Mix:** (1 − blend) × rig 1 + blend × rig 2, computed sample by sample from
  the two captures. The prediction is the mix itself, so cancellation between
  the rigs is included exactly.
- **Search:** blends in 5% steps, up to 30% either way. Values snap to 1%.
- **Cancellation:** the mix's level is compared with the rigs' power sum. If
  the mix is more than 3 dB below it, the row adds "the rigs partly cancel":
  check polarity or delay. A blend change cannot fix a polarity or delay
  problem.

### Crossover

- **Why it is a prediction:** a split at a new frequency changes what each rig
  receives, so the result cannot be computed from the captures. It is
  predicted instead.
- **How it is predicted:**
  - Each rig's band powers are scaled by how much more or less of its input a
    fourth-order Linkwitz-Riley split at the new frequency passes. The scale
    is the low-pass for rig 1 and the high-pass for rig 2, eight points per
    band, limited to ±12 dB.
  - The two rigs then add in power.
- **Search:** nine log-spaced candidates from half to twice the current split,
  60 Hz – 4 kHz, on a 5 Hz grid.

### Both modes

- **Silent rig:** a rig that is not heard (muted or soloed away)
  keeps the setting, and the row names the rig.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual`):

- **Rules (`dualRules`), 44.1, 48 and 96 kHz:**
  - A dark rig 1 raises the blend toward rig 2, and a bright rig 2 lowers it.
  - The predicted error equals the measured error of the mix at the new blend.
  - Two on-target rigs keep the blend.
  - A silent rig 2 keeps the blend, and silence reports no playing.
  - A quieter inverted copy of rig 1 reports cancellation. Independent rigs do
    not.
  - The crossover prediction for splits at 250 and 700 Hz, from rigs heard at
    400 Hz, is within 1.5 dB per band of the re-rendered split (independent
    sources).
  - A low rig 8 dB louder than the high rig lowers an 800 Hz split, within an
    octave and on the 5 Hz grid.
- **Capture (`dualLaneCapture`):**
  - The engine's captured rigs, after level and polarity, sum to its output
    with the blend weights.
  - The processor feeds the taps in Dual routing only.
- **Controller (`controllerDual`), real processor, fixtures fed to the lane
  taps:**
  - The pass runs alone, and the applied blend matches the analysis.
  - 50% strength moves the blend half-way.
  - The lock keeps the blend and skips the pass.
  - Classic routing has no pass and says why.
  - A silent rig 2 keeps the blend.
  - A loud low rig lowers the crossover and leaves the blend alone.
  - UNDO AUTO restores the settings.
- **Full run (`controllerIntegration`, Classic routing):** fourteen result
  rows, with the Dual row (kept, Classic routing) after the cabinet row.
- **Locks (`controllerEveryLock`):** all sixteen locks at four strengths.

## Not yet validated

The 0.5 dB threshold, the 30% blend limit and the one-octave crossover limit
are starting values. They have been checked on synthetic noise and filtered
rigs, not on real Dual rigs or listening tests.

The crossover prediction is linear. It ignores what the amps' distortion does
with a new split, and it adds the rigs in power. Real rigs with different amps
and cabs are mostly incoherent, so a power sum suits them. A rig pair that
shares one clean amp sums coherently, and the prediction can be off by up to
3 dB near the split. The blend is exact on the captured audio. The level
change it causes is left to the Final EQ and output pass.
