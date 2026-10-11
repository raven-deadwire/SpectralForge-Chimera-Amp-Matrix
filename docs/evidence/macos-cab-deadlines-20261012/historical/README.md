# Historical CAB deadline audit bundle

This directory records the published 1.3.1-beta.1 candidate and its earlier 781d03dd comparison. It contains measurements and analysis tooling only. It does not change DSP, UI, release tags, installation assets or release approval. Later controlled experiments and accepted performance changes must be recorded separately.

## Contents

| File | Purpose |
|---|---|
| [historical-audit.md](historical-audit.md) | Findings, all 21 latest affected populations, model mappings, measured scope, environment and causal limits. |
| [archives-manifest.json](archives-manifest.json) | Exact GitHub artifact IDs, SHA-256 digests, source/run/attempt bindings, download endpoints and portable relative ZIP paths. |
| [analyze_archives.py](analyze_archives.py) | Standard-library-only, read-only archive analyzer; rejects incomplete or inconsistent populations. |
| [route-summary.csv](route-summary.csv) | All 396 historical OS/candidate populations, including every zero-miss population. |
| [deadline-misses.csv](deadline-misses.csv) | All 128 macOS misses from the two candidate runs; every original CSV field remains unchanged. |
| [validation.json](validation.json) | Successful validation of six artifacts / 633,600 raw candidate callbacks. |
| [corruption-checks.json](corruption-checks.json) | Results from isolated synthetic corruptions; original ZIP bytes were rechecked unchanged. |
| [relative-layout-validation.json](relative-layout-validation.json) | Reproduction using this portable manifest layout and the verified original ZIPs. |

## Download and local layout

ZIP payloads are intentionally not committed. Download the exact artifact ZIP for each row using its GitHub artifact link or the Actions REST `GET /repos/{owner}/{repo}/actions/artifacts/{artifact_id}/zip` endpoint. Follow the temporary redirect and save the returned ZIP bytes at the relative path below. Keep the archive intact: unpacking and recompressing it changes the digest. The manifest paths are resolved relative to the manifest file, independently of the command working directory.

| Relative path | Run / attempt | Artifact | SHA-256 |
|---|---|---|---|
| `archives/latest-macOS.zip` | [38175545597 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597) | [11715842700](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597/artifacts/11715842700) | `2630e2e9f54dd9f3beed1f93499fbe37a7fb53ec76ffd70a4cbd6045972ca239` |
| `archives/latest-Windows.zip` | [38175545597 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597) | [11715229202](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597/artifacts/11715229202) | `e920a95522731490dc4848be8ef8bda51181d944f5deacd95addea6762236b98` |
| `archives/latest-Linux.zip` | [38175545597 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597) | [11714929573](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597/artifacts/11714929573) | `32fa8b64225746023ab2e6f8539ad5f586aea32762bb29ce2711a047c5ed3ccd` |
| `archives/prior-Linux.zip` | [38173348216 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216) | [11714868316](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216/artifacts/11714868316) | `36b01241d53ad8ba7e9b9f14bb321fceef95fffcad8dd9279491c750df6590e9` |
| `archives/prior-Windows.zip` | [38173348216 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216) | [11714653713](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216/artifacts/11714653713) | `fc1e2af5c0c3078ce468bc8c3cf6ea6e9c35cb50144562bf677df95e4ae97a49` |
| `archives/prior-macOS.zip` | [38173348216 / 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216) | [11714524399](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216/artifacts/11714524399) | `9b86ba93f0ccfa9a9983be37df5eb1ae1082ff2dec8028128e27b9e51c7ed631` |

The API URLs, artifact names and expiry times observed at audit are included in the manifest. If an artifact has expired, obtain an independently preserved byte-identical ZIP matching its digest; a newly generated timing run is different evidence.

From this directory, after placing the six ZIPs in `archives/`:

```bash
python3 analyze_archives.py --archives archives-manifest.json --output recomputed
```

Python 3.9 or newer is sufficient; no third-party packages or network calls are used by the analyzer. The output includes `validation.json`, full route and miss CSV/JSON files, run summaries and original environment objects. Any invalid archive returns exit code 2 and an explicit validation failure. Do not treat partial results from that invocation as a complete six-artifact audit. The input ZIPs and manifest are protected against output overwrites.

## What 66 passing populations mean

The 66 count is a test-population count: 36 integration populations plus 30 expanded-buffer populations. It includes independent repeats of overlapping rate/buffer/channel settings. Each population measures three CAB lanes and six microphone paths for 1,600 callbacks. The gate compares CAB wall-clock p99 with one block period.

It is not a total-plugin CPU envelope: amplifier, PRE/POST processing, pitch shifting, host callback release timing and a DAW GUI are outside this harness. It provides no CPU/headroom budget for those additional stages and does not prove every callback met deadline. The published macOS candidate has 66/66 passing p99 populations while retaining 54 individual misses, including 25 whose paired thread CPU also exceeds the block period.

The independent thread-CPU measurement supports diagnosis only. It cannot replace the wall-time gate, and its clock reads enclose the wall interval. The 29 wall-dominant misses are not a complete scheduler diagnosis; the 25 CPU-above-budget records are not automatically proven DSP-only overruns.

## Reproduction integrity

The companion validation was produced with temporary filesystem links to already verified original ZIPs. This avoided duplicating archive payloads while exercising the exact relative manifest layout. No such links or machine-specific source paths are included in this bundle. All six archive hashes were checked again. Full route and miss CSVs were compared against the committed copies; source archives remained byte-identical.

The analyzer checks archive/source/run identity, manifest/receipt agreement, per-CSV SHA-256 and byte counts, a complete distinct 66-key population grid, all 1,600 consecutive callback indices, phase and automation assignments, unchanged sample-rate-derived budgets, finite values, original individual miss flags and platform-specific paired-clock validity/deltas and cumulative clock progression. It reuses sorted index 1584 for p99 and never omits outliers.

The recorded synthetic checks use temporary copies or in-memory modified records. They do not change historical data. Subsequent tests should preserve the unmodified acceptance populations and record diagnostic interventions separately.
