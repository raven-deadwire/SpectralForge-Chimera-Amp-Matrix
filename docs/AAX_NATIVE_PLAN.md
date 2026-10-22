# AAX Native development and delivery plan

Updated: **2026-10-12 KST**. Status: **PLANNED; AAX target not yet implemented**.
The owner requested a Pro Tools version alongside the remaining Chimera
updates. Version dates and checkpoints are owned by
[the development roadmap](DEVELOPMENT_ROADMAP_2026_2027.md).

## Verified baseline and initial scope

At [main 3817246f](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/3817246f779a50f19b5378ec9661a2275e6b91b2),
[CMakeLists.txt](../CMakeLists.txt) pins JUCE **8.0.8** and builds VST3 plus
Standalone, adding AU on macOS. There is no AAX target in that source.

First delivery scope is **AAX Native on Windows x64 and macOS Universal
(x86_64 + arm64)**, reusing the existing audio engine, editor and state.
Test the actual Apple Silicon host path as well as the Intel path.
AAX DSP acceleration for HDX/Carbon is outside this initial Native scope.
AudioSuite exposure must be decided and tested explicitly; an ordinary
offline-bounce check does not establish AudioSuite support.

JUCE's official change list records a bundled AAX SDK from version 8.0.0.
The pinned 8.0.8 therefore provides a starting point for the Native wrapper.
A separate SDK download or major JUCE upgrade is not assumed as a prerequisite
for the internal build. Verify the actual pinned SDK/toolchain compatibility
when implementing; developer-host and signing access remain separate.

## Calendar and work budget

| Target, KST | Result | Meaning |
|---|---|---|
| October 16, 2026 | Platform/bus/ID plan, actual access inventory and workload review | Ready-to-build assessment; record SDK, host and signing prerequisites separately; do not infer access |
| October 30, 2026 | Conditional internal unsigned Windows/macOS AAX prototype and bundle review | Requires the applicable toolchain and allocated work; build proof does not establish host operation or a public beta |
| November 20, 2026 | Conditional Pro Tools validation candidate and evidence review | Requires an actual usable Developer host and built candidate; signed normal-host checks require actual signing access |
| Public release date uncommitted | Signed and packaged AAX beta verified in normal Pro Tools | Requires the full delivery stage below |

Initial planning allowance: **10–20 engineering days**, including host and
installer work, excluding unknown external approval waits. This remains an
estimate, not measured velocity or additional staff. Re-estimate October 16
and 23 against actual access and weekly progress. It is not a second full-time
implementation running alongside chain/EQ, shared FX or a focused product.
Give the immediate chain/EQ quality work priority if shared capacity conflicts;
record any moved AAX target and its reason explicitly. The public AAX version
will be assigned to the then-current verified Chimera release, rather than
promised as 1.3.2 regardless of readiness.

The October 30 and November 20 entries are conditional evidence checkpoints,
not automatic passes or guaranteed releases. If a prerequisite is unavailable,
record the affected AAX stage as blocked, keep any achieved build evidence, and
continue independent product changes. An account request, an unsigned artifact
or an internal demo cannot substitute for the final signed-host acceptance.

### Weekly implementation and maintenance

Follow the central roadmap's weekly delivery rule: one principal implementation
plus bounded maintenance, with at least one actual verified change/build each
week. AAX contributes a concrete wrapper, layout, state, packaging, signing or
host fix when its prerequisites permit; planning text and an access status
alone are not an implemented update. Publish source identity, changed behavior,
validation result and the actual available package for each completed increment.

If AAX access is blocked, that does not block the weekly Chimera/native-VST3
maintenance build. Do not issue an unusable unsigned public AAX package merely
to meet a weekly cadence. Public AAX still requires Stage 3 and the applicable
release checks. Allocate substantial AAX implementation as the principal work
for an explicit week or revise its checkpoint, rather than counting that work
again inside another product's implementation window.

## Scope of the new focused products

The October 12 follow-up adds separately installable Náströnd and Niflheimr
VST3 products; see [their product/NAM specification](FOCUSED_AMP_PRODUCTS_AND_NAM.md).
This document's dated milestones and initial 10–20 engineering-day allowance
cover **Chimera's first AAX Native implementation**, not three accepted products.

Design the build/signing integration for reuse by the focused products. Each
still needs its own stable AAX product/layout IDs, bundle/installer ownership,
parameter mapping, source-bound signature and normal-Pro-Tools acceptance.
Do not reuse Chimera's plugin identity or infer acceptance from a sibling's
signed binary. Add the incremental platform, host and packaging work to the
October 16/23 capacity review and subsequent weekly reviews.

The central calendar now allocates November 30–December 25, 2026 to the first
shared original FX/reuse integration, December 28, 2026–January 22, 2027 to
the first Náströnd native VST3 beta, and January 25–February 19, 2027 to the
first Niflheimr native VST3 beta. These are three separate four-week principal
work windows, not an allowance for three additional AAX implementations. Reuse
AAX build/signing work where possible, then explicitly allocate the remaining
product-specific host and package validation when each native product is ready.

Focused-product AAX is a follow-on format track with no committed public date.
It must not block a separately qualified VST3 release, and it does not make
NAM files or a future NAM runtime part of the AAX implementation scope.

## Stage 1: compile and integrate Native

- Add an opt-in Windows/macOS AAX format through the existing JUCE CMake
  configuration and common DSP/UI sources. Keep supported existing formats
  and existing public product/version identity stable.
- Freeze the AAX identifier, manufacturer/product codes and supported host
  input/output layouts. Give each required AAX layout a stable distinct ID.
  Classic/Dual/Matrix internal routing does not itself create three host buses.
- Preserve all released parameter IDs, ordinal contracts and preset-state
  meaning. Record any wrapper-specific mapping explicitly.
- Inspect actual Windows and macOS .aaxplugin contents and architectures.
  Bundle construction, installation paths and uninstall ownership need
  explicit implementation; adding a format token is not a delivered package.
- Preserve old projects, user presets and personal IRs through install/update/
  uninstall behavior. An AAX build does not promise automatic replacement of
  an existing VST3/AU instance inside a different host project.

## Stage 2: developer-host validation

The current public JUCE instructions describe an AAX Developer account,
a Pro Tools Developer Bundle activation request and a separate Pro Tools
Developer application for testing unsigned plugins. Record which of these
is available before claiming runtime validation.

Use the actual build source and platform package to check:

| Area | Evidence |
|---|---|
| Layout/routing | Supported mono/stereo configurations and all Classic/Dual/Matrix modes |
| Audio | Playback, bypass, sample-rate/buffer changes and reference-matched output |
| Automation/state | Parameter gestures, write/read automation, save/reopen, presets and A/B |
| Latency | Reported and measured delay, host compensation, active Transpose and quality changes |
| Host lifecycle | Repeated UI open/close, plugin removal, session close and application exit |
| Offline/multiple instances | Actual offline bounce and simultaneous instances without shared-state faults |

Include the existing [issue #3](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/3)
lifecycle scenarios. Do not mark them resolved from standalone or wrapper-unit
tests. Preserve a short running-plugin demonstration with audio to support the
later developer/signing request where the provider requires it.

## Stage 3: sign, validate normal Pro Tools and package

Ordinary commercially distributed Pro Tools requires valid PACE AAX signing
to load a plugin. Free distribution does not remove that requirement.
An unsigned internal prototype must not be described as a usable public AAX beta.

The official PACE route starts with Avid publisher verification, followed by
PACE signing-tool provisioning. The current guidance lists publisher/contact/
website/plugin information and the Avid Developer email. Check the current
requirements when preparing the concrete request. Developer correspondence,
account agreements and provider credentials are separate actions from writing
this plan.

PACE documents a USB iLok signing-certificate path and a cloud-signing service.
Choose the route after actual access, cost and CI requirements are known.
No approval lead time, service cost or existing access is assumed here.
Developer code signing is separate from introducing end-user iLok licensing.

Public package sequence:

1. Build the exact candidate and record platform/source identity.
2. Sign the final AAX binaries using the authorized route.
3. Verify signature integrity after packaging.
4. Install and test that signed package in normal Pro Tools on the declared
   Windows/macOS/CPU coverage.
5. Verify update/uninstall behavior and publish accurate format/platform
   support, hashes and installation guidance.

Preserve the existing Windows signing and macOS notarization work as
platform-specific packaging requirements; neither replaces PACE AAX signing.
AAX publication must not inherit an unsigned-beta assumption from VST3/AU.

## Completion and unresolved dependencies

Record three distinct statuses: **BUILT**, **DEVELOPER-HOST VERIFIED** and
**SIGNED NORMAL-HOST ACCEPTED / DELIVERED**. Publish an AAX beta only after the
last stage and existing applicable release requirements are met.

As of this planning revision, Avid/PACE access, Pro Tools Developer/normal host
availability, signing route/cost and actual Native build results are **unknown
or not executed**. Continue independent source work while those prerequisites
are resolved; do not fabricate their completion or an external approval date.

## Public artifact version and conditional deployment reviews

AAX uses the same product version as that release's other formats; it does not
have a separate AAX version counter. Freeze and sign the matching final source,
then publish its AAX package with that new product release. Do not append a
changed-source binary to an already published older version.

The October 18 owner decision in **issue #97 supersedes the former November 27 / 1.3.2-beta.6 first-publication review**: AAX Native is a mandatory part of the next Chimera update, planned as **1.3.2-beta.2**. October 23 is its development and conditional publication-review target. It does not certify Avid/PACE access, signed packages or normal Pro Tools acceptance. If those prerequisites or any other P0 remain incomplete, the next update remains blocked and its unused reservation must be explicitly reviewed. Do not silently ship other formats without the required AAX, reuse the public beta.1 tag or inherit beta.1 expiry/publication approval. Historical engineering estimates remain estimates, not a later exemption from the next-update requirement.

Focused-product AAX review reservations are **March 26, 2027** for Náströnd
and **April 9, 2027** for Niflheimr, after their native stable-review targets.
Use the then-current verified product version and separately estimate the
incremental work. These reviews must fit the existing shared maintenance/
verification budget or explicitly move affected work; they are not included
as additional guaranteed releases in the native four-week allocations.
See [the release calendar](RELEASE_CALENDAR_2026_2027.md).

## Primary references

Checked October 12, 2026:

- [JUCE change list: AAX SDK bundled from 8.0.0](https://github.com/juce-framework/JUCE/blob/master/CHANGE_LIST.md)
- [JUCE AAX development and signing instructions](https://github.com/juce-framework/JUCE/blob/master/README.md#aax-plug-ins)
- [JUCE CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
- [Avid AAX developer entry point](https://developer.avid.com/aax/)
- [PACE AAX signing workflow](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
- [Avid Apple Silicon host/plugin compatibility](https://kb.avid.com/pkb/articles/en_US/Knowledge/Missing-plug-ins-after-upgrading-to-Pro-Tools-2023-3-on-Apple-Silicon-Mac)
