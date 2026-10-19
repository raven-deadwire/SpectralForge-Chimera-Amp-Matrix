# Beta lifecycle — next-version development contract

This work targets `1.3.1-beta.3` in PR #62. The released beta.2 source
`6033236cb81820a43679cbbd34be020804f9658d`, tag, binaries and update feed are
unchanged. No new release is authorized. `RELEASE_REVIEW_PENDING` is present.

## Configuration and identity

`Validation/beta-lifecycle-policy.json` has `expires_at_utc: null` and
`expiry_approved: false`. The owner must select an absolute UTC timestamp before
publication. No compilation time, first-run clock or reinstall starts a new trial.
The existing VERSION/RELEASE_CHANNEL, source SHA and ten-character build ID remain
the only product identity. CMake generates `ChimeraBetaLifecycleBuild.h` and
`chimera-beta-lifecycle.json` from that identity and policy; the existing
`chimera-build-version.json` fingerprints the lifecycle JSON.

Normal unapproved developer builds say that the date is unset and cannot be
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

## Expiry choices for owner review

No option below is selected. Proposed windows measured from the eventual approved
publication date are 90 days, 120 days (recommended balance), or 180 days. Convert
the selected window to an explicit absolute UTC date at release preparation and
obtain approval for that exact date. Each option retains the 14-day advance notice,
active-instance grace, offline clock recovery and project preservation.
