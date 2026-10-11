# Chimera 1.3.1 beta publication pipeline

Product version: `1.3.1`. Package: `1.3.1-beta.1`. Intended tag:
`v1.3.1-beta.1`. The published 1.3.0 tag and its package bytes remain unchanged.

## Authorization and evidence boundary

On 2026-10-11 KST, after requesting a complete amplifier-knob audit and
hardware-reference corrections, the owner instructed:

> 수정되면 배포랑 업데이트까지 해

This authorizes completion, integration, beta publication and update
distribution after correction and the required verification. It does not
assert that the owner personally tested this build or accepted Studio One,
instrument listening or any individual unexecuted check. No additional
general publication approval is required.

The source includes the 1.3.1 Signal Path, Tone/Final EQ and direct-head-control
work, together with the fixes already shipped in 1.3.0. Source integration and
changing the package channel require fresh native builds. Prior preview and
1.3.0 success cannot certify these new binaries.

## Exact-source release gates

`Tools/publish_beta131.py` requires the latest run at the product source SHA
for every workflow below. Missing, pending, cancelled and failed results do
not fall back to an older success. Every named platform job must succeed.

| Workflow | Coverage |
| --- | --- |
| `build.yml` | Full Windows, macOS and Linux CTest, packaging, Windows installer/MSIX/Defender receipts, candidate assembly and consolidation. |
| `update-candidate.yml` | Windows update candidate and preparation contracts. |
| `cab-panel.yml` | Native CAB contracts on all three platforms. |
| `cab-realtime.yml` | CAB performance and source-bound timing evidence on all three platforms. |
| `amp-head-controls.yml` | Native head controls, complete model/channel layouts, gestures, navigation and screenshots on all three platforms. |
| `signal-path.yml` | Routing, state, Tone/Final EQ integration and native navigation on all three platforms. |
| `graphical-eq.yml` | Graphical EQ audio, state and UI contracts on all three platforms. |

The publisher verifies one nonexpired
`SpectralForge-Chimera-1.3.1-beta.1-Release-Candidate` and one
`Chimera-A-stage-release-gate` from the same Product run. ZIP sizes/digests,
embedded source, run and attempt identities must match. Five fresh binary
packages, their exact hashes, three-platform `update-beta.json`, package
receipts, manual and release notes are checked before any public write.

## Strict validation and the separate beta decision

The original 128-check inventory, strict hard gates, CI ownership, empty
waivers and two existing deferred transpose checks are retained. The
successfully executed automatic receipts cover 21 checks. The other 107 are
unexecuted baseline reports, including 42 strict hard gates, and remain
**BLOCKED**. This is not a full-validation or stable-release PASS.

`Tools/beta_publication131.py` independently recomputes those results against
the fixed policy and ownership fingerprints. The version-specific
`Validation/beta-publication-policy-1.3.1.json` records this turn's authorization
with `testing_reported=false`. It accepts only the complete fixed inventory:
all 21 implemented checks pass; all 107 disclosed gaps are genuinely
unexecuted. Executed failures, changed thresholds, reduced platform coverage,
invented PASS results, new waivers, producer errors or stale evidence reject
publication. The additional native workflow gates above are also mandatory.

The final product commit is recorded in `approved_product_source_sha` only
after integration and review. Until then this value is null and live
publication rejects the candidate. A publication-only descendant commit may
bind the final SHA without rebuilding identical product bytes. The existing
seven-file controller allowlist is version-specific; it excludes production,
artwork, version, build workflows, the strict policy and packaged documents.

The public beta attaches the strict `release-gate.json`, complete raw-evidence
ZIP, `beta-publication-decision.json` and full `BETA_VALIDATION_SCOPE.md`.
Release notes retain Studio One/listening/signing/transpose limitations.

## Publication and update verification

`publish-beta131.yml` is manual-only. Its default performs a read-only review.
Publication requires branch `release/open-beta-1.3.1`, `publish=true`, the full
product source SHA and `confirm=publish-v1.3.1-beta.1`. The Python publisher
checks the live invocation, controller ancestry, current branch and latest
exact-source CI again before the first write. It can wait for existing running
CI, but terminal failures stop immediately.

The transport stages a draft, uploads and verifies every file, then publishes
the prerelease. Existing tags or differing assets are never overwritten.
Unauthenticated release visibility, downloads and the exact update manifest
are verified after publication.

Standalone's existing beta discovery chooses the highest eligible semantic
version with `update-beta.json`; publishing this verified manifest exposes
1.3.1 to installed clients. Hosted VST3/AU opens the release page. Download and
installation remain explicit user actions; the plugin does not replace a
running binary. Homepage links, version text, manual and actual screenshots
are updated only after the release is publicly verified.

Run the historical and new publisher contracts plus the raw-evidence
consolidator before CI. Their positive cases use synthetic fixtures and do
not constitute native product or commercial-DAW acceptance.
