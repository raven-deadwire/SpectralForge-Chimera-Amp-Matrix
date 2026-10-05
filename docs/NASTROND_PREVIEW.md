# Náströnd 1.2 Preview

The October 5 owner instruction authorizes the complete sequence through a
usable test installer. This branch integrates Náströnd for that preview. The
public 1.1.2-beta.1 release, update feed and homepage remain separate.

## Use the preview

Select **Náströnd** in the AMP menu, or use **Factory → Original / Náströnd**.
The defaults are GAIN7.2, BASS5, MID5.5, TREBLE5, MID FREQ850Hz, PRESENCE5,
DEPTH5, MASTER4.5, CLANK6, CRUSH6.5, IMPACT6.5, ROT4 and BLOOM3.5.
The ALL panel exposes the complete 13-control amp. Software input trim and
output level remain separate. All six Classic/Dual/Matrix contexts can select it.

| Preset | Gain | Intended role |
|---|---:|---|
| Fenrir | 7.2 | Tight low-tuned rhythm |
| Surtr | 7.8 | Dense sustained lead, delay/reverb |
| Níðhöggr | 7.6 | Asymmetric decaying grind |
| Fimbulvetr | 7.0 | Broad low-mid sustain |
| Ragnarök | 7.8 | Heavy transient impact |

These are five voices of one authored amp. They append at preset indices38–42.
The first31 Factory, three bass signatures and four guitar signatures keep
their identities and previously revised gain settings. Total selectable presets:43.
Ironball remains retired from selection; its saved-project identity is retained.
Modern Clean retains its product-facing alias without EICH in the preset name.

## Integration contract

- Append model24: 25 serialized amps, 24 active amps; 84 active AMP/PRE/POST models.
- Append90 parameters after Gate Range: all first3,891 positions preserved,
  production total3,981. New controls have version hint3.
- Reuse the existing native bank engine switch, model-change crossfade,
  oversampling and latency compensation. The Original core is not oversampled twice.
- Reuse the measured core unchanged from `NASTROND_VOICING.md`. No original NAM
  model weights are embedded or redistributed in the installer.
- Native dispatch parity, six-context state restore, five preset binary audio
  roundtrips, A/B gain preservation and pre-1.2 migration are covered by
  `ChimeraIntegratedProcessorTests`.
- Real editor tests exercise model selection, 13 controls, MID FREQ in Hz,
  compact/ALL views and 75% scale. Original names use explicit UTF-8 decoding.
- Catalog regeneration preserves the released high-gain defaults and ZUTA CH3;
  the generator can no longer silently restore the earlier weak defaults.

The standalone `OriginalAmpProcessor` and 84-parameter development panel remain
isolated test tools. The product uses the core via `AmpNativeDSP` and its existing
native state bank; the development enable parameters are not product parameters.

## Reference and listening status

The 26 NAM / five named-family comparison and subsequent voicing are documented
in `NASTROND_REFERENCE_COMPARISON.md` and `NASTROND_VOICING.md`. Product dispatch
is checked against that same core. Strong saturation at default gain is retained;
output normalization is separate from gain and character controls.

Meshuggah references include cabinets, Granophyre physical provenance remains
uploader-asserted, and four families lack absolute input calibration. The
synthetic fixtures check regressions and reference-informed behavior. They do
not establish a five-head hardware clone or substitute for actual instrument-DI
listening. Audition palm-mutes, chords, weak notes and sustained lead at equal
listening level, with the same external IR if comparing recordings.

Close DAWs and Standalone before installing. Windows Setup verification covers
custom destinations, installed Standalone/VST3, repair and uninstall. The preview
is unsigned; the delivered checksum identifies the verified bytes. The exact
commit and installer verification record travel with the test package.

## Measured preset levels

The five Original presets measure −26.24 to −25.89 dBFS RMS on the production
pluck fixture, with at least12.02 dB nominal peak headroom. Output trims are
+2.5/+4.0/+3.0/+3.5/+1.5 dB respectively; gain and character settings are unchanged.
All43 preset recalls and17 existing high-gain paths passed. Product-core dispatch
residual is0. See [local evidence](evidence/nastrond-preview-20261005/README.md);
exact-commit CI and installer checks accompany the downloadable candidate.

The subsequent [five-channel high-gain revision](NASTROND_CHANNELS.md) adds the
owner-requested channel banks and stronger midpoint; earlier tables above remain
frozen baseline evidence.
