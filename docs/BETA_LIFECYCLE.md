# Beta lifecycle — 1.3.2-beta.1 integration contract

The current integrated candidate is `1.3.2-beta.1` in PR #68. Its approved
expiry is **2027-02-11T14:59:00Z / 2027-02-11 23:59 KST**. Publication and
deployment were requested, but source/binary acceptance and release approval
remain incomplete. `RELEASE_REVIEW_PENDING` is present; expiry approval alone
does not permit publication.

The `1.3.1-beta.3` development branch in PR #62 remains a separate, unpublished
policy scope: its expiry is unset and unapproved. The 1.3.2 approval does not
apply to that branch or its synthetic qualification packages. The released
beta.2 source `6033236cb81820a43679cbbd34be020804f9658d`, tag, binaries and
update feed are unchanged.

## Configuration and identity

`Validation/beta-lifecycle-policy.json` binds version `1.3.2-beta.1` to
`expires_at_utc: "2027-02-11T14:59:00Z"` and `expiry_approved: true`.
Both normal and qualification configurations use this committed deadline.
No compilation time, first-run clock or reinstall starts a new trial.
The existing VERSION/RELEASE_CHANNEL, source SHA and ten-character build ID remain
the only product identity. CMake generates `ChimeraBetaLifecycleBuild.h` and
`chimera-beta-lifecycle.json` from that identity and policy; the existing
`chimera-build-version.json` fingerprints the lifecycle JSON.

For an unapproved policy such as PR #62, normal developer builds say that the date is unset and cannot be
staged as distributions. CI explicitly permits a **qualification-only** synthetic
2099 date. This is a test fixture, not an approved deadline. Such packages are
marked in SETTINGS and in the EN/DE/KR notice; the existing publisher rejects
them even if someone removes the independent review-pending marker.

After approval, the committed policy contains the real absolute UTC date and the
approval flag. CMake and packaging reject an elapsed, malformed, mismatched or
non-UTC date. Qualify a fresh complete source and resulting binary after changing
that policy. The old beta.2 approval receipt cannot approve the new version.

## Audio and state boundary

The processor constructs the existing `InstanceLatch` once. `processBlock` reads
a lock-free atomic decision; it does not read clocks, files, the network, UI state
or an entitlement mutex. A processor admitted before expiry retains full audio
until destruction. Opening/closing an editor, starting/stopping/recording
transport, offline rendering, state replacement and prepare/release do not reset
that boundary. A host reusing the same processor across projects also retains it.

A processor created at or after expiry restores the full graph and host latency,
but its final output is zero. It does not switch to dry input or add noise. The
editor, SETTINGS, update action, automation, preset/reference export, state
serialization and A/B remain available. The permission is never serialized into
the project. User IR, preset, data paths and stable plugin/parameter IDs are not
changed by this policy. Only the user can choose to delete their own work.

RECHECK CLOCK is an explicit control-thread action after correcting a mistaken
system clock. A falsely expired instance recovers with a 50 ms fade of the
processed signal. There is no persistent watermark: ordinary clock rollback can
extend access. An obviously invalid nonpositive clock warns and admits the
instance. This is basic offline lifecycle control, not tamper-proof DRM.

## UI, updates and packaging

SETTINGS shows BETA, version/build and UTC expiry. Its existing button gains a
non-modal warning marker in the final 14 days or an expired/error state. Support
shows the current instance's processing decision and clock recheck. Hosted plugins
reuse the explicit browser GitHub Releases action; Standalone reuses its existing
manifest validation, background updater and verified downloads. GitHub failure
does not invalidate an otherwise valid beta or revoke active-instance grace.
No purchase, account, activation server or hardware binding is included.

The generated `BETA_LIFECYCLE.txt` contains the exact build/date and EN/DE/KR notice.
It is published as a separate preinstallation asset only with an approved release,
shown before Windows installation, and bundled with each platform package. The
Windows installed-file checks and macOS/Linux extracted-package checks compare
lifecycle documents to the staged bytes. Windows's existing exact-file document
preservation guard covers the new JSON too.

## Automated evidence and limits

- Pure C++ injected-clock policy tests and Python policy/metadata regressions.
- Actual processor mono/stereo Classic, Dual Blend, Dual Crossover and Matrix
  routes; equality to unrestricted audio before/at/after expiry, transport and
  offline render, different sample rates/buffers, editor lifetime and reprepare.
- New expired output, finite bounds, equal latency and quantitative 50 ms recovery.
- Actual serializer `.chimera` roundtrip, every parameter ID/order/value, A/B,
  MIDI assignment, personal data path, preserved external files and exact embedded
  IR bytes after removal of the fixture's source IR.
- An injected GitHub transport failure runs through the real update worker and
  SETTINGS error path, without writing user update preferences or using a network.
- Compiled definitions match generated JSON and its configure-time fingerprint.
- Windows/macOS/Linux CI, plus five consecutive macOS native-state restores.
- A separate helper is compiled against the immutable released beta.2 product
  source to generate a real processor state fixture. Candidate binaries on all
  three platforms restore, save and reopen it before, at and after expiry.
  This is a reconstructed released-source fixture, **not a user DAW project**.
- Existing released parameter, update-channel, publisher, approval, packaging,
  installer and document-upgrade regressions remain in force.

Actual Studio One/Cakewalk/Cubase sessions, hardware listening and installed
macOS/Linux audio acceptance are not certified by these automated tests. Keep
unexecuted cases NOT_RUN and retain the existing release gates. PR body/CI results
record concrete outcomes; this document itself is not a PASS verdict.

## Historical expiry options — PR #62 only

No option was selected for PR #62. Proposed windows measured from the eventual approved
publication date are 90 days, 120 days (recommended balance), or 180 days. Convert
the selected window to an explicit absolute UTC date at release preparation and
obtain approval for that exact date. Each option retains the 14-day advance notice,
active-instance grace, offline clock recovery and project preservation.
These historical options do not replace or extend the approved 1.3.2-beta.1 deadline.

## Integration audit — October 14, 2026

Compared PR #62 at `5c858442d0ed8360703e0ce63241a567736608a9` with PR #68 at
`ec47f6c09349af9ee5c1b6aa11a287432610b7ef`. The former is an ancestor of the
latter. Of the 35 files changed by #62, 22 have identical contents at both heads;
the other 13 were reviewed for their actual differences. File overlap alone was
not used as the integration verdict. Hashes, CI identities and policy-isolation
results are in [the audit evidence](evidence/beta-lifecycle-integration-20261014/audit.json).

| Area | Source-level result |
| --- | --- |
| Expiry and clock | `BetaLifecycle.h` / `BetaLifecycleRuntime.h` are identical. Inclusive expiry, 14-day warning, lifetime activation latch, nonpositive-clock warning and explicit recovery are retained. |
| Existing projects | The final mute/fade block and state save/load entry points are identical. The latch is neither reset by state restore nor stored in projects; EQ edit metadata additions do not replace the existing IR/A-B/MIDI paths. |
| State restoration | All old parameter ID/ordinal/version-hint/value checks remain. The historical fixture test now explicitly preserves the 4,967 released parameters and checks defaults for 19 tuner plus 40 dynamics additions. |
| Updates | Support panel, update service and beta notice are unchanged. Network failure does not revoke processing; expired instances retain update and clock-recheck actions. |
| Packaging | The generator, package stagers, installer, installed-document verifier and publisher are identical. Source/version identity, metadata fingerprints, canonical EN/DE/KO notices and synthetic-package rejection remain enforced. |
| Approval separation | #62 stays unset/unapproved; #68 alone uses the approved date above. Cross-version policy/metadata probes reject mixing. The pending marker, exact-source approval receipt and acceptance gates remain required. |

No missing runtime protection was found. The concrete integration omission was
documentation: current #68 manual/update/release-note text still described #62's
unapproved policy. This correction updates those statements and the pending
marker's explanation without changing runtime, policy JSON, tests or gate logic.
Historical #62 and beta.2 evidence stays identified as historical.

At the audited #68 source, lifecycle run
[38500448294](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38500448294)
passed all seven jobs: 6/6 focused checks per OS, five consecutive macOS native
state restores, and beta.2 historical-source restore on all three OSes. The Linux
artifact's ZIP digest, compiled identity, policy JSON and configure-time fingerprint
were verified; the binary reports the approved deadline, not the 2099 fixture.

This is not full product clearance. The same source's full build run
[38500448272](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38500448272)
failed three macOS tests (EQ stopped-audio display decay, room cabinet lane,
tuner acquisition); its release gate was **7 PASS / 121 BLOCKED** and candidate
assembly was skipped. The tuner test's lifecycle failure concerns tuner input
acquisition, not the beta expiry latch. Source inspection does not waive any failure.

The documentation correction passed 94 local Python lifecycle/publisher/approval
checks and the pure C++ policy test. Local packaging checks had 18 passes, one
environment error (CMake unavailable) and three Windows-only skips; those are not
reported as a complete local packaging PASS. Public-document checks and manual
script parsing are recorded separately in the evidence.

Keep #62 open as a Draft with its branch and evidence until the audited successor
is merged and traceable; it can then be closed as superseded, without implying
publication approval. No branch is deleted by this audit. A documentation edit
changes the embedded manual and source identity, so prior native CI/packages do
not certify the corrected commit: fresh exact-source qualification and a separate
publication decision are still required. No released asset or live update feed is changed.
