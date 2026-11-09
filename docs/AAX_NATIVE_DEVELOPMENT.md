# AAX Native development foundation

Foundation: 2026-10-18 KST; access/Developer-host update: 2026-10-30 KST. Related: [P0 issue #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97).
**Implementation candidate; no signed AAX release or Pro Tools acceptance.**

The October 18 P0 supersedes the old October/November scheduling assumptions in
the [October 12 plan](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/docs/remaining-updates-eq-aax-20261012/docs/AAX_NATIVE_PLAN.md).
This change starts from main `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`, containing
the published 1.3.2-beta.1 product. It does not integrate #98 or Transpose/EQ
research, change the public version, or attach new binaries to an old release.

## Build and identity

JUCE is still **8.0.8**, upstream commit
`d6181bde38d858c283c3b7bf699ce6340c050b5d`. Its bundled
`modules/juce_audio_plugin_client/AAX/SDK/Interfaces/AAX_Version.h` declares
`AAX_SDK_VERSION=0x0208`, revision `20208000`. JUCE's `_juce_init_bundled_aax_sdk`
connects that source automatically. There is no additional SDK download or
PACE credential in the compilation path. Native toolchain compatibility must
be established by the actual CI, not inferred from the SDK's presence.

```powershell
# Windows, Visual Studio C++ x64 toolchain
cmake -S . -B build-aax -A x64 -DCHIMERA_BUILD_AAX=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-aax --config Release --target ChimeraAmpMatrix_AAX --parallel 2
python Tools/aax_native.py package --build build-aax --output dist-aax --platform windows
```

```sh
# macOS, Xcode command-line tools; both slices are mandatory
cmake -S . -B build-aax -DCHIMERA_BUILD_AAX=ON -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build-aax --config Release --target ChimeraAmpMatrix_AAX --parallel 2
codesign --force --sign - 'build-aax/ChimeraAmpMatrix_artefacts/Release/AAX/SpectralForge Chimera.aaxplugin'
python3 Tools/aax_native.py package --build build-aax --output dist-aax --platform macos
```

Default `CHIMERA_BUILD_AAX=OFF` retains VST3/Standalone and macOS AU. Linux,
Windows non-x64/non-MSVC and non-Universal macOS are explicitly rejected when
the option is enabled. `COPY_PLUGIN_AFTER_BUILD` stays off. No build installs
into a live host automatically. CI may request the existing qualification-only
beta policy; this never grants a new expiry approval. The common version's
approved expiry and policy are reused, source-bound and included in the package.

| Identity | Contract |
|---|---|
| Manufacturer / product | `SpFg` / `Chm1`, unchanged |
| Bundle/effect identifier | `audio.spectralforge.chimera` |
| Native mono in → mono out | `ChM1` / `0x43684d31` |
| Native stereo in → stereo out | `ChS1` / `0x43685331` |
| Reserved, unadvertised AudioSuite IDs | `ChMA`, `ChSA` |
| Category | AAX Effect |
| Host multi-mono / AudioSuite / HDX DSP | Not exposed / not exposed / not implemented |

Internal Classic/Dual/Matrix and Dual Mono do not create new host buses or IDs.
The existing equal mono/stereo bus predicate is unchanged. No asymmetric,
sidechain or surround configuration is introduced.

The AAX wrapper links the same `ChimeraAmpMatrix` shared code: APVTS IDs,
ordinals, automation, state chunks, presets, personal/embedded IR handling,
beta lifecycle, editor and DSP are not forked. JUCE AAX's nonlegacy parameter
mapping uses the existing ID strings; VST3's numeric mapping and AU mapping
remain unchanged. JUCE additionally exposes its `MasterBypassID`; it is not
inserted into Chimera's APVTS. The focused test detects collisions in JUCE's
32-bit AAX parameter hash across the complete real parameter bank.

JUCE forwards `getStateInformation` / `setStateInformation` through AAX chunks
and calls `Controller()->SetSignalLatency(processor->getLatencySamples())`
on prepare and latency changes. Shared dynamic latency is tested; real Pro
Tools PDC, automation gestures and session restore remain separate host checks.
Common preset compatibility does not promise automatic VST3/AU instance
replacement in a Pro Tools session.

### Master bypass latency correction

The October 18 follow-up found a concrete PDC gap in the shared processor:
JUCE 8.0.8's AAX wrapper calls `processBlockBypassed` for `MasterBypassID`, but
Chimera inherited the default immediate dry passthrough while reporting nonzero
latency. JUCE's `AudioProcessor::processBypassed` explicitly warns about this
case. A host applying the reported compensation can move bypassed audio early.

`HostBypass` now retains raw input in a ring allocated in `prepareToPlay` and
returns exactly the current reported number of delayed samples when the host
calls bypass. Normal processing primes that ring before input gain/effects, so
entering bypass preserves input history. The capacity covers the maximum
prepared PRE/board/pitch + engine + POST latency, including variable/oversized
callbacks. No parameter, preset field, version, beta decision or wet DSP is
changed. The same callback is available to VST3/AU wrappers. Master bypass
continues to return dry audio, including on an expired instance, as JUCE did
previously; it does not re-admit expired **processed** audio. DSP parameter edits
while host-bypassed take effect on the next wet callback or reprepare; until then
the dry path follows the still-reported delay. Transition smoothing and actual
host compensation timing are not established by this correction.

The AAX contract test now checks mono/stereo at 44.1/48/96 kHz, PRE/board paths,
the actual DSP pitch latency delta, zero semitones, disabled pitch, saved state
reopen before the first audio callback, and reprepare. It compares bypass output
sample-for-sample with delayed raw input across wet/bypass transitions and
64/128/257-sample callbacks. A standalone bounded-ring regression also covers
zero/dynamic delay, maximum delay, ring wrap and 5,003-sample callbacks. These are
processor tests, **not Pro Tools PDC acceptance**. Existing full parameter and
personal/embedded IR + beta lifecycle tests remain in the AAX job.

The AAX workflow also watches version/channel, all CMake inputs, beta policy and
generation tools, fixtures/tests, embedded assets and the embedded manual so
future common-code/policy integration cannot silently skip its build.

## Development package and installation lifecycle

`Tools/aax_native.py` checks the Windows `Contents/x64/*.aaxplugin` PE32+ DLL
or macOS `Contents/MacOS/SpectralForge Chimera` Universal Mach-O and plist.
It binds SHA-256 payloads to the configured source/version/beta metadata and
produces an `AAX-UNSIGNED-DEVELOPMENT.zip` plus SHA-256. It has no signed mode.
The existing public VST3/AU publisher is not changed to ship AAX implicitly.

The extracted archive includes a standard-library Python 3.9+ development
installer. Close all hosts, then run its README commands in an administrator
terminal (Windows) or with `sudo python3` (macOS).

| Platform | Exact bundle destination |
|---|---|
| Windows x64 | `%CommonProgramW6432%\Avid\Audio\Plug-Ins\SpectralForge Chimera.aaxplugin` |
| macOS Universal | `/Library/Application Support/Avid/Audio/Plug-Ins/SpectralForge Chimera.aaxplugin` |

The installer owns only this bundle and the adjacent
`SpectralForge-Chimera-AAX-development.json` receipt. Update verifies all old
bytes, stages on the same volume, replaces the bundle and rolls back on failure.
Removal verifies ownership/hashes first. Unowned bundles, changed/extra files,
links and Windows reparse points are refused rather than deleted. User IRs,
presets, sessions and other formats are never enumerated for removal. Replacing
this development installation with a future signed installer requires a
deliberate migration; no existing signed package is overwritten by this tool.

This is a usable **developer installation path**, not a signed Windows Setup or
notarized macOS installer. Python is a developer requirement only. Final native
installer integration must consume the final PACE-signed bundle, preserve its
bytes, verify its signature after extraction and pass the host gate below.

## Access inventory and external dependencies

The following inventory retains October 18 technical evidence with PACE access
updated from the October 27 official emails and Avid code issuance from the
October 29 01:45 KST Devauth email, read on October 30. Absence of
evidence does not mean the owner has no account elsewhere. No account was
created, agreement accepted, approval requested or email sent by this change.

| Dependency | Actual observed status | Blocks |
|---|---|---|
| GitHub repository read/write + Actions | Connected repository access; Draft PR/CI available | Nothing in source implementation |
| JUCE 8.0.8 bundled SDK | Retrieved and inspected at the pinned upstream SHA | Native build execution still required |
| Local supported OS/toolchain | Linux x86_64; no local Windows/macOS SDK or Pro Tools | Local AAX compile/host test; use Actions for compile |
| Avid developer / PACE referral | Referral reached PACE and publisher setup is confirmed; do not reopen this completed referral | Pro Tools host licensing is separate from signing access |
| Pro Tools Developer + iLok authorization | Developer Bundle activation code issued October 29 01:45 KST; October 29 retry superseded. User completed Avid sign-in and authorized EULA acceptance; after acceptance, Avid rejected the code as invalid. The submitted value exactly matches the issuance email | Redemption BLOCKED_INVALID_CODE; confirm code status with Avid Devauth before retrying. Correct iLok destination, actual Intro/Developer Build/Prerelease deposits and activation remain unverified |
| PACE AAX signing access | Publisher/admin created; Central Access and Fusion SDK licenses deposited October 27, one each, expiry 2027-11-01; LITE signing only | USB arrival/activation, certificate, wraptool installation and first signing unverified; no separate AAX Cloud Signing entitlement established |
| Windows publisher certificate / Apple Developer ID + notarization | Not established by this task; repository workflow alone is not access evidence | Final platform installer qualification |
| Normal Pro Tools on Windows, macOS Intel and Apple Silicon | No actual host sessions available | Signed release acceptance on those paths |

Historical enrollment route (do not repeat the completed Avid-to-PACE referral):

1. Register through [Avid AAX](https://developer.avid.com/aax/) and complete the
   applicable developer agreement. Avid directs commercialization/tool/license
   requests to `audiosdk@avid.com`; it separately requires iLok authorization
   for the testing host.
2. [JUCE 8.0.8 instructions](https://github.com/juce-framework/JUCE/blob/8.0.8/README.md#aax-plug-ins)
   direct Pro Tools Developer activation requests to `devauth@avid.com`, followed
   by downloading the Developer build through the Avid developer account.
3. Validate the unsigned plugin in that Developer host. Prepare a short running
   plugin/audio demonstration and product description for the signing request.
4. [PACE's current procedure](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
   starts with Avid publisher verification, then Avid forwards approval to PACE.
   Prepare company/admin/contact/website/plugin and Avid-developer-email details.
   PACE lists an iLok USB certificate route and separately a cloud-signing
   service. Provisioned tooling determines the concrete signing command; this
   PR does not invent a working wraptool command, certificate, cost or lead time.
5. Sign the final binary, verify it after packaging, test the signed installed
   package in ordinary Pro Tools, then apply the normal release approval gates.
   Ad-hoc macOS sealing, Authenticode and notarization are not substitutes for
   PACE AAX signing. Signing does not require adding end-user iLok licensing.

## Required host evidence before release

Record exact source, archive/binary hashes, signing verification, OS/CPU,
Pro Tools edition/version, interface/driver, rate/buffer, preset and route.
Use actual Developer and normal signed-host columns, never one combined PASS.

Follow the [Windows Developer test runbook](AAX_WINDOWS_DEVELOPER_TEST_20261030.ko.md)
for the selected unsigned CI artifact and first smoke test. Avid's issuance
email requires launching regular Pro Tools before Pro Tools Developer and
following that Developer download's ReadMe. Developer supports unsigned AAX
and debugging, but **session saving is disabled**. Save/reopen acceptance belongs
to signed AAX in normal Pro Tools. Bounce/export capability must be checked in
the specific Developer build; unsupported operations stay NOT_SUPPORTED, never PASS.

- Scan, load and remove mono/stereo inserts; Classic/Dual/Matrix and internal
  Dual Mono; sample-rate/buffer changes and bypass; realtime and offline bounce.
- Parameter gestures and automation read/write; session save/reopen; presets,
  embedded/personal/missing IR recovery; cross-format preset semantic comparison.
- Reported latency and measured compensation after Transpose/quality changes;
  multiple instances and real DI rendering, not just silence or unit tests.
- Repeated editor open/close, instance removal, session/app exit (including #3).
- Expiry before/boundary/after, active-instance grace, newly muted instance,
  state recovery and update; no destructive effect on session/preset/IR data.
- Install/update/repair/uninstall of the **signed final native installer** and
  post-extraction PACE/platform signatures; Intel and Apple Silicon host paths.

`AAX Native Development` CI compiles real bundles, checks exports/architectures,
runs shared regression tests, extracts the development ZIP, and exercises its
installation lifecycle in isolated and disposable standard system paths.
These checks cannot establish any Pro Tools host result. Record statuses as
**BUILT → DEVELOPER-HOST VERIFIED → SIGNED NORMAL-HOST ACCEPTED → DELIVERED**.

## Signing-input preflight (October 27)

From the exact source checkout, run `Tools/aax_native.py signing-preflight` on
an extracted, trusted unsigned package with `--package`, `--platform` and
`--expected-source` (full SHA). Add `--require-approved-beta` for deployment
input; qualification-only policy is then rejected. See the
[first-signing runbook](AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md).

This read-only command checks package hashes/architecture, receipt/build/source
identity and beta metadata/policy. It never contacts PACE, reads credentials,
installs or signs; output always leaves PACE/platform verification NOT_RUN and
host/release acceptance false. It proves consistency, not authentic build
provenance; independently verify the source-bound CI run and archive checksum.
The native ZIP lifecycle tests also emit `signing-preflight.json` for CI.
