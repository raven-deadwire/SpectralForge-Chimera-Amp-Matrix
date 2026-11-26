# 1.5.0-beta.1 candidate

> **Published.** [v1.5.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.5.0-beta.1)
> was published on 2026-11-23 at 15:17 UTC (2026-11-24 00:17 KST) from product
> source `95a215eda8368c84e75bd0e8d32e9659b106361c`. This record was completed
> after publication; the release's own copy still shows the state before the
> owner's final decisions. The sections below say which is which.

## Owner instructions (Asia/Seoul)

- **2026-11-19:** "다음 릴리즈 언제 가능하냐". Offered a fast path (current
  main plus the small open PRs, as 1.4.2-beta.1) and a feature path, the owner
  answered "빠른 경로로 1.4.2-beta.1 준비해줘", then "만료일 유지하고 CI 통과하면
  #339 머지해서 공개 진행해줘".
- **Scope change:** before #339 could be merged, #334, #335, #336 and #342
  were merged to main (and #341, #343 and #345 after them). The fast path excluded the first
  three, and #342 did not exist when it was chosen. A release is published
  only from main, so 1.4.2-beta.1 as prepared could no longer be published:
  its notes said the parameters and saved projects were unchanged, which was
  no longer true of main. #339 was therefore not merged although its CI had
  passed.
- **2026-11-20:** told this and offered three options, the owner chose
  option 1 ("1번"): include the features and release as **1.5.0-beta.1**.

- **2026-11-20, later:** asked to approve the release-gate totals and the
  merge and publication of this scope, the owner answered "1번 2번 승인", and
  then "승인 기록 PR은 빌드 안 기다리고 머지해줘".
- **Conflicting PR:** another session opened #350 to revert the features and
  publish 1.4.2-beta.1, stating an owner choice of that scope. Publication was
  held and the conflict reported; the owner answered "1.5". #350 was closed
  without merging.

At the time of the release source, preparation was authorized and publication
was not: the owner's earlier "merge #339 and publish" had been given for the
1.4.2-beta.1 scope. Publication was authorized by the decisions above and
recorded in the receipt (`Validation/weekly-publication-policy.json`, #355,
`OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS`). The 1.4.1-beta.1
authorization and its receipt (bound to source
`6bc7216cd2dc72772daaca65a89edbaf6b5a8c43`) do not carry over. No failed or
unexecuted check is promoted to PASS. No 1.4.2-beta.1 was published.

**License period:** 2027-05-31 23:59 KST (`2027-05-31T14:59:00Z`, Unix
`1811775540`), the date of 1.4.0-beta.1 and 1.4.1-beta.1. The owner confirmed
keeping it on 2026-11-19 for the release then named 1.4.2-beta.1; it is
carried to 1.5.0-beta.1 unchanged. A verified tester code lifts the expiry
with no end date.

## Source

- **Preparation branch:** `claude/release-1.5.0-beta.1` (#344), from main `161aedd` (after #341), with main `b3e78e6` (#343, #345) merged in. Merged to main as `c329480`.
- **Product source:** main `95a215e`, which adds #346, #347, #349, #351, #352 and #353 to the merge. The tag `v1.5.0-beta.1` points to it.
- **Controller:** main `9bc1e25` (#355), which differs from the product source only in the receipt.
- **Identity:** `VERSION` 1.5.0, `RELEASE_CHANNEL` beta.1,
  `Validation/beta-lifecycle-policy.json` 1.5.0-beta.1.
- **Release scope:** `Validation/release-policy.json` `release_scope` records
  the 1.5.0-beta.1 preparation authorization (profile `beta_1_3`, the two
  transpose deferrals kept).

## What is in it since 1.4.1-beta.1

All of it is already on main; this branch adds only the release identity,
the notes and the gate totals.

| PR | Change | Saved projects |
|---|---|---|
| #342 | POST second slots: 269 appended parameters, 14-block POST order | load with every second slot off; audio bit-identical (tested) |
| #335 | POST page: drag rack rows to reorder | no change |
| #336 | Metro Clean remodelled as a two-channel head; 48 appended parameters | open on MODERN; **sound can differ** (VOICING / HI MID retired, HIGH PASS and mid-frequency ranges changed) |
| #334 | Reference heads 26–30: new artwork | no change |
| #326, #328, #329, #330, #331, #337, #338, #341, #343, #345 | Auto Set phases 33–42, all off by default | no change |
| #346, #352 | Auto Set phases 43 and 45: muted or solo-silenced rigs and lanes are kept without a listening pass, and "no playing heard" names the band; they refine steps that are off by default | no change |
| #351, #353 | Auto Set phases 44 and 46: tests and documents only | no change |
| #347 | POST second preamp: the plugin reports the changed latency in the block where the audio switches, not one block later | no change |
| #349 | OBS filter test build guide for testers (document) | no change |
| #332 | OBS tests tolerate the runner's timing (tests only) | no change |
| #340 | Windows AAX and installer code-signing guide (document) | no change |
| #325, #327, #333 | tuner fixture digest, 1.4.1 receipt, 1.4.1 notes | no change |

The rows from #346 down reached main after #344 was merged; they are in the
release but were missing from this table in the release's own copy.

Open PRs not included: #354 (Auto Set phase 47, still open; its new test fails
on macOS), #301 (draft that
conflicts with the published manual), #249, #199 and #196 (1.4.0-beta.1 took
the parts the owner chose; #199's evidence files hold digests of real tester
codes and stay out of main). #339 is superseded by this branch.

## Release gate: E1 coverage totals

The strict report needs 21 PASS / 107 BLOCKED. With the new parameters and
screens, the `E1.DSP` and `E1.STATE_UI` checks no longer matched the totals
in `Validation/ci-check-map.json`, which still named the 1.4 catalogue; #342's
own CI reported 19 PASS / 109 BLOCKED although every test passed. The same
happened before 1.4.0-beta.1, and the owner approved updating the totals then.

This branch updates the totals to the values the tests print on this source.
The owner approved the change on 2026-11-20 ("1번 2번 승인"); the release's own
copy of this record still says the approval is needed. #348, a second PR with
the same totals, was closed as superseded. Check
ownership, producers, platforms and every other requirement are unchanged.
See `Tools/fixtures/README.md`.

## Notes and lifecycle

- **Release notes:** [OPEN_BETA_1_5_RELEASE_NOTES.md](OPEN_BETA_1_5_RELEASE_NOTES.md),
  summarised at the top of the packaged
  [OPEN_BETA_RELEASE_NOTES.md](OPEN_BETA_RELEASE_NOTES.md). The packaged notes
  are worded for the published release, so they need no change before
  publication.
- **Lifecycle:** recorded in [BETA_LIFECYCLE.md](BETA_LIFECYCLE.md).

## Auto Set steps merged while this branch is open

New Auto Set phases are being merged to main every few hours. Each one is off
by default and leaves saved projects unchanged. The notes therefore describe
the group ("phase 33 onward, for the amps of the Dual rigs and the Matrix MID
and HIGH lanes") and point to `docs/AUTO_SET_PHASE*.md` in the release source
for the exact list, instead of a count that the next merge would make wrong.
A merge of any other kind before publication still needs the notes revisited.

## Evidence

Before the merge of #344:

- #344 passed CI at `5feebf7`, `417bab9` and `f3f7155`: 26 checks succeeded,
  2 skipped, each with a strict report of 21 PASS / 107 BLOCKED, hard gates
  21 / 63, on Windows, macOS and Linux. At `f3f7155` the macOS
  `ChimeraGraphicalEQGroupEditTests` failed once ("Delete entry removed the
  wrong members", on a runner that logged a 926 ms dispatch overrun) and
  passed on a full rerun of that workflow.
- Local Linux build of main `161aedd`: the three E1 tests pass and print the
  new totals. No full local product build or full CTest run was done.

On the product source `95a215e`:

- All seven required workflows succeeded on the first attempt: build (run
  44893110564), update-candidate, cab-panel, cab-realtime, amp-head-controls,
  signal-path and graphical-eq.
- Strict report of the build run: 21 PASS / 107 BLOCKED, hard gates 21 / 63.

On main commits between the merge and the product source, which were not
published: at `c329480` five of the seven workflows had passed when #349
moved main; at `3cea685` cab-realtime failed on macOS in
`ChimeraCabExpansionIntegrationTests` and was not rerun, because #351 moved
main again.

Publication:

- Receipt #355 was merged without waiting for its own build, as the owner
  asked. The publisher's review mode and then its publication mode succeeded
  (runs 45202776017 and 45202996883).
- The release has 30 assets. The five installers and archives, the manual and
  the checksums were fetched from the published URLs; the manual and the
  release body show 1.5.0-beta.1.

## Final-source evidence

- [x] **Builds and tests:** Windows, macOS and Linux product builds, CTest,
      package verification and candidate assembly succeeded on the product
      source, with a strict report of 21 PASS / 107 BLOCKED, and the seven
      required workflows succeeded on it.
- [x] **Owner decisions:** the E1 totals update and the publication of this
      scope (2026-11-20, confirmed "1.5" on 2026-11-23).
- [x] **Approval receipt:** #355 binds the product SHA, version, strict
      policy and CI-map fingerprints.
- [ ] **Version shown everywhere:** checked in the release body and the
      manual only. SETTINGS, the update manifest and the installed packages
      were not inspected by hand.
- [ ] **Lifecycle P0:** expiry while running, relaunch, update from
      1.4.1-beta.1 with projects, presets and personal IRs. Not performed.
- [ ] **Tester codes:** acceptance of the existing HMAC codes by the
      1.5.0-beta.1 build. Not performed.
- [ ] **Manual:** a review of the German and Korean text by a native speaker.

## Not validated

- real guitar and bass DI listening, including the Drop A takes;
- real DAW and audio-device acceptance;
- signing and notarization;
- AAX (not included in this release);
- the Metro Clean channels, the POST second slots and the Auto Set steps
  added since 1.4.0 on real instruments (automated tests only).

Synthetic and CI results do not stand in for these.
