# SpectralForge Chimera — system requirements

Reviewed **2026-10-18 KST** against the published **1.3.2-beta.1** packages.
The next integrated build is still in development. Its CPU/RAM minimums,
maximum instance count and real-time capacity have **not been certified**.
The recommendations below are starting points to validate on your complete DAW session.

## Installation conditions, recommendations and verification

| Item | Installation / compatibility condition | Practical recommendation — not a measured minimum | Confirmed scope / remaining verification |
| --- | --- | --- | --- |
| Windows | Windows 10/11, x86_64 package; installer requires Windows 10.0 or later | Use an OS/driver combination supported by your DAW and interface | Public build/install automation ran on Windows Server 2025, not a Windows 10/11 audio workstation. Native Windows ARM64 is not supplied. |
| macOS | macOS 12+ build target; Universal arm64 + x86_64 app/VST3/AU | Match the OS and native host architecture to the interface and DAW | Both slices and package payload verified; CI ran on macOS 15.7.9 arm64. macOS 12 and Intel hardware audio acceptance not established. |
| Linux | x86_64, Ubuntu 22.04 build baseline; glibc >=2.35 plus runtime dependencies | Debian/Ubuntu environment satisfying **all** package dependencies | Ubuntu 22.04.5 build/dependency and extraction checks completed. Other distributions and physical audio remain unqualified. |
| CPU | 64-bit architecture as above; no certified CPU model, frequency or microarchitecture minimum | Modern CPU with strong single-core performance, **4+ cores** as an entry candidate; **6–8+ cores** for multiple instances | No guaranteed instance count. No explicit AVX-only requirement in product CMake; this does not certify every older x86_64 CPU. |
| RAM | Certified minimum **not established** | **8 GB** entry candidate; **16 GB+** general DAW recommendation; **32 GB** optional headroom for large sample/DAW sessions | No full-session peak/growth measurement establishing these thresholds. A 7 GB CAB-test VM is not an 8 GB product qualification. |
| Display | Editor 1180×780 logical pixels at 100%; 885×585 at 75%; resizable up to 1770×1170 | **1280×800** entry candidate, **1920×1080+** preferred | Allow for OS scaling, host frame and taskbar. 1280×800 at 100% UI may not fit the usable desktop; try 75%. Real 100/125/150% OS-DPI combinations remain to be checked. |
| Storage | See package-specific figures below; install volume and temporary/cache volume both need free space | Planning allowance before download: **1.5 GB Windows/Linux; 4 GB macOS**, plus personal data and dependency downloads | Planning values, not measured installer peak or hard minimum. Existing installs, preserved documents and caches change the peak. |
| Plugin / host | Windows/Linux: **VST3 + Standalone**. macOS: **VST3 + AU + Standalone**. Corresponding 64-bit host required for plugins | Use the host's supported native format; verify scan, automation, save/reopen and shutdown | Package/build checks are available; actual DAW acceptance remains incomplete. No VST2/32-bit package. **AAX is not in this public release.** |
| Audio interface / driver | No external interface required merely to install; live input needs a suitable input device and working OS/host audio driver | Guitar/bass instrument input with suitable gain; manufacturer driver. Windows DAW: ASIO where supported; macOS: Core Audio; Linux: compatible configured audio backend | DAW ASIO use is separate from Standalone. Product CMake does not enable JUCE ASIO; do **not** advertise direct Windows Standalone ASIO. Actual public-binary device enumeration/loopback is NOT_RUN. Mic gain and +48 V are hardware functions. |
| Sample rate | Determined by the host/device; no single universal live-rate qualification | **44.1/48 kHz** for tracking and mixing | Native DSP tests include 44.1/48/96 kHz; individual EQ tests at 192 kHz are not whole-product support/performance evidence. 88.2/96 kHz are optional, higher-load choices. |
| Buffer | Host/device setting; no certified lowest buffer | Tracking **128–256 samples**; mixing **256–512 samples**. Start diagnostics at 48 kHz/256 | 64 samples only after whole-session validation. Buffer period is not driver roundtrip or Transpose algorithmic latency. |
| Amp oversampling | Available choices **1× / 2× / 4× / 8×**; default parameter **4×** | Tracking **2×/4×**; mixing **4×**; 8× only after checking load and benefit | Higher factors, Transpose, multiple CAB paths, Dual/Matrix, analyzers and extra instances affect load. No mode/instance capacity guarantee. |

Linux package dependencies observed in the public DEB:
`libasound2 >=1.0.16`, `libc6 >=2.35`, `libcurl4 >=7.16.2`,
`libfontconfig1 >=2.12.6`, `libfreetype6 >=2.8`, `libgcc-s1 >=4.0`,
`libstdc++6 >=12` (pre-dependency: `libc6 >=2.34`). The TAR has the same runtime
requirements; it is not a self-contained AppImage. Windows binaries use the
static MSVC runtime and do not require a separate VC++ runtime installer.
Windows publisher signing and macOS Developer ID signing/notarization are pending.

## Download size, installed payload and update workspace

All sizes below apply **only to 1.3.2-beta.1**. MB/GB here are decimal
(1 MB = 1,000,000 bytes). Payload figures sum regular-file bytes, not filesystem
allocated blocks, installer overhead or measured disk peaks.

| Public package | Download bytes (MB) | Measured unpacked payload | Installation interpretation |
| --- | ---: | ---: | --- |
| Windows Setup EXE | 210,840,300 (210.84 MB) | Setup not independently unpacked | Same source staging as ZIP; selected components determine installed files. Native installed disk total/peak not measured. |
| Windows portable ZIP | 231,660,067 (231.66 MB) | 255,230,463 (255.23 MB) | Complete archive incl. docs/tools. Standalone executable: 125.82 MB; VST3 executable: 126.13 MB. Setup's uninstaller/metadata add overhead; ZIP is not an exact Setup disk measurement. |
| macOS Universal PKG | 703,016,900 (703.02 MB) | 785,636,516 (785.64 MB) | App + VST3 + AU + docs/helper. PackageInfo independently declares 767,247 KiB; actual installation/receipt/APFS footprint not measured. |
| Linux DEB | 213,495,586 (213.50 MB) | 263,138,580 (263.14 MB) | Installed regular-file payload before filesystem/package-manager overhead and missing OS dependencies. |
| Linux TAR.GZ | 234,235,310 (234.24 MB) | 263,151,065 (263.15 MB) | Its `usr` payload is 263,138,580 bytes. The installer copies it to user locations; keep space for both extracted and installed copies while installing. |

The four ZIP/PKG/DEB/TAR downloads were SHA-256 checked and inspected without
installing them. Setup size/digest comes from the public GitHub asset metadata
and published checksum, not a new native installation. Select one package for
your OS; do not add alternative downloads together as a requirement.

Plan the temporary peak as **download + extracted staging + new installation +
old installation retained during replacement + installer/cache overhead + preserved
documents/backups**. Existing old files already occupy disk; distinguish total
peak occupancy from additional free space needed. Downloads, temp and destination
can be on separate volumes, so check each volume independently.

For a same-version-sized update retaining one old copy, a conservative logical
file model is `D + 3P` before unknown overhead: about **976.53 MB** for Windows
Setup using the ZIP payload proxy, **1,023.69 MB** for Linux TAR, and
**3,059.93 MB** for macOS. These are planning calculations, **not measured maxima**.
The 1.5/4 GB allowances leave some working room but cannot cover arbitrary personal
IRs, sessions, backups, dependency downloads or retained historical copies.
Measure clean install, repair and update peaks on the next actual installer
before publishing a hard free-space minimum. The former blanket **500 MB** reserve
is withdrawn.

## Actual checks and real-time qualification status

The published source is `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`.
Its [product CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39036409533)
completed the following automated inventories. They do not include a certified
30-minute DAW/interface load test.

| Observed environment | Confirmed automated scope | CPU/RAM and audio-device qualification |
| --- | --- | --- |
| Windows Server 2025 10.0.26100, x64 build | 64/64 CTests; public installer report includes install/repair/removal, app launch and VST3 loader checks | CPU model/RAM not established from reviewed product-job environment record; physical interface/DAW NOT_RUN |
| Ubuntu 22.04.5 x86_64 | 58/58 CTests; package extraction/dependency checks | CPU model/RAM not established from reviewed product-job environment record; physical interface/DAW NOT_RUN |
| macOS 15.7.9 (24G830), arm64 runner | 57/57 CTests; universal bundle slices/code-seal and expanded package verification | CPU model/RAM not established from reviewed product-job environment record; Intel execution and physical interface/DAW NOT_RUN |

Historical performance evidence is useful for choosing tests, but is tied to
its own source and workload:

- [CAB comparison #76](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/76):
  Apple M1 **Virtual**, 3 cores/7 GB, native arm64; 96 kHz, 32/64 samples,
  three CABs/six microphones, mono/stereo. 96 trials/1,843,200 callbacks.
  Both compared sources passed 66 CAB deadline rows, but individual callback
  misses remain. This excludes the amp/FX/full processor and DAW driver.
- [UI comparison](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/b7923ea8b7a0f85cfbfafb0e0338238adc09b181/docs/ISSUE8_UI_REFRESH.md):
  AMD EPYC 9V74 host model (allocated cores/RAM unrecorded), Ubuntu 24.04/GCC 13/JUCE 8.0.8, Xvfb 2560×1600, short synthetic Matrix runs
  at 48 kHz/128 with filter-only CAB. Candidate closed-editor p99 1533.2 µs
  with 7/4500 compute deadline misses. Two instances were measured **idle**
  (3.03% message-thread CPU, one logical core = 100%), not two live audio instances.
- [Latest CAB research #96](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/96)
  remains CAB-only and did not demonstrate the proposed prefetch improvement.
  [Transpose research #99](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/99)
  and [#100](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/100)
  are unmerged experiments, not performance improvements in the public product.
- Callback allocation checks and correct state restoration do not measure whole
  process RAM consumption, peak RSS/working set or 30-minute memory growth.
  No reviewed evidence establishes a certified RAM floor or maximum instance count.

| Mode | 1 active instance | 2 active instances | 4 active instances |
| --- | --- | --- | --- |
| Classic | BLOCKED | BLOCKED | BLOCKED |
| Dual (Blend and Crossover recorded separately) | BLOCKED | BLOCKED | BLOCKED |
| Matrix | BLOCKED | BLOCKED | BLOCKED |

This matrix means **next integrated Release build, 30-minute actual DAW/interface
qualification**. The required workstations, integrated binary and callback adapter are unavailable in this execution session. It does not erase historical synthetic/CI checks. No unmeasured
cell is PASS. Start with 48 kHz, 128/256 samples and 4×; record Transpose bypass/−2
and editor/analyzer closed/open. Preserve each exercised condition separately.
Record CPU model/core allocation, RAM, OS/build, DAW/version/format, interface and
driver, power mode, preset/IR hash, routing/CAB count, duration, glitches/xruns,
callback wall p99/peak/deadline misses, thread CPU where available, host process
RAM start/peak/end and session reopen. Report unavailable counters as unavailable.
Use host meter, message-thread CPU and audio callback time as separate metrics.
Keep the existing acceptance gates; the proposed 30-minute observation is not
a new automatic release approval or a claim of zero failures for all workloads.

At 48 kHz, one 128/256-sample block lasts 2.67/5.33 ms. These periods do not
include driver/converter/roundtrip delay. Public Transpose still adds approximately
43–46 ms algorithmic latency at 48/44.1 kHz; the ≤10 ms target is not achieved.

## Measurement framework and current environment record

The [source/binary-bound measurement framework](../Tools/SystemPerformance/README.md)
now defines 36 baseline comparisons, 12 Dual Crossover checks, 12 focused GUI
checks and two independent representative 30-minute playback/recording checks.
It records all required hardware/OS/DAW/format/interface/driver/power metadata,
raw callback wall/thread CPU times, p99/peak, compute and scheduled deadline
misses separately, and sampled process RSS start/peak/end/growth. Unknown
counters stay null. Raw logs and identities must revalidate before a result is
accepted as a measurement; no result automatically certifies requirements.

The current [environment and execution record](evidence/system-performance-20261018/README_KO.md)
contains observed Linux executor metadata and blocked workstation/format slots.
**No new Chimera CPU/RAM measurements or 30-minute DAW/device results were obtained.**
Collector contract tests, including a native C++ clock/overflow test, are tool
verification only. Physical hardware, the next integrated Release module, an
actual host callback adapter and the DAW/interface are still required. The
recorder header is not installed instrumentation in the published plugin.

Use NOT_RUN for a planned condition not yet attempted, BLOCKED for a named
missing prerequisite, MEASURED for an auditable population and FAILED for a
failed collection. A MEASURED row may contain misses or failed stability. Keep
the public OS/format build/package verification above separate from this
prospective performance table. The earlier 4-core/8 GB and 6–8-core/16 GB figures
remain unmeasured planning candidates; this framework does not promote them.

## AAX / Pro Tools — separate, not yet supported

There is no AAX target in the reviewed product CMake and no public `.aaxplugin`
package. Windows x64 and macOS Intel/Apple Silicon **AAX Native** are next-build
goals, not released support. No Pro Tools version is qualified for Chimera yet.

Finalize this table only with the actual signed package and real host results:
Chimera version/hash, AAX SDK/build architecture, PACE signature, platform signing,
Pro Tools edition/version, compatible OS/CPU/RAM, driver/interface, mono/stereo,
automation/PDC, presets/IR restore, session reopen and shutdown, beta expiry.
Check the requirements for that exact Pro Tools version against
[Avid's official system requirements](https://kb.avid.com/pkb/articles/en_US/Knowledge/Pro-Tools-System-Requirements)
at qualification time; do not copy Chimera's macOS 12/Windows 10 build floor into
an AAX support promise. The [AAX developer requirements](https://developer.avid.com/aax/)
and signing process are separate from end-user plugin licensing. No new Chimera
iLok activation requirement is declared here.

## Document status

Specification draft and published-package audit: **complete**. Representative
hardware/DAW measurement: **pending**. CPU/RAM certification: **not established**.
The source README, installation guide, EN/DE/KO manual and release-note source
use these distinctions. Public beta.1 binaries/assets are unchanged; the next
release must refresh its version, sizes and evidence before publication.

Maintainer evidence, commands and unexecuted qualification records:
[audit record](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/docs/system-requirements-20261018/docs/evidence/system-requirements-20261018).
Related [integration issue #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97).

