# AAX signing CI — design (not yet implemented)

Date: 2026-10-22 KST. Companion to `AAX_RELEASE_CHECKLIST.md`.

**Status: design only.** No signing workflow is added yet. This records how PACE
signing would be automated once the prerequisites exist, so the workflow can be
written and validated against real credentials rather than blind. Building is
already automated (`aax-native.yml`, unsigned development path); this is about
the *signed, retail-Pro-Tools-loadable* path.

## Why this is separate from the build workflow

`aax-native.yml` builds and uploads an **unsigned** AAX on GitHub-hosted runners.
Signing cannot run there:

- **A physical iLok is required for signing.** We had been using **iLok Cloud**,
  but the signing credential needs a **hardware iLok** (a physical dongle has
  been ordered). GitHub-hosted runners have no USB and cannot mount it.
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
- **Protected environment:** a GitHub `release-signing` environment with
  required reviewers, so a human approves each signing run; secrets are scoped
  to that environment.

### Per-platform flow

1. Checkout the exact release source (tag).
2. Build the AAX target (reuse the `CHIMERA_BUILD_AAX=ON` config from
   `AAX_NATIVE_DEVELOPMENT.md`; Windows x64 / macOS Universal only).
3. **macOS only:** `codesign` the bundle with the **Developer ID Application**
   identity (not ad-hoc) before wrapping.
4. **PACE sign** with `wraptool` using the account + this plug-in's **WCGUID**
   (deployment signature, not trial/dev). On the self-hosted machine `wraptool`
   can use the local iLok/PACE session, so a stored secret may not even be
   needed — confirm against the account setup.
5. **macOS only:** notarize (prefer `notarytool` with an App Store Connect API
   key) and staple.
6. **Verify:** `wraptool verify` + (macOS) `codesign --verify --deep --strict`
   + stapler validate. These prove signature validity.
7. Package the signed bundle into the installer at the standard AAX path and
   upload a **signed** artifact (and/or attach to the release).

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
- [ ] PACE developer signing plan enrolled; WCGUID assigned.
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
- Whether iLok Cloud can ever be used for signing on a hosted runner (reported
  physical-only; hardware iLok is the plan).

## Next step

Once the physical iLok + PACE enrolment + a self-hosted runner exist, implement
`aax-sign.yml` to the above and validate it on a real signing run (one platform
first), then wire it into the release path. Until then this stays design-only so
nothing inert or unvalidated lands in CI.

## Sources

`AAX_RELEASE_CHECKLIST.md`, `AAX_NATIVE_DEVELOPMENT.md`,
`.github/workflows/aax-native.yml`.
