# Factory preset dynamics and EQ finishing

Request: 2026-10-19 KST. Scope: the 48 selectable factory presets, including
Deadwire, Raven Guitar, Náströnd and Niflheimr. The owner additionally requested
that preset updates be included in the distribution as soon as they are ready.

## Implementation

`Source/FactoryPresetFinishing.h` contains one explicit recipe per stable menu
ordinal. The four full-state snapshot builders apply it after their native
PRE/amp/CAB/POST voicing. Both normal recall and direct snapshot exports therefore
contain the same finishing parameters. User-state restoration does not call it.

| Stage | Authored treatment |
|---|---|
| Tone band 1 | Bell at the preset's body/low-mid frequency, 0 or −0.5 dB static gain and a bounded 1–2 dB dynamic cut |
| Tone band 2 | Broad definition Bell, +0.5 dB where required; otherwise disabled |
| Final band 1 | Gentle −0.5 dB high shelf where required; otherwise disabled |
| Final band 2 | Bell with zero static gain and a bounded 1–2 dB dynamic cut for harsh upper mids |
| Dynamic slots | Targets 1 and 14; slots 3 and 4 unassigned |
| Common COMP | Selective low-ratio parallel control, 0 dB makeup; disabled for all eleven existing POST-compressed presets and exposed envelope/fuzz roles |
| Gate/expander | Genre-specific mode, attack and finite range; retain any longer authored hold/release and gentler finite attenuation |
| Live limiter | −1 dBFS sample-peak ceiling, after the existing final output stage |

Dynamic processing uses Bell nodes only. The current engine detects a band-pass
region even for dynamic shelves; static shelves remain suitable for the small
finishing adjustment. No detector algorithm, filter implementation, host
parameter, parameter default, oversampling or latency policy changes here.

The four snapshot functions initialize the registered sound parameters before
authoring. That reset removes prior EQ bands and unassigned dynamic controls.
Factory `EQEditState::RestoreScope` rebuilds occupancy from the enabled nodes;
factory recall remains an undo/redo barrier. Saving a user sound captures its
actual EQ/dynamics values and occupancy for later binary and A/B restoration.

## Gate, Gate 2 and Expander (2026-10-25)

Recalls now use the PRE/POST gates and the separate Expander. Each preset pairs
its hard main gate with exactly one gentle stage:

| Roles | Main gate | Added stage |
|---|---|---|
| clean, edge, lead, finger, pick, slap, matrix, envelope, fuzz (38 presets) | Gate mode, closing only below the Expander's range: authored threshold − (Expander range ÷ (ratio − 1) + 3 dB), clamped to 9–24 dB lower | **Expander** on the main gate's side, at the authored threshold and ratio, range = min(authored range, 15 dB; 9 dB for envelope and fuzz), attack ≥ 1 ms, the authored hold and release |
| rhythm (10 presets) | Gate mode, as authored | **Gate 2** on the other side of the rig (PRE when the main gate is post), Expander mode 2:1, 6 dB below the main threshold, range ≤ 18 dB |

The previous single gate in Expander mode expanded at the authored threshold
all the way to its range. The Expander now does that part up to 9–15 dB, and
the main gate removes the rest only once that range is used up, so tails keep
the same shape near the threshold and silence is still fully closed. For
rhythm presets, Gate 2 keeps hiss out of the amp between phrases while the
post-rig main gate stays as tight as before.

`ChimeraIntegratedProcessorTests` checks every preset: the main gate is in Gate
mode with exactly one of Gate 2 / Expander, the Expander sits above the main
gate and Gate 2 below it, the added stage cuts less than 0.5 dB while the pluck
fixture plays, and more than 6 dB on hiss 24 dB under its threshold.

| Measurement (local Linux) | Result |
|---|---:|
| Largest Gate 2 / Expander reduction while playing | 0.00002 dB |
| Smallest reduction on the quiet hiss | 8.9 dB (Doom Fuzz, Expander range 9 dB) |
| Lowest nominal preset RMS / highest unprotected peaks | unchanged: −28.0467 dBFS, Slap Studio |

Dynamic EQ thresholds were re-checked with the same render and kept: the
largest mean dynamic cut is 41 % of its range (Low B Foundation, body node), no
node reaches 90 %, and the preset levels and limiter results are unchanged.

## Threshold calibration

Thresholds are authored for the actual Tone and Final stage levels, which differ
by amp, routing and POST processing. The initial uniform thresholds held several
high-gain presets near their maximum dynamic cut. The revision uses each Tone
detector's measured 90th-percentile envelope as a starting point for half of the
bounded cut, rounded to 0.5 dB. Slap Studio retains a conservative −26 dB body
threshold instead of following its weakly excited synthetic upper harmonics.

The eight-harmonic pluck fixture does not fully excite the clean/bass harshness
bands. Final thresholds were raised where the initial cut stayed near maximum;
quiet bands were not given artificially low thresholds to force meter activity.
The clean, edge, lead, finger, pick, slap and Matrix common-compressor thresholds
are −24, −21, −21, −22, −22, −24 and −20 dB respectively. These are stage detector
levels, with the existing low ratios and partial wet mixes; they are not input
gain targets. No per-session automatic calibration or detector DSP was added.

## Preserved musical contracts

- Global OUTPUT remains 0 dB for all 48 presets; existing amp/CAB/POST level
  compensation is retained.
- Bass and low-tuned guitar acquire no extra audio high-pass. Common COMP HP
  acts on its detector only. Matrix dry LOW, LOW amp blends, crossovers and
  Niflheimr's internal BLEND controls are preserved.
- Bass Envelope and Filter Lead retain their PRE detector order and use no
  new common broadband compression.
- Náströnd Thall BLOOM stays at least 0.75, Molten ROT stays at least 0.75,
  and Slam Impact retains LOW COMP 0.5 / LOW amp mix 0.75.
- Niflheimr Heavy Pulse retains LOW COMP 0.25 / LOW amp mix 0; Slam Punch
  retains LOW COMP 0.5 / LOW amp mix 0.25.
- POST-compressed ordinals 1, 8, 9, 11, 20, 31, 32, 33, 34, 38 and 42
  retain their authored compressor and common COMP stays disabled.
- Factory names, ordinals, signature IDs, user IR assets and saved user states
  retain their existing identity and restoration behavior.

## Validation and interpretation

The source baseline is main `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`, whose
product DSP matches public `v1.3.2-beta.1` source
`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`. These recipes are authored starting
points, not measured artist settings or a claim of hardware equivalence.

The product test renders all 48 presets with its existing deterministic pluck
fixture at nominal and +6 dB input. It records output RMS/peak and common
compressor, limiter and dynamic-node gain reduction. It repeats peak checks with
the new limiter bypassed, so the protection stage cannot conceal bad gain
staging. The existing peak and minimum-level gates are not relaxed.

Additional checks cover active EQ occupancy, unique enabled dynamic targets,
unused bands/slots after dirty recall, common/POST compressor overlap, low-B
static and maximum-cut EQ response, full-state binary recall and A/B, native EQ
UI refresh and undo barriers. Existing common-dynamics and EQ DSP tests remain
required. Technical results and a publication receipt must identify the tested
source; real-instrument listening and DAW acceptance are separate evidence.

### Local Linux results

The final strict `ChimeraIntegratedProcessorTests` completed successfully in
319.64 seconds. The deterministic pluck renders use 48 kHz and 128-sample
blocks. All 48 independent dirty/clean recalls, processing-state project/A/B
restores, occupied EQ masks and protected/unprotected level checks passed.
The unchanged Original, four Guitar Signature and five Niflheimr regression
sections also passed. The separate product serializer export passed for all
48 files and 5,026 parameters per preset, with measured maximum normalized
restore error 0 and matching EQ/factory identity metadata.

| Final measurement | Result |
|---|---:|
| Lowest nominal preset RMS | −28.0467 dBFS, Slap Studio; existing gate is greater than −30 dBFS |
| Highest nominal peak with the new limiter bypassed | −0.759567 dBFS, Amber Heavy |
| Highest +6 dB-input peak with the new limiter bypassed | −0.909814 dBFS, Amber Heavy |
| Largest limiter reduction | 0.240434 dB, nominal Amber Heavy |
| Largest common COMP branch reduction, nominal / +6 dB input | 2.22132 / 3.2464 dB, Pick Attack Matrix |
| Nominal dynamic nodes with mean reduction at least 90% of their authored range | 0; the initial authored candidate had 23 |
| Greatest added EQ loss at 27.5 / 30.8677 Hz, including maximum dynamic cut | 0.0332644 dB |

Common COMP meter values describe its compressed branch before the partial wet
mix. Dynamic means are averages of block-end meter observations, not continuous
sample or loudness statistics. The fundamental check evaluates the added EQ
response, not hardware/DI fidelity of the complete amp path.

Measurements and test provenance are retained under
[`evidence/preset-dynamics-eq-20261019`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/main/evidence/preset-dynamics-eq-20261019/).
The local build overlaid candidate files on the baseline checkout; recorded
input hashes identify those files. Its compiled baseline SHA is not a public
build identity. Publication still requires the subsequent complete CI matrix
and package/export artifacts for the actual committed product source.

A subsequent UI follow-up passed `ChimeraCommonDynamicsUITests` and
`ChimeraSignalPathUITests` locally. It verifies both directions of the compressor
switch after factory recall and compares all 48 presets' EQ/compressor/limiter
path display against their actual parameter state. It also retains the
one-band automation, A/B, project restore and editor-recreation checks. The
[follow-up evidence](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/main/evidence/preset-dynamics-eq-20261019/ui-verification-followup.md)
documents the superseded neutral-EQ UI assertion and the corrected fixtures.

## Distribution behavior

Factory recipes are compiled into the plugin and Standalone. Shipping this
source in an installer makes the revised recipes available when a factory
preset is selected. A repository change alone does not update an already
downloaded binary. Saved songs and user presets retain their values until the
user explicitly loads a revised factory preset.

The maintenance path verifies the existing seven source-specific CI workflows,
the five packaged installers/archives and all three platform exports before
publication. It publishes a separate immutable `presets-2026.10.19.1` release,
with the revised presets already embedded in the plugin and Standalone. An
optional pack of 48 `.chimera` files is generated from those same factory states
using the product serializer and can be loaded with OPEN. A policy-only
publication receipt identifies the verified product source; changing the table
alone is not a completed publication. Existing release files, update feeds,
product version and beta expiry are retained.

The preset update does not certify or complete the separate Transpose,
FabFilter research, AAX or beta-lifecycle work tracked in issue #97. A release
must identify the actual preset scope and source without attributing those
unrelated candidate features to it.
