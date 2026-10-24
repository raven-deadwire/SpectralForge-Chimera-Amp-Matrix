# Náströnd NAM toolchain: review and redesign

Reviewed 2026-10-24 against `24273e9`. Scope: the tools that acquire the
Náströnd NAM references, render them beside `OriginalAmpProcessor`, and
summarise the results (`fetch_nastrond_references.py`, `compare_original_nam.py`,
`audit_nastrond_macros.py`, `fit_original_voicing.py`, `ChimeraOriginalAmpRender`).
The reports remain observations only. Nothing here certifies hardware matching or
owner listening acceptance.

## Problems found

| # | Problem | Effect | Evidence |
|---|---|---|---|
| 1 | The spectral score `log_spectrum_rms_db` averages linear Welch bins. 1024 of its 1354 bins are between 2 and 8 kHz and 31 are below 250 Hz. | The score that every macro audit and voicing note relies on mostly measures treble. A 20 dB bass loss barely moves it, which matters for a low-tuned rhythm amp. The voicing fit had already switched to 42 log bands, so fit and audit weighted the spectrum differently. | `test_band_metric_and_polarity`: a −20 dB cut below 250 Hz scores more than twice as high with octave bands as with the v1 metric. |
| 2 | Signed correlation is reported without saying what the sign means. | VH4 CH3 shows `correlation −0.63` against the default. That is an inverted polarity at the best lag, not an unrelated waveform. | `nastrond-nam-20261005/comparison.json` |
| 3 | No tool generates the derived evidence. The three evidence folders hold two different `gain-summary.csv` layouts, and the channel `summary.json` has no generator. | Published summaries cannot be regenerated or checked. | `nastrond-nam-…` vs `nastrond-voicing-…` CSV headers |
| 4 | Protocol constants are typed again in each tool: `37*SR`, `21*SR`, `int(hz*k/4)`, the 26/17 counts, and the macro target model-id lists in both the audit and the fit. | One edit to the probe or manifest silently desynchronises the tools. The harmonic bins are only correct while the window is 250 ms. | `compare_original_nam.py`, `fit_original_voicing.py`, `audit_nastrond_macros.py` |
| 5 | The audit requires exactly 17 states. | It cannot read any report made with `--states` (the channel report has 39), so channel conclusions were assembled by hand. | `audit_nastrond_macros.py` |
| 6 | The manifest is never validated. Role, scope, gain group and eligibility exist in it but nothing reads them, and there are no duplicate or path checks. | The target groups can drift away from the manifest without any error. | `nastrond-nam-manifest.json` |
| 7 | Reports have no source provenance and no hash of the states file. Renderer info is free text. | A report cannot be traced to a checkout or an additional state set, and latency can't be read as a number. | `comparison.json` top level |
| 8 | Original states render one after another and are held in memory as float64. | 39 states plus 2 captures took 4 min 20 s. 39 × 37 s float64 is about 550 MB. | Measured with the real renderer and stand-in NAMs |
| 9 | The renderer exits with bare codes 2–8 and no message, and `preset` and `channel` together resolve silently, with the channel winning. | Failures surface as an empty `stderr`, and an ambiguous state is measured as something other than what was written. | `Tests/RenderOriginalAmp.cpp` |
| 10 | No test or CI job covers any of these Python tools. | The regressions above can't be caught. | `.github/workflows` |
| 11 | Extra state names can collide with work files (`comparison`, `probe`). | A state can overwrite the probe or the report config. | `compare_original_nam.py` |

## Redesign

```
Tools/nastrond_nam_tool.py        one CLI: fetch | compare | gain-csv | audit | groups
Tools/nastrond_nam/
  protocol.py    probe layout; every window, offset and harmonic bin derived here
  references.py  manifest validation; macro target groups derived from role/scope/gain
  analysis.py    level, THD, v1 spectral metric + polarity + octave-band metric
  renderers.py   content-addressed render cache (v1 file names and keys kept)
  states.py      frozen v1 suite; validated, non-colliding additional states
  compare.py     comparison.json (v1 keys unchanged, additions listed below)
  summary.py     gain CSV and macro-direction audit as pure functions of reports
  fetch.py       hash-verified acquisition (unchanged behaviour, validated manifest)
```

`compare_original_nam.py`, `audit_nastrond_macros.py` and
`fetch_nastrond_references.py` are now thin compatibility entry points, so the
commands quoted in the earlier Náströnd documents still work.
`fit_original_voicing.py` takes its groups, offsets and bands from the package.

### Report additions (`analysis_version: 2`)

- Every spectral comparison gains `polarity` (±1) and `band_spectrum_rms_db`,
  the RMS of the dB difference over 42 logarithmic bands from 65 to 8000 Hz.
  `log_spectrum_rms_db` keeps its v1 meaning.
- Top level gains `source` (tool commit and dirty flag), `states_sha256` and
  `analysis_version`. Each Original state gains `renderer_info`, for example
  `latency`, parsed from the renderer output.
- `protocol` is byte-identical to v1, so new reports remain audit-compatible
  with the frozen baseline.

### Audit

`nastrond_nam_tool.py audit` defaults to the v1 metric, the frozen states and
the v1 thresholds. With those defaults it reproduces
`nastrond-macro-direction-20261005/summary.json` key for key. It adds options for:

- `--metric band_spectrum_rms_db` for octave-weighted direction. Reports
  without this metric are refused, never approximated.
- `--scope channel_surtr` (or any other prefix) to audit a channel's own macro
  endpoints in reports made with `--states`.
- `--min-improvement-db` and `--max-default-drift-db`. These were hard-coded
  to a single pass before.

## Verification

| Check | Result |
|---|---|
| `gain-csv` on the frozen 2026-10-05 baseline | Byte-identical to `nastrond-nam-20261005/gain-summary.csv` |
| `audit` on the frozen baseline, voicing and macro-direction reports | Every frozen `summary.json` key identical, PASS |
| v1 and v2 `compare` on the same real `ChimeraOriginalAmpRender` (39 states with the channel suite) and stand-in NAM renderer | All v1 keys and values identical. 4 min 20 s → 1 min 23 s with 4 workers; 11 s when rerun from cache |
| v1 and v2 `fit_original_voicing.py` on the same 11 target renders | Byte-identical output |
| `Tests/test_original_renderer.py` | PASS, now also checks the diagnostics and the preset+channel rejection |
| `Tools/test_nastrond_nam.py` (added to the CI validation-contract job) | PASS |

The real 26 captures could not be fetched from this environment (the network
policy blocks api.tone3000.com), so no octave-band result exists yet for them.

## Next measurement

The full procedure, with commands, is in
[NASTROND_NAM_BAND_MEASUREMENT.md](NASTROND_NAM_BAND_MEASUREMENT.md).

1. Run `nastrond_nam_tool.py compare` on the current source with the private
   NAM directory and the channel state file.
2. Audit with `--metric band_spectrum_rms_db`, both for the default scope and
   for each `--scope channel_<name>`.
3. Compare the IMPACT and BLOOM conclusions with the v1 metric. Those are the
   macros whose effect lives mostly below 500 Hz, which the v1 metric
   underweights.

These steps still don't address the existing blockers: the Meshuggah captures
include a cabinet, Granophyre's provenance is unverified, absolute input
calibration is missing, and owner DI listening hasn't been done.
