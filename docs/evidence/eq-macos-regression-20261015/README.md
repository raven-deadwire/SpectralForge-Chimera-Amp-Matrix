# PR #59 macOS regression diagnosis — 2026-10-15 KST

Status: ROOT CAUSE CONFIRMED; final 3-OS EQ and native integration all PASS. Windows and Linux install automation completed; macOS 46/46 and universal package checks PASS. Separate full-Windows CAB timing gate FAILED. No release approval.

## Scope and immutable baseline

- Development PR: #59, related acceptance issue: #53.
- Original executed source: `e52976f869ea872e3094f3a945d74496665285c5`.
- Original documentation-only head: `a47e879b7e1259b5d3ac71243be3bd187de0c1ec`.
- First diagnostic candidate: `195bf12e950ae9cc59e82be22d878baf3b929032`.
- Final candidate: `babfecda995141ef3001fc6f9c04c05409ff7ad6`.
- Intermediate Q correction: `a915dd6fd8a4d1fc59804157eb196394b50ab02f`.
- Original EQ run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38280760403
- Candidate EQ run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38599009075
- Candidate native state/navigation: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38599009096
- Candidate package/full test run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38599009356
- Candidate Windows installation/lifecycle: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38599008997

Both baseline and candidate have the identical `Source` Git tree
`d6de02d72f92d550ce131e8ac07109eb9ae99467`. No production C++, DSP, host parameter,
factory preset, CMake, release version, channel, workflow or acceptance threshold
is changed. The candidate changes only three regression tests and their shared
assertion helper (four test files). This is not a fix to the audio engine.

## Distinguishing numeric representation from event ordering

The native UI check required `gain == 0` after `+ BAND`. The backend check combined
`undo()` and `output == -9` into one failure message. A decimal grid position in
JUCE's `NormalisableRange<float>` need not have the literal float's bits on every
architecture/compiler. JUCE 8.0.8 uses:

`start + interval * floor((value - start) / interval + 0.5)`

With `.01f` interval, contracting the multiply/add into FMA preserves the small
representation error in `.01f`; separate multiplication/addition can round it away.
A local GCC 13.3 probe using the actual unmodified JUCE 8.0.8 headers reproduces:

| Requested raw value | contraction off | FMA enabled |
|---|---:|---:|
| EQ gain 0 dB | 0 | -5.36441803e-07 |
| Output -9 dB | -9 | -9.00000095 |
| EQ frequency 1000 Hz | 1000 | 1000 |

The original complete UI and editing test translation units were also rebuilt from
`e52976f` against the same production library with FMA enabled. Both original
failure messages reproduced, exit 1 each (see `local-original-*-failure.log`).

Actual macOS runner output independently confirms this diagnosis. Artifact
11912560722 from [first-candidate integration run 38594295857](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38594295857)
has SHA-256 `c0aaa3013397a200c2644ec866d6bf030e4db1ebbbfab9ee81030e4d58c13d29`.
The retained LastTest.log records gain `-5.36441803e-07`, output before and after
Undo `-9.00000095`, `undone=1` and external epoch `1 -> 1`. Both banks pass the
notification-order trace; all 4,845 non-EQ values remain bit-identical. Thus the
old assertions fail even when initialization and history ownership are correct.

The revised checks require exact bits against the released parameter's canonical
roundtrip plus the existing numeric intent tolerance. This is not an arbitrary
larger epsilon. The UI also checks default Q using the established .002 tolerance, plus exact filter type and enabled state. The
Undo test separately checks history availability, replay success, unchanged raw
output bits, unchanged external revision and all 4,845 non-EQ normalized values.

A new synchronous notification trace asserts both banks use:

- Reuse: frequency, gain, Q, type, then enabled=true; tuning stays disabled.
- Undo reuse: enabled=false, then original frequency/gain/Q/type.
- Redo reuse: tuning first, enabled=true last.

This trace uses the underlying parameter rather than APVTS raw listener ordering.
The native UI still waits for the real asynchronous button dispatch; the backend
regression does not pump a native event loop. Neither test adds sleeps, retries,
skipped assertions or an OS-specific production branch.

## Restore, automation and history boundaries

`EQEditState::addBand` initializes the first vacant persistent slot and enables it
last. `removeBand` disables before clearing occupancy. `replay` has separate disable,
tuning and enable phases. Existing host parameter IDs and automation lanes do not
move. No replacement parameter or extra host parameter is introduced.

Only the 122 EQ parameters are bound to the EQ history listener. Non-EQ changes
must not advance its external revision. External EQ writes, including equal-value
notifications and synchronous reentry, invalidate local history. Thread-local
own-write classification remains bit-exact; it is deliberately NOT relaxed.

`RestoreScope` establishes a barrier before `replaceState` and after occupancy
restoration. Factory recall, project recall and global A/B remain history boundaries;
ordinary save does not clear history. Existing tests cover legacy/no-EQ state,
eight malformed metadata cases, binary project/A-B recall, 48 factory presets,
64-operation history bound, automation of deleted slots, same-value notifications,
reentrant host changes and exact four-rate DSP output equivalence.

## October 23 acceptance boundary

Automatable checks and actual host/device acceptance are separate:

| Check | Automation available | Evidence required |
|---|---|---|
| All 4,967 IDs, EQ 122 contract, 48 presets | EQ backend and catalog tests | Candidate exact-source results |
| Binary state/A-B/legacy/malformed recall | EQ backend and native state tests | Candidate CI and native logs |
| Native event dispatch, pending numeric input, editor reopen, Freeze | Native EQ UI suite | Candidate CI |
| Windows VST3 scan/load/state/recreate/unload | Independent VST3 ABI harness | Candidate Windows CI; not a named DAW |
| Windows install/repair/uninstall and user data preservation | Existing installer verification | Candidate installer CI |
| macOS universal pkg payload | Existing package builder | Architecture/signature/payload hash checks; not actual installation |
| Linux deb install/painted launch/repair/remove | Disposable Ubuntu 24.04 container | Exact installed hashes, normal post-paint close and preserved test user data; no physical audio |
| Studio One Read/Touch/Latch/Write, DAW Undo and existing lane reuse | Not available in current CI | Supported actual DAW session |
| Prior public binary-created DAW project roundtrip | Not established by synthetic migration fixtures | Old and new binaries in actual DAW |
| Physical interface, listening, high-rate sustained operation | Not available in current CI | Representative hardware and DI sessions |
| Cakewalk Sonar/Cubase 10.5 shutdown reports | ABI harness is partial coverage | Named-host reproduction |

October 23 remains a conditional development acceptance target. No merge,
public release, tag, publisher workflow or approval is performed by this work.


## Intermediate failures retained and resolved

The first diagnostic candidate added an over-strict exact canonical check for Q.
It failed locally and on both Windows/macOS: the editor's extra snap followed by
logarithmic host conversion can yield adjacent float representations of the same
.001 grid position. This was a test regression introduced during diagnosis, not a
production change. It was corrected to the pre-existing backend .002 default-Q
contract in `a915dd6`; frequency/gain and output checks retain exact canonical bits.
The failed first-candidate JUnit/logs are retained rather than overwritten.

The wider native navigation suite exposed another stale expectation inherited
from PR #59: it searched for a frequency control with selected band -1 in an empty
bank. Production `focusForNavigation` already deliberately focuses the graph in
that case. The final test covers empty banks in Classic/Matrix and occupied banks
in both Dual configurations, checking both banks. It retains the original
frequency-focus assertion for occupied slots, exact graph focus for empty slots,
and the zero-host-write/gesture navigation assertion. No production focus behavior
was changed. The old check reproduced locally (7.60 s); the corrected full native
navigation test passed (76.31 s).

## Local verification (GCC 13.3 / JUCE 8.0.8 / FMA diagnostic)

| Last relevant execution | Result | Duration |
|---|---|---:|
| Original UI assertion, unmodified old test | Expected failure reproduced | See log |
| Original Undo assertion, unmodified old test | Expected failure reproduced | See log |
| Native state restoration | PASS | 5.65 s |
| EQ editing backend, including contract and order/isolation assertions | PASS | 28.47 s |
| Native EQ UI after Q-check correction | PASS | 121.65 s |
| Native signal navigation after empty/occupied fixture correction | PASS | 76.31 s |

These are separately retained executions, not a claimed single 4/4 CTest run.
The production library retains build identity `195bf12`; final test translation
units were rebuilt against its identical production Source tree. Local FMA flags
are diagnostic and are not a change to shipping compiler flags. Standard final
candidate CI must supply platform-qualified results.

Local catalog, candidate-transfer, release-gate, release-scope and CTest evidence
producer contracts passed. Packaging-version tests ran 11 cases: 8 passed, 3
Windows/PowerShell-specific cases skipped locally; those skips are not Windows
acceptance. The initial local build left two zero-byte generated artwork objects;
regenerating those objects resolved linking, without changing repository source.
The environment rejects UNIX X11 sockets, so `run_tcp_ui.py` uses an authenticated
TCP Xvfb/Openbox desktop and the original native readiness/input checks. No fake
window atoms, relaxed native assertions or OS-specific product code were used.

## Reproduction notes

The ordinary CI commands are unchanged in `graphical-eq.yml` and
`signal-path.yml`. They check out the PR head explicitly. For the additional Linux
FMA diagnosis, configure Release with `-DCMAKE_CXX_FLAGS="-mfma -ffp-contract=fast"`,
`CHIMERA_EQ_DEVELOPMENT_PREVIEW=ON` and `CHIMERA_UI_PROBES=ON`, using JUCE 8.0.8.
The TCP desktop script is invoked from the repository root; create
`evidence-local/` first. Its subprocess receives the real native display.

`local-original-build-commands.json` retains the actual original-test compilation
and linking argv (including the original diagnostic workspace paths). Rebuild the
old test translation units from `git show e52976f:Tests/<file>` outside the tracked
Tests tree, substituting your checkout/build paths. `run_original_tests.py` expects
those two binaries under `evidence-local/baseline/`; it succeeds only if both
original failure messages occur with exit 1. These scripts reproduce the diagnosis
and are not new release-gate tests. Do not count the expected old failures as
successful product acceptance.

## Final-candidate CI results

Exact candidate: `babfecda995141ef3001fc6f9c04c05409ff7ad6`.

| Suite | Windows | macOS | Linux |
|---|---|---|---|
| Original EQ run 38280760403 | 8/8 PASS, 409.55 s | 6/8 PASS, 291.85 s | 8/8 PASS, 483.74 s |
| Final EQ run 38599009075 | 8/8 PASS, 407.44 s | 8/8 PASS, 519.82 s | 8/8 PASS, 484.72 s |
| Final native integration 38599009096 | 8/8 PASS, 352.11 s | 8/8 PASS, 471.12 s | 9/9 PASS, 380.93 s |

Completed final suites have zero failed, disabled or skipped CTest cases. OS-specific
workflow steps and Python packaging subcases may be intentionally inapplicable;
that is not extra cross-platform acceptance. The macOS baseline and final EQ jobs
use the same macOS 15.7.9 ARM64 runner image `20260907.0337.1` and standard Release
compiler flags. No Intel-native macOS run is claimed. The longer final macOS test
duration includes UI/editing work that the old assertions aborted early, so total
suite duration is not a controlled performance comparison.

The final macOS raw log confirms both banks' canonical neutral gains, exact output
bits before/after Undo, unchanged epoch, slot-notification order, all host contracts
and all factory resets. JUnit, LastTest.log, build-version metadata, runner provenance
and archive/file SHA-256 values are retained in `final-*` evidence files.

## Whole-product and installation evidence

All entries below refer to the exact `babfecda995141ef3001fc6f9c04c05409ff7ad6`
source. A later evidence-only commit is not a new tested executable.

| Run / scope | Result | Acceptance limit |
|---|---|---|
| Full Linux matrix, 38599009356 | 47/47 PASS, 1279.66 s | Hosted Ubuntu 22.04; zero CTest failures/skips |
| Full Windows matrix, 38599009356 | 49/50 PASS, 1750.96 s; CAB timing gate FAIL | Windows packaging/install/MSIX/scan skipped in this run |
| Independent Windows candidate, 38599008997 | 50/50 PASS, JUnit total 1765 s | Separate runner, same SHA; does not erase the failed CAB result |
| Windows VST3 lifecycle ABI harness | 7/7 scenarios PASS, 60.05 s in candidate | Loader, editor/pending/reopen/multiple-instance/live-remove; not a named DAW |
| Windows candidate installer | PASS | Real default/custom Unicode install, repair, removal, VST3 loader, Start Menu launch, preserved presets/IRs |
| Linux package verification | PASS | deb/tar payload hashes, executable/VST3/manual and builder dependency checks |
| Linux actual deb install in disposable Ubuntu 24.04 | PASS after actual first paint readiness | Install, 24 payload hashes, standalone launch/normal close, binary repair, restart, removal and isolated user-data sentinel |
| macOS universal full matrix, 38599009356 | 46/46 PASS, 1256.18 s | ARM64 native execution; zero CTest failures/skips |
| macOS universal package | PASS | Standalone/VST3/AU arm64+x86_64 slices, ad-hoc code seals, extracted binary hashes; not actual installation |

The full workflow concluded FAILURE: Windows CAB gate failed, the repository release
gate remained BLOCKED, and `assemble-candidate` was skipped. The release-gate
summary is retained in `final-release-gate/`; its broad unmet release requirements
are not additional executed EQ failures. All eight other automatically triggered
workflows completed successfully. No release gate was overridden.

The independent Windows installer is `SpectralForge-Chimera-update-babfecda99-win64-Setup.exe`,
SHA-256 `50b7e33207d70b4ea13f5ee122c7a1ea7ef7e438cb267c6a301c145177366202`.
Its structured report binds to the exact source and payload manifest; all 23
recorded installer checks pass. Defender reports no detections in this candidate
scan. That does not establish publisher signing, SmartScreen reputation, MSIX
acceptance, actual DAW acceptance or public release approval.

The VST3 harness passed headless, editor-pumping, editor-pending, multiple-instances,
reopen-project (3 rounds), live-remove-controller-first and live-remove-component-first
(3 removal cycles each, 2 peers retained). Its project state is synthetic test state,
not an old public-binary-created Studio One/Cubase/Sonar project.

### Separate failed Windows CAB gate stays open

The unchanged `ChimeraOriginalCabIntegrationTests` failed with
`CAB-only p99 exceeds one block period (runner CPU gate)` in the full Windows run.
EQ, state, navigation and VST3 lifecycle passed in that run. Two failing configurations:

| Sample rate / block / channels | Wall p99 | One-block budget | Maximum | Budget misses |
|---|---:|---:|---:|---:|
| 48 kHz / 256 / stereo | 10496.6 us | 5333.33 us | 79712.9 us | 36/1600 |
| 96 kHz / 64 / mono | 4995.1 us | 666.67 us | 29926.1 us | 49/1600 |

The independent same-source Windows candidate passes the same test. This suggests
runner-sensitive timing, but does not prove scheduling is the sole cause. The matching independent-run wall p99 values were 215.3 us and 144.7 us,
with zero budget misses in these two configurations. All four 1,600-callback CSVs
and source artifact hashes are retained in `cab-timing-comparison/`. Windows
thread CPU time is unavailable in this evidence; valid thread cycle counts must
not be relabeled microseconds. The threshold, implementation and failure are
retained; no retry-until-green or weakened gate is used. Overall full-run success
and real-time acceptance remain blocked pending attribution/resolution.

### Linux installed startup/close boundary

The exact CI deb has SHA-256
`d01509c02da1076d9f14a2b874de8b44f9aacac3d96b6c5e0980ed429ef07ef8`.
The test refuses any pre-existing package or owned paths, verifies installed
manifest source/hash/size, launches the absolute installed ELF and matches the
mapped product window PID. It repairs a deliberately appended owned-binary marker
through reinstall, verifies every installed payload hash again, then removes only
the package it installed. The isolated user-data sentinel survives repair/removal.
Cleanup completed; no Chimera package remained installed.

Two environment-specific probe adjustments are recorded, not product changes:
this container excludes `/usr/share/doc` by default (per-install package-only
`--path-include` restores the expected documentation); child procfs executable
introspection is unavailable (identity uses verified absolute ELF plus window PID).

Mapping is not first-paint readiness. The early probe sent close to the initially
white mapped window and timed out on first launch and restart (15 s each). A
screenshot diagnostic showed the white frame; a subsequent execution closed
normally. The final probe waits for an actual varied painted product frame, not a
fixed sleep, then sends the same WM_DELETE_WINDOW request. First paint took 0.871 s
and 0.862 s after mapping; both normal closes exited 0 within the unchanged 15 s
bound. Both captured frames were inspected and show the actual product UI.
The final complete install/repair/remove sequence passed. The earlier immediate,
pre-paint close behavior is retained as an unresolved startup-edge observation;
it is not claimed fixed or accepted. Force termination used only for failed-probe
cleanup is never counted as a successful shutdown.

No physical audio device is available: the screenshot has the normal muted-input
banner and ALSA reports the missing sequencer device. This probe is installation
and application lifecycle evidence, not audio, DAW, or old-install migration
acceptance. Reports, command logs, screenshots and reproduction scripts are in
`linux-installed-verification*`, `linux-close-diagnostic` and
`linux-install-setup-history`. Run scripts from the repository root with the
recorded deb and `evidence-local/linux-install-source.json` present.

## Outstanding acceptance

Do not merge/publish on the strength of this test correction. Resolve/attribute
the separate full-Windows CAB timing failure, retain the Linux pre-paint-close
observation, and obtain actual supported-DAW tests for automation modes, DAW Undo,
reused automation lanes, old-version session roundtrip and reported host shutdowns.
Physical interface/listening/high-rate sustained operation, macOS real installation, AU host validation/DAW scanning, Developer ID signing/notarization
and Intel-native macOS execution remain unassessed. October 23 is conditional.

## Evidence-only follow-up

The follow-up commit adds this evidence folder and its `-text` Git attribute only.
Its executable, test, build and workflow trees remain identical to the tested
`babfecda995141ef3001fc6f9c04c05409ff7ad6`. `[skip ci]` avoids claiming another
execution for unchanged code. `SHA256SUMS.json` covers every retained evidence
file except itself, including original CRLF logs and the native screenshots.
