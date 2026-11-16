# 1.4.1-beta.1 integrated candidate

Owner instruction, 2026-11-15/16 (Asia/Seoul):

- "릴리즈 언제 가능하냐"
- "지금 가능한거 다 넣어"
- Asked to confirm the proposed version **1.4.1-beta.1** and keeping the 1.4
  beta expiry **2027-05-31 23:59 KST**, the owner answered "ㅇㅇ" (yes).

Preparation is authorized. **Publication is not**: it still needs the latest
exact-source workflows to succeed, a fresh source-bound receipt and the
owner's explicit publication decision. The 1.4.0-beta.1 authorization and its
`Validation/weekly-publication-policy.json` receipt (bound to source
`1e02b889794810e14f66996ccaccc560f0f0163b`) do not carry over. No failed or
unexecuted check is promoted to PASS.

**License period:** 2027-05-31 23:59 KST (`2027-05-31T14:59:00Z`, Unix
`1811775540`), the same date as 1.4.0-beta.1; a verified tester code lifts the
expiry with no end date.

## Source

- **Branch:** `claude/release-next-integration` (PR #324), from main
  `88b59f8` (after #319 and #323).
- **Identity:** `VERSION` 1.4.1, `RELEASE_CHANNEL` beta.1,
  `Validation/beta-lifecycle-policy.json` 1.4.1-beta.1.
- **Release scope:** `Validation/release-policy.json` `release_scope` records
  the 1.4.1-beta.1 preparation authorization (profile `beta_1_3`, the two
  transpose deferrals kept).

## What is in it since 1.4.0-beta.1

Already on main:

- Auto Set phases 26–31 (#308, #311, #316, #317, #318, #319), all off by
  default;
- the OBS filter preset-during-save fixes and the wider real-OBS app tests
  (#306, #310, #314, #321);
- test stabilisation (#313 CAB timing gate, #323 UI refresh waits and tuner
  feeder repeat pauses);
- the preset-maintenance 1.3.2 acceptance fixture (#315; #309 was reverted by
  #312 and relanded).

Open PRs and how each is included:

| PR | Included as |
|---|---|
| #322 | merged whole (OBS tests) |
| #320 | merged whole (Auto Set phase 32, off by default) |
| #301 | not included: a draft that rewrites the part of `docs/MANUAL.html` main rewrote for the published 1.4.0-beta.1; the two conflict |
| #249 | not included again: 1.4.0-beta.1 took its non-overlapping parts |
| #199 | not included again: 1.4.0-beta.1 took its documents; the evidence files hold digests of the five real tester codes and stay out of main |
| #196 | not included again: its readiness rules broke a recall guarantee in 1.4 and were left out; its records are on main |

Open PRs created after this branch was cut are not in it.

## Notes and lifecycle

- **Release notes:** [OPEN_BETA_1_4_1_RELEASE_NOTES.md](OPEN_BETA_1_4_1_RELEASE_NOTES.md),
  summarised at the top of the packaged
  [OPEN_BETA_RELEASE_NOTES.md](OPEN_BETA_RELEASE_NOTES.md).
- **Lifecycle:** recorded in [BETA_LIFECYCLE.md](BETA_LIFECYCLE.md).

## Evidence so far

- The integration commit `f3c8f25` (before the version change) passed PR CI:
  63 checks succeeded, 2 skipped. The OBS add-on `macos` job failed once
  (a recording did not start on the runner) and passed on a full rerun of
  that workflow.
- No local build of the merged tree was run.

## Final-source evidence (open)

- [ ] **Builds and tests:** Windows, macOS and Linux full product builds,
      CTest, package verification and candidate assembly succeed on the final
      commit of this branch.
- [ ] **Version shown everywhere:** the packages, update manifest, SETTINGS
      and embedded notes show 1.4.1-beta.1 and the 2027-05-31 expiry.
- [ ] **Lifecycle P0:** expiry while running, relaunch, update from
      1.4.0-beta.1 with projects, presets and personal IRs.
- [ ] **Tester codes:** the existing HMAC codes are accepted by the
      1.4.1-beta.1 build (secret configured for the candidate build).
- [ ] **Manual:** the EN/DE/KO manual shows 1.4.1-beta.1 and names the new
      Auto Set steps. Open: a review of the German and Korean text by a
      native speaker.
- [ ] **Strict evidence:** consolidated accurately; the deferred transpose
      checks and the real DI / DAW / listening gaps stay disclosed.
- [ ] **Approval receipt:** a fresh approval receipt binds the final SHA,
      version, strict policy and CI-map fingerprints.

## Not validated

- real guitar and bass DI listening, including the Drop A takes;
- real DAW and audio-device acceptance;
- signing and notarization;
- AAX in Pro Tools;
- the seven new Auto Set steps on real DI (synthetic tests only).

Synthetic and CI results do not stand in for these.
