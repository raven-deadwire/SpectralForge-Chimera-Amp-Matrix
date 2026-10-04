# AMP / PRE / POST and factory-bank audit — 1.1.1

This correction follows the user's 2026-10-04 request to inspect every AMP,
PRE and POST control, strengthen lead/high-gain distortion, revoice all factory
presets and make navigation follow category order. It supersedes the previous
release candidate. Publication remains BLOCKED on required manual acceptance.

## What the knob numbers mean

A serialized `0..1` position is not necessarily a bipolar gain. `0.5` means the
middle of travel. The DSP determines whether that position is unity, a tone
setting or a timing value. Volume pots can attenuate to silence; active EQ can
cut and boost; passive guitar tone stacks have insertion loss and interacting
controls. Converting every control into a boost would be incorrect.

Native AMP and normalized PRE knobs now display a `0..10` position rather than
an unexplained fraction. POST 1176 timing displays `1..7`; Pultec continuous
positions display `0..10`. Frequency/dB controls retain their units. Display
conversion does not change parameter IDs, ordinals, defaults or normalized
host mappings. These position scales are not measured hardware-pot tapers.

## Concrete corrections

| Area | Finding | Change / evidence |
|---|---|---|
| Lead / high gain | Common pre-gain mapping provided -18..+18 dB, including attenuation over the lower half | Selected lead/high-gain channels now use a closed-at-zero audio taper with amplified midpoint/full settings and stronger interstage drive; output trim remains separate. Eleven weak-input THD fixtures check increased saturation and remaining control travel. |
| Bass signature / guitar rhythm | Tight 515 recipes selected Rhythm despite lead/high-gain intent | Crom Cruach, Wild Hunt and the second Feel My Wrath lane now explicitly select Lead. |
| Factory recall | 31 general presets used legacy DSP while the visible knobs edited independent native banks | Full sound snapshot, explicit native AMP/PRE/POST voicings, all latent banks reset; no first-edit switch to unrelated defaults. |
| Preset levels | Blanket -9/-10/-11 dB output trims lacked per-preset level matching | All 38 presets have individually measured end-of-chain trims. Nominal six-note plucks and +6 dB input checks preserve peak headroom; values are in `Source/FactoryPresetLevels.h`. |
| Wild Hunt | ISA LINE gain position 0 caused 20 dB attenuation | Select LINE position 20 for unity through this model, then set the measured final output trim. |
| Factory Variable Mu | Converted input setting added an unintended 3 dB attenuation | Factory input starts at the model's unity position; compression and output remain separately authored. |
| BDDI v2 | Blend=0 bypassed the active EQ | Blend now mixes the driven/presence path before active Bass/Mid/Treble EQ and Level. Regression proves dry-blend EQ remains active. |
| Cali76 FET 2024 | Timing direction/ranges differed; OUT also scaled DRY | Attack 4.8→0.2 ms, Release 398→69.5 ms clockwise; independent DRY mix after wet OUT, up to +9 dB. Native timing avoids the generic FET character's additional attack scaling. |
| M87 | Release was 40–800 ms | Release now spans 50–1100 ms. Attack remains 20–800 μs; manual wording/dial-direction ambiguity is not declared resolved. |
| SVT-VR channel 1 | Bass/mid/treble specification mismatches | Bass 40 Hz ±12 dB; mid 220/800/3000 Hz ±20 dB; treble 4 kHz ±12 dB. Small-signal mid span measures 40 dB. Ultra voicing remains approximate. |
| B7K + DB751 | B7K bass and DB751 tone values mismatched published values | B7K bass 100 Hz; DB751 bass 40 Hz, mid 750 Hz, treble 4 kHz +12/-7 dB. Filter shapes are authored approximations. |
| Subway D-800+ | Mid ranges ±15 dB and repeatedly applied variable coupling HPs | Mids ±12 dB; fixed 22 Hz two-pole plus variable two-pole HP, combined cutoff 30–150 Hz. Butterworth approximation, not a captured response. Factory clean/bass HP starts at minimum. |
| EICH T900 | Existing ranges were already correct | Preserve 30 Hz ±15, 250/800 Hz ±12, 8 kHz ±15 dB. No fictitious EICH range fix. |
| Preset browsing | Menu groups and previous/next ordinal sequence disagreed | One category order drives both menu and navigation; all 38 saved IDs remain append-only. |

Gain correction covers Tight 515 Lead, Wide Rect Vintage/Modern (Raw retains a
lower range), Liquid Lead, Orange Dirty, Silk ODS, Cinder CH3/CH4, Fourfold
CH3/CH4, Night Harvest EP/KK, Hot Lead OD, Blue Sky Lead and E670FE Lead I/II.
The legacy Ironball DSP and the released host-parameter contract remain intact.
Existing projects retain saved parameters; native-model tone can change because
the corrected DSP is intentional. No automatic preset replacement occurs.

## Preset-specific tone intent

The follow-up request also rejects indiscriminate noon settings. The 31 general
factory recipes now have explicit per-preset bass/mid/treble/presence/resonance
profiles in `FactoryNativeVoicing.h`. Examples: Bell Clean has lean bass and an
open top, Classic Crunch emphasizes mids, Tight/Melodic Death rhythm keeps bass
controlled, Dumble/Melodic Lead has stronger mids and a softer top, Slap Studio
has a modest scoop, and Low B Foundation restrains treble. PRE compression has
explicit timing/ratio/tilt choices and Subway frequencies are selected by role.

Crom Cruach's formerly noon-heavy active AMP/PRE/EQ banks are revoiced. Wild Hunt
and Azhi Dahaka retain their already differentiated tone/crossover settings with
the signal-path fixes above. The four Guitar signatures now explicitly voice
both active amp lanes: Alsatia for singing midrange, Wrath for groove weight,
Blackhearted for tighter death-metal attack and Throne for a darker dense tone.

Unity masters and output stages, deliberately flat correction bands, bypassed
effects and inactive channel banks can correctly remain neutral. Knobs are not
moved just to make a preset look edited. All levels are remeasured after these
tone changes; actual-DI listening remains required.

## Automated coverage and its limits

| Scope | Checked |
|---|---|
| AMP | 24 serialized / 23 active models, 380 control endpoint responses, 448 inactive-channel isolation cases; 44.1/48/96 kHz routes, all oversampling paths and callback allocation checks |
| PRE | 39 models, 154 connected control responses; 7 explicitly unavailable controls remain unavailable, not counted as passing audio controls |
| POST rack | 9 models, 64 audio control responses, 3 meter controls; bypass/reset/stereo/block partition tests |
| POST spatial | 6 modulation, 3 delay and 3 reverb models; all 36 exposed controls |
| Factory bank | All 38 recalls from dirty and clean state, explicit native engine selection, category permutation/navigation, nominal and +6 dB input plucks |
| Compatibility | Released IDs/ranges/defaults, legacy Ironball, state restoration, synthetic pitch, Gate and gain/GR regression |

The source inventories and measured output are in `Validation/ControlAudit`.
Local evidence contains source-file hashes because a test from an uncommitted
working tree must not masquerade as the parent commit's result. Final-source
Windows/macOS/Linux CI must run after the correction is committed.

**An audible endpoint change is not proof of hardware authenticity.** Passive
guitar tone stacks still use independent authored filters, so their loading,
control interaction and insertion loss do not reproduce the original circuits.
POST preamp calibration, original potentiometer laws, nonlinear drive spectra,
spatial hardware equivalence and multichannel reference acceptance remain
unverified. The connected-control audit does not turn these into PASS.

## Primary specifications used for corrections

- [Tech 21 BDDI v2 manual](https://www.tech21nyc.com/wp-content/uploads/2023/11/BSDR_v2_OM4.pdf)
- [Origin Effects Cali76 FET 2024 manual](https://origineffects.com/wp-content/uploads/2024/11/Origin-Effects-Cali76-FET-Compressor-Owners-Manual-V2-2.pdf)
- [Dunlop M87 manual](https://www.jimdunlop.com/content/manuals/M87.pdf)
- [Dunlop M82 manual](https://www.jimdunlop.com/content/manuals/M82.pdf) — DECAY affects stop frequency; it is not a conventional release-time knob.
- [Ampeg SVT-VR manual](https://ampeg.com/data/6/0a020a3f12fd55e0b8fc78d976/application/pdf/Owner%E2%80%99s%20Manual%20-%20English%20.pdf)
- [Darkglass B7K Ultra manual](https://www.darkglass.com/pages/microtubes-b7k-ultra-manual)
- [Aguilar DB751 specifications](https://aguilaramp.com/en-int/products/db751)
- [EICH T300/T500/T900 manual](https://www.eich-amps.com/media/61/manual-t300-t500-t900.pdf)
- [Mesa Subway D-800+ manual](https://mesa-boogie.imgix.net/media/User%20Manuals/070533-Subway_D800_Plus_171106-download.pdf)
- [Universal Audio 1176LN manual](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf)
- [Pulse Techniques EQP-1A specifications](https://pulsetechniques.com/products/tube-equalizers/eqp-1a/)

Other existing reference-panel review statuses and source links are retained in
`reference-inventory.csv`; they are not upgraded to measured response matching.

## Release blockers

Policy v4 `beta_1_1_1` defers only I2.PITCH_LIVE and I2.PITCH_DI. Actual commercial
DAW close/remove/reopen and soak, callback timing acceptance, real-DI/level/Gate
listening, and E670FE reference captures/acceptance are still missing. Synthetic
pluck/THD tests cannot replace those gates. The user's 15:11 KST conditional
publication authorization is fulfilled as authorization, not as test evidence.
