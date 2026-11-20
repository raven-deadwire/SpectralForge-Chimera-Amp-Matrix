# 1.4.2-beta.1 candidate

Owner instruction, 2026-11-19 (Asia/Seoul):

- "다음 릴리즈 언제 가능하냐"
- Offered a fast path (current main plus the small open PRs, as 1.4.2-beta.1)
  and a feature path (also #334, #335 and #336, as 1.5.0-beta.1), the owner
  answered: "빠른 경로로 1.4.2-beta.1 준비해줘".

**Publication decision (2026-11-19, Asia/Seoul).** Told that the preparation
PR was up, that the expiry had been left unchanged and unconfirmed, and that
merging and publication needed a separate instruction, the owner answered:
"만료일 유지하고 CI 통과하면 #339 머지해서 공개 진행해줘". The gaps under "Not
validated" below were disclosed to the owner for 1.4.1-beta.1 two days earlier
and are restated in this release's notes. Publication still requires the latest
exact-source workflows to succeed and a fresh source-bound receipt
(`OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS`). The 1.4.1-beta.1 authorization and its
`Validation/weekly-publication-policy.json` receipt (bound to source
`6bc7216cd2dc72772daaca65a89edbaf6b5a8c43`) do not carry over. No failed or
unexecuted check is promoted to PASS.

**Scope kept, source rebuilt (2026-11-20, Asia/Seoul).** #339 was closed
unmerged after #334, #335, #336 and #342 reached main, and #344 proposed
1.5.0-beta.1 instead. The publisher runs only on main, and main may differ
from the product source only in the approval receipt, so the fast-path scope
cannot be published while those features are on main. Shown this, the owner
chose to keep the 1.4.2-beta.1 scope and to take main back to it for the
release ("1.4.2 원래 범위로 진행해줘", then "main 잠깐 되돌리기", confirmed
over #344 with "1.4.2 원래 범위"). After publication the reverted features
are restored on main.

**License period:** 2027-05-31 23:59 KST (`2027-05-31T14:59:00Z`, Unix
`1811775540`), the date of 1.4.0-beta.1 and 1.4.1-beta.1, confirmed by the
owner for 1.4.2-beta.1 on 2026-11-19. A verified tester code lifts the expiry
with no end date.

## Source

- **Branch:** `claude/release-1.4.2-scope`, from main `b3e78e6` (after #345).
  It reverts the merges of #342 (POST second slots), #336 (Metro Clean
  TT-800), #335 (POST rack drag) and #334 (reference head art), then carries
  the preparation commits of the closed #339 (first cut from main `2aa300e`).
  The product source equals #339's apart from Auto Set phases 40–42 and two
  repository documents.
- **Identity:** `VERSION` 1.4.2, `RELEASE_CHANNEL` beta.1,
  `Validation/beta-lifecycle-policy.json` 1.4.2-beta.1.
- **Release scope:** `Validation/release-policy.json` `release_scope` records
  the 1.4.2-beta.1 preparation authorization (profile `beta_1_3`, the two
  transpose deferrals kept).

## What is in it since 1.4.1-beta.1

Already on main:

- Auto Set phases 33–38 (#326, #328, #329, #330, #331, #337), all off by
  default;
- OBS tests: two real-OBS checks tolerate the runner's timing (#332; tests
  only);
- the tuner fixture digest registered with the owner's approval (#325) and
  the 1.4.1-beta.1 publication receipt (#327);
- the 1.4.1-beta.1 notes on the standalone OBS filter packages (#333).

Open PRs and how each is included:

| PR | Included as |
|---|---|
| #338 | on main (Auto Set phase 39, off by default) |
| #341, #343, #345 | on main (Auto Set phases 40–42 for the Matrix MID and HIGH lanes, off by default) |
| #340 | on main (AAX / Windows signing guide; repository document) |
| #334, #335, #336, #342 | reverted for this release: the owner chose the fast path; they change the screens, the Metro Clean sound and the parameter list, and return to main after publication |
| #301 | not included: a draft that conflicts with the published manual |
| #249, #199, #196 | not included again: 1.4.0-beta.1 took the parts the owner chose; #199's evidence files hold digests of real tester codes and stay out of main |

Open PRs (#344, #346, #347, #348, #349) are not in it.

## Notes and lifecycle

- **Release notes:** [OPEN_BETA_1_4_2_RELEASE_NOTES.md](OPEN_BETA_1_4_2_RELEASE_NOTES.md),
  summarised at the top of the packaged
  [OPEN_BETA_RELEASE_NOTES.md](OPEN_BETA_RELEASE_NOTES.md). The packaged notes
  are worded for the published release, so they need no change before
  publication.
- **Lifecycle:** recorded in [BETA_LIFECYCLE.md](BETA_LIFECYCLE.md).

## Evidence so far

- Main `fde0d90` (through phase 36) passed the build workflow twice, each
  with a strict report of 21 PASS / 107 BLOCKED.
- #338's own CI was still running when the first branch was cut.
- The four reverts applied without conflicts; the product source differs
  from #339's only by Auto Set phases 40–42 (`git diff 632beb9`).
- No local product build of this branch was run. Local: `test_beta_lifecycle`,
  `test_publish_release`, `test_weekly_publication`, `test_preset_maintenance`
  and `test_packaging_version` pass.

## Final-source evidence (open)

- [ ] **Builds and tests:** Windows, macOS and Linux full product builds,
      CTest, package verification and candidate assembly succeed on the final
      commit of this branch, and the seven required workflows succeed on the
      product source on main.
- [ ] **Version shown everywhere:** the packages, update manifest, SETTINGS
      and embedded notes show 1.4.2-beta.1 and the 2027-05-31 expiry.
- [ ] **Lifecycle P0:** expiry while running, relaunch, update from
      1.4.1-beta.1 with projects, presets and personal IRs.
- [ ] **Tester codes:** the existing HMAC codes are accepted by the
      1.4.2-beta.1 build (secret configured for the candidate build).
- [ ] **Manual:** the EN/DE/KO manual shows 1.4.2-beta.1 and names the new
      Auto Set steps. Open: a review of the German and Korean text by a
      native speaker.
- [x] **Expiry confirmation:** the owner confirmed 2027-05-31 23:59 KST for
      1.4.2-beta.1 (2026-11-19).
- [ ] **Approval receipt:** a fresh approval receipt binds the final SHA,
      version, strict policy and CI-map fingerprints.

## Not validated

- real guitar and bass DI listening, including the Drop A takes;
- real DAW and audio-device acceptance;
- signing and notarization;
- AAX in Pro Tools;
- the Auto Set steps added in 1.4.1 and 1.4.2 on real DI (synthetic tests
  only).

Synthetic and CI results do not stand in for these.
