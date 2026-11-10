# AAX signing CI — design (not yet implemented)

Date: 2026-10-22 KST; corrected 2026-10-27 KST. Companion to `AAX_RELEASE_CHECKLIST.md`.

**Status: design only.** No signing workflow is added yet. This records how PACE
signing would be automated once the prerequisites exist, so the workflow can be
written and validated against real credentials rather than blind. Building is
already automated (`aax-native.yml`, unsigned development path); this is about
the *signed, retail-Pro-Tools-loadable* path.

## Why this is separate from the build workflow

`aax-native.yml` builds and uploads an **unsigned** AAX on GitHub-hosted runners.
The currently provisioned USB signing route cannot run there:

- PACE publisher/admin setup and one Central Access + one Fusion SDK license
  were issued on October 27, expiring 2027-11-01, for LITE AAX signing only.
  USB arrival, activation, certificate and first signing remain unverified.
- **PACE Central Access requires a second-generation or later USB iLok.**
  Fusion SDK can activate to USB or iLok Cloud, but that does not provision a
  signing certificate or the separate **AAX Cloud Signing** service. No such
  service entitlement is established here. GitHub-hosted runners cannot attach
  the owner's USB. A future separately provisioned cloud route would require
  its own validation; physical-only is not a universal PACE limitation.
- Signing needs **high-value secrets** (PACE account, Apple Developer ID +
  notarization, Windows Authenticode). These must never be exposed to PR/fork
  builds.

So signing is a **separate, gated workflow on self-hosted runners** that have the
iLok attached, triggered only in trusted contexts.

## Proposed architecture

New workflow (e.g. `.github/workflows/aax-sign.yml`), independent of
`aax-native.yml`:

- **Triggers:** `workflow_dispatch` and release-tag `push` only. **Never**
  `pull_request` (secrets must not reach fork PRs).
- **Runners:** self-hosted, one per platform because each binary is signed on
  its own OS:
  - `[self-hosted, windows, ilok-signing]` — Windows x64 AAX.
  - `[self-hosted, macos, ilok-signing]` — Universal macOS AAX.
  Each runner has: the physical iLok inserted and authorized, PACE `wraptool`
  installed and logged in, and the OS code-signing identities installed.
  Only one of each PACE tool license is confirmed. Start sequentially with the
  same authorized USB moved between machines; do not assume two simultaneous
  activations or purchase extra licenses as part of this change.
- **Protected environment:** a GitHub `release-signing` environment with
  required reviewers, so a human approves each signing run; secrets are scoped
  to that environment.

### Per-platform flow

1. Checkout the exact release source (tag).
2. Build the AAX target (reuse the `CHIMERA_BUILD_AAX=ON` config from
   `AAX_NATIVE_DEVELOPMENT.md`; Windows x64 / macOS Universal only).
3. Run the read-only signing-input preflight against the exact source and
   approved beta policy; independently verify the trusted CI archive checksum.
4. Copy the frozen unsigned bundle to a private signing workspace. Confirm
   deployment and OS-signing integration options against the installed PACE
   SDK manual; do not guess credential flags or sign the original in place.
5. PACE-sign and verify on the local signing machine. On macOS verify the
   Developer ID seal as well; notarize/staple the appropriate final installer.
   The exact sequence/options must follow the installed SDK and Apple tooling.
6. Integrate the final bytes into native installers, verify after installation,
   then complete retail Pro Tools acceptance. Never publish merely because
   wraptool verification succeeds. The current development packager cannot
   consume a signed bundle or grant this gate.

## Secrets (in the `release-signing` environment)

- `CHIMERA_WCGUID` — the plug-in's PACE wrap GUID.
- PACE account reference (only if not using the runner's local login).
- macOS: Developer ID Application cert + password; notarization API key
  (`ASC_KEY_ID`, `ASC_ISSUER_ID`, the `.p8`), or Apple ID + app-specific
  password.
- Windows: Authenticode cert (`.pfx`) + password, or the signing provider's
  token.

Never commit any of these; keep them in the environment, not in the repo, and
never echo them in logs.

## What CI can and cannot prove

- **Can:** a valid PACE signature, valid OS code signature, successful macOS
  notarization/staple, and that the bundle exports the AAX entry points.
- **Cannot (stays a manual acceptance step):** that the signed plug-in actually
  loads and runs in **retail** Pro Tools. The `-7054` unsigned error and the
  load/scan/automation/session checks in `AAX_RELEASE_CHECKLIST.md` §6 are done
  by a human on a retail Pro Tools machine; CI signature validity is necessary
  but not sufficient.

## Prerequisites to activate (then the workflow can be written + validated)

- [ ] Physical iLok received and authorized for AAX signing (ordered).
- [x] PACE publisher/admin and two signing-tool licenses issued (October 27).
- [ ] Central Access/Fusion SDK activation and signing certificate verified.
- [ ] Product WCGUID assigned and kept outside the repository.
- [ ] A self-hosted runner per platform with the iLok attached, `wraptool`
      installed/logged in, and OS signing identities installed; registered with
      the `ilok-signing` labels.
- [ ] GitHub `release-signing` environment created with required reviewers and
      the secrets above.

## To confirm with Avid/PACE before implementing

- Exact `wraptool sign` invocation and flags for a **deployment** signature
  (per current PACE docs / the account).
- Whether a stored secret is needed or `wraptool` uses the runner's local
  iLok/PACE session.
- If cloud signing is later chosen, verify separate AAX Cloud Signing service
  provisioning; Fusion SDK Cloud activation alone is insufficient evidence.

## Next step

First validate one manual signing run with the received/activated USB and
current PACE tools. Then provision a trusted self-hosted runner and implement
`aax-sign.yml` against that verified procedure, one platform first. Until then this stays design-only so
nothing inert or unvalidated lands in CI.

## Sources

`AAX_RELEASE_CHECKLIST.md`, `AAX_NATIVE_DEVELOPMENT.md`,
`.github/workflows/aax-native.yml`.

Current procedure/status: [preparation checklist](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md)
and [first-signing runbook](AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md).
Official references: [PACE onboarding](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
and [signing platform](https://paceap.com/code-signing-platform/).
