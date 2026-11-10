# 1.4.0-beta.1 integrated candidate

Owner instruction, 2026-11-09 (Asia/Seoul):

- "현재 돌고 있는 작업들이 문제 없이 끝나면 이제 1.4버전 릴리즈 할 준비 해"
- "다른 작업들까지 전부 반영해야해"

The owner chose:

- version **1.4.0-beta.1** (beta channel);
- beta expiry **2027-05-31 23:59 KST** (`2027-05-31T14:59:00Z`, Unix `1811775540`);
- every open PR, drafts included;
- #287 fixed and included.

For three PRs the owner decided the treatment below.

**Preparation is authorized; publication is not.** Fresh exact-source
qualification on Windows, macOS and Linux is still required, then owner
acceptance and an explicit publication decision with a new source-bound
receipt. The 1.3.2-beta.1 and 1.3.2-beta.2 authorizations do not carry over.
No failed or unexecuted check is promoted to PASS.

## Source

- **Branch:** `release/1.4.0-beta.1`, from main
  `bf2b022f5ad2f6238976921eab5feed15cbba9f5` (after #294).
- **Identity:** `VERSION` 1.4.0, `RELEASE_CHANNEL` beta.1,
  `Validation/beta-lifecycle-policy.json` 1.4.0-beta.1.
- **Release scope:** `Validation/release-policy.json` `release_scope` records
  the 1.4.0-beta.1 preparation authorization (profile `beta_1_3`, the two
  transpose deferrals kept).

## Open PRs and how each is included

| PR | Included as |
|---|---|
| #283, #284, #295, #296, #297 | merged whole |
| #272, #258, #252, #230 | merged whole |
| #287 | merged whole; its macOS gesture test fixed, and #283's single-band DELETE test removed (superseded by #287) |
| #250 | merged whole |
| #238 | merged whole (research tools; no product change) |
| #171 | merged whole |
| #199 | documents only (owner decision) |
| #196 | records only: its readiness rules were tried and left out (see below) |
| #249 | the non-overlapping parts only (owner decision) |

Notes on the "merged whole" rows:

- **#284 and #297:** both appended an Auto Set option; both options are kept.
- **#287:** the external-automation case now waits for the panel's timer
  instead of a fixed 60 ms. #287 replaced #283's DELETE (the active band only)
  with deleting the whole selection, so #283's single-band DELETE check is
  removed; `groupDeleteEntryPoints` covers deletion now.
- **#250:** its cabinet edit fix was already on main; it adds the Gate capture
  stabilisation and the review records.
- **#171:** main's newer (11/04) AAX status is kept. The October 30
  "invalid code" lines now point to the November 3 registration.

### #199: documents only

- **Kept:** the HMAC tester codes stay.
- **Added:** the migration review and the allowlist note, marked as records.
- **Not included:**
  - the allowlist authentication, the migrate tool and the build changes;
  - the evidence files, because they hold digests of the five real codes,
    which can be reversed for a short code.

### #196: records only

- **Tried:** the audio-thread readiness rules (readiness at both block
  boundaries, the stricter `Cab::settled()`, zero-weight paths) were merged
  into this branch.
- **Left out:** they broke an existing recall guarantee.
  `ChimeraSignalOrderTests` measured an audio difference of 0.1475 between a
  processor and its restored copy, which must be 0; reverting them restored 0.
  Per the owner's rule for #196, they are not in 1.4.
- **Kept:** the IR worker stays main's. #196's measurements remain as
  records.

### #249: non-overlapping parts only

- **Included:**
  - the Windows thread-dump tool;
  - the test-only startup observer (off by default, never packaged);
  - the startup audit and its evidence.
- **Not included:** the startup probe and the harness changes. Main waits for
  OBS to finish loading in its own way.

Open PRs created by other sessions after this branch was cut are not in it;
they are added before the final source is frozen.

## Notes and lifecycle

- **Release notes:** [OPEN_BETA_1_4_RELEASE_NOTES.md](OPEN_BETA_1_4_RELEASE_NOTES.md),
  summarised at the top of the packaged
  [OPEN_BETA_RELEASE_NOTES.md](OPEN_BETA_RELEASE_NOTES.md).
- **Lifecycle:** recorded in [BETA_LIFECYCLE.md](BETA_LIFECYCLE.md).

## Final-source evidence (open)

- [ ] **Builds and tests:** Windows, macOS and Linux full product builds,
      CTest, package verification and candidate assembly succeed on the final
      commit of this branch.
- [ ] **Version shown everywhere:** the packages, update manifest, SETTINGS
      and embedded notes show 1.4.0-beta.1 and the 2027-05-31 expiry.
- [ ] **Lifecycle P0:** expiry while running, relaunch, update from
      1.3.2-beta.1 with projects, presets and personal IRs.
- [ ] **Tester codes:** the existing HMAC codes are accepted by the
      1.4.0-beta.1 build (secret configured for the candidate build).
- [ ] **Manual:** the EN/DE/KO manual describes the new pages (SYNTH, Room,
      signal order, EQ group editing / SOLO / per-EQ A/B, Auto Set phases).
      Done: a 1.4 section in all three languages, the 132-model count, the
      1.4.0-beta.1 header/footer, and model cards for the 47 new models
      (PRE 12, AMP 5, POST 18, a new CREATIVE 9 · SYNTH 3 chapter). Open: a
      review of the new German and Korean descriptions by a native speaker,
      and the AMP input-route table for the five new amps.
- [ ] **Strict evidence:** consolidated accurately; the deferred transpose
      checks and the real DI / DAW / listening gaps stay disclosed.
- [ ] **Approval receipt:** a fresh weekly approval receipt binds the final
      SHA, version, strict policy and CI-map fingerprints.

## Not validated

- real guitar and bass DI listening, including the Drop A takes;
- real DAW and audio-device acceptance;
- signing and notarization;
- AAX in Pro Tools.

Synthetic and CI results do not stand in for these.
