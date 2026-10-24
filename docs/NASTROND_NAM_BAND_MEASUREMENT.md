# Measuring Náströnd against the real NAM captures with the band metric

This is the procedure for the next measurement in
[NASTROND_NAM_TOOLCHAIN.md](NASTROND_NAM_TOOLCHAIN.md). It renders the 26
manifest captures and the OriginalAmp states, then audits macro direction with
the octave-weighted `band_spectrum_rms_db` metric and with the v1 metric, so
the two can be compared.

The results are observations only. They do not certify a hardware match or
owner listening acceptance.

## What stays private

NAM weights, rendered audio and the render cache go in a private directory
outside the repository. The resulting `comparison.json` and `audit-*.json`
files hold no weights or audio, so they can be committed as evidence. The
paths below are examples.

## 1. Prerequisites

```sh
pip install numpy scipy

# The NAM renderer, at the commit recorded in the manifest (nam_core_commit)
git clone https://github.com/sdatkinson/NeuralAmpModelerCore.git ~/nam-core
cd ~/nam-core
git checkout 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target render
# -> ~/nam-core/build/tools/render

# The OriginalAmp renderer, from the source under test
cd ~/SpectralForge-Chimera-Amp-Matrix
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ChimeraOriginalAmpRender
ctest --test-dir build -R ChimeraOriginalRendererTests --output-on-failure
```

## 2. Acquire and render

```sh
python Tools/nastrond_nam_tool.py fetch --out ~/private/nastrond-nam

python Tools/nastrond_nam_tool.py compare \
  --models ~/private/nastrond-nam \
  --nam-render ~/nam-core/build/tools/render \
  --original-render build/ChimeraOriginalAmpRender_artefacts/Release/ChimeraOriginalAmpRender \
  --states docs/evidence/nastrond-channels-20261005/channel-states.json \
  --out ~/private/m-main --workers 4
```

- `fetch` only saves a file whose SHA256 matches the manifest. It reports
  files that are already present as `VERIFIED`.
- `--states` adds the 22 channel states to the frozen 17, for 39 in total.
- Renders are cached in the `--out` directory. A rerun only re-renders what
  changed: the renderer binary, the probe, a state or a model.
- Check the report: `analysis_version` is 2, `source.tool_commit` is the
  commit you built, and `source.tool_tree_dirty` is `false`.

## 3. Audit macro direction with the band metric

Only reports made with `analysis_version` 2 or later have the band metric.
To judge the current DSP alone, pass the same report as baseline, incoming
and candidate. The audit then asks one question: does each macro move from 0
to 1 towards its target captures?

```sh
R=~/private/m-main/comparison.json

# Frozen v1 states: CRUSH, IMPACT, ROT, BLOOM
python Tools/nastrond_nam_tool.py audit --metric band_spectrum_rms_db \
  --baseline $R --incoming $R --candidate $R \
  --out ~/private/m-main/audit-band.json

# Each channel's own macro endpoints
for c in surtr ragnarok nidhoggr fimbulvetr; do
  python Tools/nastrond_nam_tool.py audit --metric band_spectrum_rms_db --scope channel_$c \
    --baseline $R --incoming $R --candidate $R \
    --out ~/private/m-main/audit-band-$c.json
done
```

| Scope | Macro | Target captures |
|---|---|---|
| `channel_surtr` | CRUSH | VH4 569473, 569474, 569477 |
| `channel_ragnarok` | IMPACT | Twin Jet 652436, 652437 |
| `channel_nidhoggr` | ROT | Granophyre 403977, 421075, 421074 |
| `channel_fimbulvetr` | BLOOM | Matamp 614727, 561845, 543854 |

`channel_fenrir` stops with `No macro endpoint states in scope`. Its macro,
CLANK, has no amp-only target, because the Meshuggah captures include a
cabinet. This is expected. `python Tools/nastrond_nam_tool.py groups` prints
the target groups derived from the manifest.

When a macro improves a target capture by less than 0.1 dB on pluck or
chord, the audit exits with status 1 and lists it in `failures`. That is a
measurement result, not a tool error. The JSON is written either way.

## 4. Compare with the v1 metric

Run the same commands without `--metric`, which defaults to
`log_spectrum_rms_db`, and write `audit-v1*.json`. For each macro, compare:

```
groups.<macro>.measurements.candidate.<pluck|chord>.mean_scores_db
groups.<macro>.measurements.candidate.<pluck|chord>.per_reference[].endpoint_improvement_db
```

The main question is whether IMPACT and BLOOM change. Their effect lies
mostly below 500 Hz, which the v1 metric underweights. Record both metrics.
Neither one replaces the other in the frozen evidence.

## 5. Optional: compare source versions

To compare an earlier source with the current one, build
`ChimeraOriginalAmpRender` at each commit and run `compare` once per build,
each with its own `--out` directory. Then pass the reports as baseline,
incoming and candidate. The audit refuses mixed inputs:

- All three reports must use the same NAM renderer binary, manifest and
  probe. Use one `~/nam-core/build/tools/render` for every run.
- The audit checks that the default state's level did not drift between
  incoming and candidate by more than 0.001 dB. If the DSP was
  intentionally revoiced, raise the limit with `--max-default-drift-db` and
  say so in the evidence.
- Renderers older than the five-channel revision do not support the
  `channel` key, so compare channel scopes only between builds that have
  channels.

## 6. Record the evidence

Copy into a new `docs/evidence/nastrond-band-<date>/` folder:

- `comparison.json`
- the `audit-band*.json` and `audit-v1*.json` files
- `gain-summary.csv`, generated with
  `python Tools/nastrond_nam_tool.py gain-csv --comparison $R --out gain-summary.csv`
- the `ctest` log of the renderer test

Write the findings into a short report next to the other `NASTROND_*.md`
documents. State the metric for every number, and keep the existing
blockers: Meshuggah captures include a cabinet, Granophyre's provenance is
unverified, absolute input calibration is missing, and owner DI listening
has not been done.
