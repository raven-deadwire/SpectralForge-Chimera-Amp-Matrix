# AAX release checklist — commercial Pro Tools

Date: 2026-10-22 KST; PACE/Avid issuance update: 2026-10-30 KST.

**Requirement (owner):** the next release must ship an AAX plug-in that loads in
**commercial (retail) Pro Tools**, because the beta testers run commercial Pro
Tools — not a Pro Tools Developer build. This document is the step-by-step gate
for that. It states a requirement and a procedure; it does not by itself claim
Pro Tools acceptance, which is proven only by the verification steps below on a
real signed build.

Scope note: today AAX is opt-in / development in
`RELEASE_SCOPE_SNAPSHOT_20261022.md` and `AAX_NATIVE_DEVELOPMENT.md`
("Implementation candidate; no signed AAX release or Pro Tools acceptance"). The
item below is **BLOCKED** until every box is checked on a real build.

## The critical path is signing, not building

The AAX target already builds: JUCE 8.0.8 bundles the AAX SDK, so
`-DCHIMERA_BUILD_AAX=ON` produces `SpectralForge Chimera.aaxplugin` with no extra
SDK download. **But a plug-in built that way is unsigned, and commercial Pro
Tools refuses to load an unsigned AAX.** Loading in retail Pro Tools requires a
**PACE** signature. Publisher/admin setup and tool-license issuance are now
confirmed (October 27); USB activation, certificate/tool setup, actual signing
and retail host acceptance remain separate. Building alone proves none of them.

## 1. Issued access and remaining prerequisites

Official PACE administrator and license-deposit emails received **2026-10-27
08:15 KST** confirm LITE AAX digital-signing-only access, **one PACE Central
Access and one Fusion SDK license**, both expiring **2027-11-01**. No exact
expiry time/timezone is stated. This is unrelated to Chimera's beta expiry.

- [x] PACE publisher/admin setup and the two tool licenses issued.
- [ ] USB iLok (second generation or newer) received and Central Access activated.
- [ ] Fusion SDK activated to USB or supported iLok Cloud. This is **not**
      separate AAX Cloud Signing service entitlement, nor proof of a signing
      certificate. No Cloud Signing service grant is established.
- [ ] PACE Central initial login and current signing tools/manual obtained.
- [ ] Signing certificate and product WCGUID configured/verified privately.
- [ ] Platform signing identities ready: macOS Developer ID Application,
      appropriate installer signing/notarization credentials; Windows
      Authenticode certificate or supported signing service. Confirm bundle
      signing integration in the installed PACE manual, not just installer signing.
- [x] Pro Tools Developer Bundle activation code issued **October 29, 01:45 KST**;
      official Devauth email read October 30. The retry is superseded; no duplicate request sent.
- [ ] Redeem in the Avid developer account and verify the intended iLok account.
      Current browser is blocked at sign-in; no redemption is claimed.
- [ ] Verify actual Pro Tools Intro, Developer Build and Prerelease deposits
      and activation in ILM; receipt of an activation code is not a deposit.
- [ ] Complete the [Windows unsigned Developer-host test](AAX_WINDOWS_DEVELOPER_TEST_20261030.ko.md).
      Developer cannot save sessions; save/reopen remains a signed normal-host gate.

Do not repeat the completed Avid-to-PACE referral. The initial seven-business-day
invitation estimate was never a post-submission review SLA; the derived November
3 reply/chase date is withdrawn. Keep #171's successful application-stage iLok
Validate history; unread mail does not establish account inactivity.

PACE's initial setup: install current License Support/ILM, connect USB, activate
Central Access onto USB, sign out and back into ILM using the same account, then
use **PACE CENTRAL**. Activate Fusion SDK separately; use Central HELP/downloads
for certificate, product and exact command setup. See the
[preparation checklist](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md).

Keep account IDs, personal email, private mail links, serials, WCGUIDs, passwords,
tokens, keys and raw vendor logs outside the repository/CI artifacts. License
issuance does not enable extra Fusion/White-Box protection or introduce end-user
iLok licensing. Detailed PACE Central documentation was not accessible through
the public web lookup; deployment flags remain to be verified on the signing machine.

## 2. Build the AAX target

Supported targets only: **Windows x64 / MSVC** and **Universal macOS**
(Linux, non-x64 and non-Universal are rejected when `CHIMERA_BUILD_AAX=ON`).

Windows (Visual Studio C++ x64):
```powershell
cmake -S . -B build-aax -A x64 -DCHIMERA_BUILD_AAX=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-aax --config Release --target ChimeraAmpMatrix_AAX --parallel 2
```
macOS (Xcode CLT; both slices mandatory):
```sh
cmake -S . -B build-aax -DCHIMERA_BUILD_AAX=ON -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build-aax --config Release --target ChimeraAmpMatrix_AAX --parallel 2
```
Output: `build-aax/ChimeraAmpMatrix_artefacts/Release/AAX/SpectralForge Chimera.aaxplugin`.

- [ ] Windows x64 `.aaxplugin` built.
- [ ] macOS Universal `.aaxplugin` built (verify both arm64 + x86_64 slices).
- [ ] `aax-native.yml` CI green for the AAX build on the release source.

## 3. Freeze and check the signing input

- [ ] Confirm the trusted CI run and archive checksum for the exact candidate SHA.
- [ ] Run `Tools/aax_native.py signing-preflight` with the extracted package,
      target platform, full expected source SHA and `--require-approved-beta`
      from that source checkout. See the [runbook](AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md).
- [ ] Preserve the original unsigned archive and copy only the candidate bundle
      to a private signing workspace. Check the copy's pre-sign hashes.
- [ ] Confirm current PACE deployment and OS-signing options on each signing OS.
      macOS ad-hoc `codesign --sign -` is not Developer ID or PACE signing.

## 4. PACE-sign for commercial Pro Tools (the gate)

Use the installed SDK/HELP documentation for the actual deployment invocation,
account authentication, WCGUID and OS-signing integration. The old speculative
command (including Windows key/password flags and in-place output) is removed;
it was not validated against provisioned tools. No executable signing command
or credential collection is introduced by this update.

- [ ] Final bundle PACE-signed with a deployment signature, not trial/dev-only.
- [ ] PACE verification succeeds on that signed bundle.
- [ ] Platform signature verification succeeds (macOS Developer ID seal included).
- [ ] Record source, pre/post-sign hashes, tool versions and sanitized results.
- [ ] macOS: notarize/staple the appropriate final installer; preserve the
      bundle seal and verify again after installation.

A successful input preflight always leaves PACE verification **NOT_RUN**,
normal-host acceptance **false** and release approval **false**. It checks
consistency only; it is not a provenance attestation or a signature verifier.

## 5. Install-path and packaging

Standard AAX locations (the release installer must place the signed bundle here):
- Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
- macOS: `/Library/Application Support/Avid/Audio/Plug-Ins/`

- [ ] Signed `.aaxplugin` packaged into the Windows installer and the macOS
      installer at the paths above. Signed native installer integration is
      **not implemented**. Keep the existing development packager unsigned-only;
      never flip its receipt flags to pass off a signed release as development.
- [ ] Installer itself OS-signed (Windows Authenticode; macOS signed/notarized).
- [ ] Install/update/remove and post-install PACE/platform signature verification
      pass on the final native package, with the exact final hashes recorded.

## 6. Verification in commercial Pro Tools (acceptance — do not skip)

- [ ] On a machine with **retail** Pro Tools (not a Developer build) and the
      iLok authorization a normal user would have, the signed plug-in loads and
      passes Pro Tools' own plug-in scan (AAX Native).
- [ ] Insert on a mono and a stereo track; audio processes; no scan rejection.
- [ ] Automation of a few parameters records/plays back; session save/reopen
      restores state; A/B and embedded IRs recall.
- [ ] Confirm on both Windows and macOS (Apple Silicon + Intel if claimed).
- [ ] Record the Pro Tools version(s) tested and the tester's environment.

Only when every box in §2–§6 is checked is the "AAX loads in commercial Pro
Tools" requirement met; then update `RELEASE_SCOPE_SNAPSHOT_*.md` to move AAX
from out-of-scope into the release scope with this evidence cited.

## Notes / honest limits

- This repo and CI can build and (in CI) compile-verify the AAX target, but
  **cannot PACE-sign**: signing needs the owner's PACE/iLok account on a signing
  machine. Issued licenses alone do not complete the remaining §1 setup.
- AAX **Native** only (no DSP/HDX/TDM target here).
- The beta expiry and lifecycle policy are reused and source-bound in the AAX
  package exactly as for VST3/AU/Standalone; the AAX build grants no new expiry.

## Sources

`AAX_NATIVE_DEVELOPMENT.md`, `CMakeLists.txt` (`CHIMERA_BUILD_AAX`, AAX identity),
`.github/workflows/aax-native.yml`, `Tools/aax_native.py`,
`RELEASE_SCOPE_SNAPSHOT_20261022.md`.

Official sources checked October 27: [PACE onboarding](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/),
[PACE signing platform](https://paceap.com/code-signing-platform/),
[iLok License Manager](https://help.ilok.com/ilm_overview.html),
[Avid AAX](https://developer.avid.com/aax/). Issuance details were verified from
the two official emails; their private identifiers/content are not stored here.
