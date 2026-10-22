# AAX release checklist — commercial Pro Tools

Date: 2026-10-22 KST.

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
**PACE** signature. PACE enrollment is an external, account-gated (and paid)
process that must be set up by the owner; it cannot be produced from a build
environment alone.

## 1. One-time prerequisites (owner action — the real lead time)

- [ ] **Avid Developer registration** for RavenForge Luthier Intelligence (AAX
      developer, manufacturer code `SpFg`, plug-in code `Chm1`, identifier
      `audio.spectralforge.chimera` — already set in `CMakeLists.txt`).
- [ ] **PACE Eden / developer account** and a **signing iLok** (USB or iLok
      Cloud) authorized for AAX signing. This is the gating credential.
- [ ] **`wraptool`** (PACE Developer signing tool) installed on the signing
      machine, logged in to the PACE account.
- [ ] Obtain the product's **WCGUID** (wrap GUID) assigned to this plug-in from
      the PACE account; record it in the release notes, not in the repo.
- [ ] Platform code-signing identities: **macOS** Apple Developer ID Application
      certificate (for `codesign` + notarization); **Windows** an Authenticode
      (OV/EV) certificate for the installer. PACE wrapping and OS code-signing
      are separate steps — both are needed.

Keep every credential (iLok, PACE account, certificates, WCGUID) **out of the
repository**; reference them by name only.

### Enrollment path (how to register — start here)

Signing is performed by **PACE**, not Avid; Avid registration is the doorway
that gets PACE to enroll you. The order matters. (Steps below are
community-reported; pricing and procedure change, so confirm current details
directly with Avid and PACE before committing.)

1. **Register as an AAX developer with Avid** and email the Avid developer
   authorization team at **`devauth@avid.com`**. Request a **Pro Tools Developer
   Bundle** activation code too — a Developer build of Pro Tools loads *unsigned*
   plug-ins, so you can test functionality before signing is in place.
2. **Avid notifies PACE**, who enrol you in a developer signing plan. Cost
   reports conflict (some say free for registered Avid developers, others
   describe a paid/enterprise plan on the order of US$500/yr) — **confirm with
   PACE directly.**
3. **Get a physical iLok dongle** and have it authorised for AAX signing. A
   developer reported the signing credential works only on a hardware iLok, not
   iLok Cloud — plan for a USB iLok.
4. **Install PACE `wraptool`** on the signing machine, log in to the PACE
   account, and obtain this plug-in's **WCGUID** from that account.

Lead-time note: steps 1–3 (Avid email → PACE enrolment → hardware iLok) are the
long pole and are owner-only; the build (§2) is already ready. So the first
actions are to send the Avid `devauth@avid.com` registration email and order a
hardware iLok. Verification signal: an unsigned AAX in **retail** Pro Tools
fails to load with error **-7054**; a correctly PACE-signed build clears it.

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

## 3. Code-sign the bundle (macOS) before PACE wrapping

- [ ] macOS: `codesign` the `.aaxplugin` with the Developer ID Application
      identity (the development ad-hoc `codesign --sign -` in
      `AAX_NATIVE_DEVELOPMENT.md` is for local load only, **not** for release).

## 4. PACE-sign for commercial Pro Tools (the gate)

Sign the built bundle with `wraptool` using the PACE account and this plug-in's
WCGUID. Representative shape (confirm exact flags against current PACE docs and
your account):
```sh
wraptool sign --verbose --account <PACE_ACCOUNT> --wcguid <CHIMERA_WCGUID> \
  --signid "<Developer ID Application: ...>" \          # macOS
  --in  "…/SpectralForge Chimera.aaxplugin" \
  --out "…/SpectralForge Chimera.aaxplugin"
# Windows uses --keyfile/--keypassword (Authenticode) instead of --signid.
```
- [ ] AAX bundle PACE-signed (deployment signature, not a trial/dev-only
      signature).
- [ ] `wraptool verify` reports a valid signature on the signed bundle.
- [ ] macOS: notarize the signed bundle / installer and staple.

## 5. Install-path and packaging

Standard AAX locations (the release installer must place the signed bundle here):
- Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
- macOS: `/Library/Application Support/Avid/Audio/Plug-Ins/`

- [ ] Signed `.aaxplugin` packaged into the Windows installer and the macOS
      installer at the paths above (extend `Tools/aax_native.py package` /
      the installer scripts to include the signed AAX).
- [ ] Installer itself OS-signed (Windows Authenticode; macOS notarized).

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
  machine. That enrollment is the long-pole item — start §1 first.
- AAX **Native** only (no DSP/HDX/TDM target here).
- The beta expiry and lifecycle policy are reused and source-bound in the AAX
  package exactly as for VST3/AU/Standalone; the AAX build grants no new expiry.

## Sources

`AAX_NATIVE_DEVELOPMENT.md`, `CMakeLists.txt` (`CHIMERA_BUILD_AAX`, AAX identity),
`.github/workflows/aax-native.yml`, `Tools/aax_native.py`,
`RELEASE_SCOPE_SNAPSHOT_20261022.md`.
