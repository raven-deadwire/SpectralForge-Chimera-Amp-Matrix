# Beta lifecycle — published policy and SETTINGS development

## Current public baseline (2026-10-18 KST)

[v1.3.2-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.2-beta.1)
was published on 2026-10-18 at 01:51:47 KST. Its product source is
`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, build ID `3bec6c70a5`.
The former beta.3 development / unapproved-date text described PR #62's
historical `1.3.1-beta.3` candidate, not the current public release.

| Policy field | Approved public v1.3.2-beta.1 value |
| --- | --- |
| Product / package version | `1.3.2` / `1.3.2-beta.1` |
| UTC expiry (inclusive) | `2027-02-11T14:59:00Z` |
| Korea expiry | `2027-02-11 23:59:00 KST (UTC+09:00)` |
| Unix expiry | `1802357940` |
| Approval | `expiry_approved: true` |
| Advance warning | `1209600` seconds (14 days) |
| Already active instance | Audio continues until instance destruction |
| Newly created expired instance | Final output muted, graph and state preserved |
| Network / payment / account | Not required |

The policy is `Validation/beta-lifecycle-policy.json`. Published tags, binaries,
release assets and historical evidence remain immutable. This branch adds a
countdown and documentation corrections for review; it is not a newly published
release and has no new release approval.

## Configuration and next-version identity

The existing VERSION/RELEASE_CHANNEL, source SHA and ten-character build ID remain
the only product identity. CMake generates `ChimeraBetaLifecycleBuild.h` and
`chimera-beta-lifecycle.json` from that identity and policy; the existing
`chimera-build-version.json` fingerprints the lifecycle JSON. UTC expiry is the
source of truth; the KST text in the header and distribution notice is derived
from it using a fixed +09:00 offset.

**The approved date above applies only to `1.3.2-beta.1`.** This implementation PR
keeps that baseline version and policy unchanged. When preparing another version,
update the policy version explicitly and leave `expires_at_utc: null` /
`expiry_approved: false` until that version's exact date is approved. A changed
source version with the old policy is rejected; no date or 90/120/180-day period
is inherited, selected or inferred. Rebuilding/reinstalling never restarts a trial.

Normal unapproved developer builds show an unset date and cannot be staged as
distributions. CI may explicitly use a marked **qualification-only** synthetic
2099 fixture, which the publisher rejects. After separate approval, qualification
must bind the new policy, complete source and binary; earlier release approvals
and test evidence cannot approve a modified binary.

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

SETTINGS shows BETA, version/build and exact UTC expiry. This development change
adds KST (UTC+09:00) and `Remaining: N days HH hours MM minutes SS seconds`.
The existing `Evaluation.secondsRemaining` supplies the countdown and status
from one evaluation; no second timer clock, local date subtraction or rounded-up
day count is introduced. The panel's existing 4 Hz UI refresh resamples UTC at
one-second precision. At/after the exact expiry the display clamps to zero and
says expired, while the separate instance latch still determines audio permission.
Invalid clocks/metadata and unconfigured builds show unavailable, never a
fabricated remaining duration. OS timezone/DST changes cannot move the deadline;
a wall-clock correction changes the remaining duration on the next UI refresh.
A previously muted instance still requires explicit RECHECK CLOCK after correction. Its existing button gains a
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

- Pure C++ injected-clock policy/countdown tests: 14-day, day, hour, minute,
  final-second and inclusive expiry boundaries; zero clamp and clock correction.
- Actual SETTINGS timer refresh: compiled UTC/KST/version/build, process timezone
  changes (UTC/KST/DST/fractional offset), date jumps, active grace and muted recovery.
- Python policy/metadata tests: UTC/KST day/year/leap-day rollover; a new version
  cannot inherit the old approved policy; unapproved publication remains rejected.
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
- Existing Windows/macOS/Linux CI and five consecutive macOS native-state restores.
  Concrete results for this change must be read from its own PR/CI, not inferred
  from previous runs.
- An expired instance's save is restored into a synthetic successor and resaved /
  reopened with parameter identities, A/B, MIDI automation and exact embedded IR
  bytes retained even after the source IR is removed. This synthetic successor
  does not assign a deadline to a real future release.
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

## Review checklist before the next distribution

Compare the exact-source compiled identity, policy JSON, SETTINGS, generated
`BETA_LIFECYCLE.txt`, package JSON, release note and update manifest version/build.
The update manifest currently carries version and download identity, not an
expiry field; the packaged lifecycle JSON and notice carry the deadline. Obtain
a separate deadline approval for the chosen next version and qualify that source.
No release gate or installed DAW acceptance is waived by this display change.
