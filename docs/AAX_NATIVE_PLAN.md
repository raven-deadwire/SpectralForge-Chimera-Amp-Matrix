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
| October 16, 2026 | Platform/bus/ID plan, access inventory and workload estimate | Ready-to-build assessment; do not infer an account or signing permission exists |
| October 30, 2026 | Internal unsigned Windows/macOS AAX prototype and bundle report | Build proof; host operation requires actual host evidence |
| November 20, 2026 | Pro Tools validation candidate and evidence report | Conditional on a usable Developer host; signed normal-host checks when available |
| Public release date uncommitted | Signed and packaged AAX beta verified in normal Pro Tools | Requires the full delivery stage below |

Initial planning allowance: **10–20 engineering days**, including host and
installer work, excluding unknown external approval waits. This is an estimate,
not measured velocity or an assumption of additional staff. Re-estimate October
16 and 23. Give the immediate chain/EQ quality work priority if shared capacity
conflicts; record any moved AAX target explicitly. The public AAX version will
be assigned to the then-current verified Chimera release, rather than promised
as 1.3.2 regardless of readiness.

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

## Primary references

Checked October 12, 2026:

- [JUCE change list: AAX SDK bundled from 8.0.0](https://github.com/juce-framework/JUCE/blob/master/CHANGE_LIST.md)
- [JUCE AAX development and signing instructions](https://github.com/juce-framework/JUCE/blob/master/README.md#aax-plug-ins)
- [JUCE CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
- [Avid AAX developer entry point](https://developer.avid.com/aax/)
- [PACE AAX signing workflow](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
- [Avid Apple Silicon host/plugin compatibility](https://kb.avid.com/pkb/articles/en_US/Knowledge/Missing-plug-ins-after-upgrading-to-Pro-Tools-2023-3-on-Apple-Silicon-Mac)
