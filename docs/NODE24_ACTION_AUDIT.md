# Node 24 action migration — 2026-10-07

Audited exact sources: main `15427d5717d30f566de27bd3dcc0eafa459a5563`,
PR #22 `a4b744f9b65dc98eff50b7429e77ea91d7141e5e`, and
PR #23 `04152aaf121d449ffef29efc395784e655f9ffb6`.

| Action | Previous | Target | Audited upstream commit |
| --- | --- | --- | --- |
| checkout | v4 / v4.2.2 SHA | v5 / v5.1.0 SHA | fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09 |
| setup-python | v5 | v6 | ece7cb06caefa5fff74198d8649806c4678c61a1 |
| setup-node | v4 | v5 | a0853c24544627f65ddf259abe73b1d18a591444 |
| upload-artifact | v4 | v6 | b7c566a772e6b6bfb58ed0dc250532a479d7789f |
| download-artifact | v4 | v7 | 37930b1c2abaa49bbe596cd826c3c89aef350131 |

Each target's `action.yml` declares `runs.using: node24`. Notably,
download-artifact v5 and v6 still declare node20 at audit time; v7 is required.
Existing tag versus SHA pin style is retained, including the three SHA-pinned
checkout uses. GitHub-hosted runners are used throughout; Node 24 actions require
runner 2.327.1 or later. No runtime force/legacy opt-out environment flag is added.

## Compatibility decisions

- checkout v5 retains the local git config credential model; v6 changes it.
  All existing `ref`, `fetch-depth`, repository/path, recursive submodule and
  `persist-credentials` inputs remain unchanged, including omitted defaults.
  No `pull_request_target` or `workflow_run` event is used.
- Every existing Python setup uses 3.12 without dependency caching. No `cache`
  or `cache-dependency-path` is added or removed. setup-node still installs Node
  22 for application tooling; v5's new automatic package cache is explicitly
  disabled to retain v4's opt-in cache behavior.
- Upload names, paths/globs/exclusions, conditions, hidden-file defaults,
  retention, overwrite and compression are unchanged. Download name, run ID,
  token, pattern, path and merge inputs are unchanged.
- download v7 flattens singleton pattern results even with `merge-multiple:
  false`. On PR #23, a small transport adapter restores the original artifact
  directory using its actual current-run API name. The pattern still selects
  unexpected artifacts, so existing rejection remains intact. It never infers
  names from payload `kind`, changes file bytes, or edits verdicts. Only this
  job gains `actions: read` to retrieve names; contents permission stays read.
- Artifact digests remain SHA-256 of each newly uploaded ZIP, not a promise of
  identical ZIP hashes across Node versions or runs. Payload identity and
  checksums, source/run/attempt validation and the publisher's independent
  archive verification are unchanged.
- No NAM source, fidelity threshold, release-gate policy/waiver, production
  publisher, or manual acceptance record is modified. New smoke artifacts have
  their own `node24-contract-*` namespace and cannot qualify a product release.

## Regression coverage

`Tools/fixtures/node24-workflows.json` records original workflow text hashes.
`Tools/test_workflow_actions.py` reverses only the documented migration edits
and verifies every other byte, covering names, paths, inputs, credentials,
source expressions, permissions, conditions, job dependencies and commands.
The PR #23 adapter's exact read-only permission/step additions are allowlisted.
The fixture is specific to the audited baseline; reconcile it deliberately
when merging later workflow changes.

`Node 24 action contracts` performs real upload/download roundtrips on Linux,
Windows and macOS. It verifies named downloads, singleton layout restoration,
merged and separate layouts, SHA-256 against the actual downloaded ZIP and the
GitHub artifact metadata, exact checkout/payload source and run/attempt, both
checkout credential modes, and the disabled Python dependency cache. Local
negative tests reject corrupt digests, stale source/run/attempt, unsafe paths
and directory collisions, and preserve unexpected artifact names.

Reproduce local migration checks:

```sh
python -m pip install PyYAML==6.0.3
python Tools/test_workflow_actions.py -v
python -m unittest discover -s Tools -p 'test_*.py' -v
# PR #22 only, existing fidelity/source/acceptance failure checks:
python -m unittest discover -s Tools/Nam -p test_ci.py -v
```

The transport smoke does not train NAMs or execute release publication.
A successful workflow is not NAM fidelity, manual acceptance or release approval.

## Primary sources

- https://github.com/actions/checkout/blob/fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09/action.yml
- https://github.com/actions/checkout/blob/fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09/README.md
- https://github.com/actions/setup-python/blob/ece7cb06caefa5fff74198d8649806c4678c61a1/action.yml
- https://github.com/actions/setup-node/blob/a0853c24544627f65ddf259abe73b1d18a591444/action.yml
- https://github.com/actions/upload-artifact/releases/tag/v6.0.0
- https://github.com/actions/download-artifact/blob/37930b1c2abaa49bbe596cd826c3c89aef350131/src/download-artifact.ts
