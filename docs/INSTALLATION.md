# SpectralForge Chimera — Open Beta 1.3.2 installation

Current public baseline: product `1.3.2`, package `1.3.2-beta.1` (2026-10-18 KST).
Approved expiry for this version only: **2027-02-11T14:59:00Z / 2027-02-11 23:59:00 KST**.
Read the matching `BETA_LIFECYCLE.txt` before installation; SETTINGS shows the
installed build identity and expiry. The countdown improvement on this development
branch does not approve or publish a new version or carry this date forward.
At/after expiry new instances are muted; active instances keep processing until
closed. Projects, presets, automation and personal/embedded IR state remain
loadable/saveable. Close the DAW to update, then reopen the preserved project.
Preparation builds are test artifacts; public publication requires the separate release verdict.
Close the application and all DAWs before installing or updating. Save a copy of important sessions first.
Compare the downloaded installer against its adjacent `.sha256.txt` file or `SHA256SUMS.txt` from the same package set.
The beta packages currently have no verified publisher code signature. The macOS binaries use an ad-hoc build signature for local code integrity; they are not Developer ID signed or notarized. No Windows Store certification is claimed.

## System requirements and recommended configuration

The next integrated build has no completed representative 30-minute DAW/interface measurements (**BLOCKED**). CPU/RAM recommendations remain unmeasured candidates. The [environment/status record](evidence/system-performance-20261018/README_KO.md) keeps these gaps separate from published build/package checks.

Public baseline: **1.3.2-beta.1**. Installation/build compatibility is distinct
from actual workstation qualification. CPU/RAM numbers are **recommendation
candidates**, not certified minima. See [the complete requirements and measured
package sizes](SYSTEM_REQUIREMENTS.md) for evidence and pending tests.

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


### Real-time starting points and measured scope

- Tracking: **44.1/48 kHz · 128–256 samples · 2×/4×** amp oversampling.
- Mixing: **44.1/48 kHz · 256–512 samples · 4×**. Higher rates, 8×,
  Transpose, multiple CABs, Dual/Matrix and additional instances increase workload.
- Start troubleshooting with one instance at **48 kHz / 256 samples / 4×**.
  64 samples is an experiment after whole-session stability, not a minimum spec.
- Current Transpose algorithmic latency remains approximately **43–46 ms**.
  A smaller device buffer does not remove it or guarantee low roundtrip latency.
- Classic/Dual/Matrix × **1/2/4 active instances**: next integrated build's
  30-minute DAW/interface qualification is **BLOCKED** by unavailable workstations, the integrated binary and callback adapter. Historical CAB-only,
  synthetic Matrix and two-idle-editor results do not qualify these cells.

### Download and installation workspace

The following **1.3.2-beta.1** downloads are alternatives per OS (decimal MB):
Windows Setup **210.84 MB** / ZIP **231.66 MB**; macOS PKG **703.02 MB**;
Linux DEB **213.50 MB** / TAR.GZ **234.24 MB**.
Measured regular-file payloads: Windows ZIP **255.23 MB**; macOS **785.64 MB**;
Linux DEB installed layout **263.14 MB**, TAR extracted **263.15 MB**.
These are not allocated disk totals or native installer peak measurements.

Budget download + extracted staging + new/retained old installation + temporary
cache + preserved documents. As a **planning allowance**, reserve **1.5 GB on
Windows/Linux or 4 GB on macOS before downloading**, plus user IRs, sessions,
backups and missing dependencies. Check both temp and destination volumes.
Clean install/repair/update peak measurement remains pending; the previous
universal 500 MB reserve is withdrawn. Refresh sizes for the next actual package.

## Windows 10/11 · x64

Run `SpectralForge-Chimera-1.3.2-beta.1-win64-Setup.exe`.
Select VST3, standalone and optional reference tools. Setup requires administrator privileges for the standard VST3 folder.

- VST3: `C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3`
- Standalone: the selected application directory. The default retains `C:\Program Files\SpectralForge\Chimera Amp Matrix` for upgrade compatibility with development builds.
- Offline manual: Start Menu → SpectralForge → SpectralForge Chimera → Manual.
- The MSVC runtime is linked statically; a separate Visual C++ runtime installer is not required.
- Setup uses the same application identity as earlier builds and retires only the exact legacy executable/VST3 binary. Personal presets and IR folders are preserved.
- Uninstall from Windows Installed Apps. Personal user files remain.

The ZIP is a portable layout for manual inspection/use. Its VST3 bundle must be copied to a host-scanned folder; running the standalone does not register the plugin. MSIX output is an internal review package only and is not a public beta installer.

Windows Setup에서 앱과 VST3 경로를 각각 선택할 수 있습니다. 사용자 지정 VST3 폴더는 DAW 검색 경로에도 추가하세요. 이전 수동 설치본이 다른 경로에 남아 있다면 새 버전을 확인한 뒤 중복된 구형 플러그인을 정리하세요.

## macOS 12 or later · Apple Silicon and Intel

Run `SpectralForge-Chimera-1.3.2-beta.1-macos-universal.pkg`.
The universal app, VST3 and AU contain both arm64 and x86_64 code. Package installation requires administrator authorization.

- App: `/Applications/SpectralForge Chimera.app`
- VST3: `/Library/Audio/Plug-Ins/VST3/SpectralForge Chimera.vst3`
- AU: `/Library/Audio/Plug-Ins/Components/SpectralForge Chimera.component`
- Manual: `/Library/Application Support/SpectralForge/Chimera/Documentation/MANUAL.html`

The unsigned, unnotarized beta can be blocked by Gatekeeper. Use only an intentionally downloaded, hash-verified official package and review macOS's security information. Do not disable system security globally. The beta is not represented as a frictionless signed public release.
If macOS requests microphone access, permit it for the standalone app or host you use for recording. Refresh the DAW plugin scan after installation.

Before the renamed beta, macOS builds were distributed as loose bundles. If you manually copied `Chimera Amp Matrix.app`, `.vst3` or `.component`, remove those obsolete exact bundles after confirming the new version and backing up sessions; the pkg does not delete arbitrary manually installed folders.
Uninstall the three installed product bundles. Keep personal presets/IRs in your user data folder.
Do not remove the documentation directory as a whole if it contains your annotations or other personal files.

## Linux · x86_64 Debian/Ubuntu

The release builder targets Ubuntu 22.04 with glibc 2.35 baseline. Other distributions are not certified by the build.
Install the `.deb` with a dependency-resolving package manager:

```sh
sudo apt install ./SpectralForge-Chimera-1.3.2-beta.1-linux-x86_64.deb
```

- Standalone: `/usr/bin/chimera-amp-matrix`
- VST3: `/usr/lib/vst3/SpectralForge Chimera.vst3`
- Desktop launcher: SpectralForge Chimera
- Manual: `/usr/share/doc/chimera-amp-matrix/MANUAL.html`
- Uninstall: `sudo apt remove chimera-amp-matrix`; personal user data is not removed.

The tarball contains the same `usr` layout and a per-user `install.sh`. Extract it, read `RUNTIME-DEPENDENCIES.txt`, then run `sh install.sh` from the extracted directory. It installs only beneath your home directory: `~/.local/bin`, `~/.vst3`, and `~/.local/share`. Put `~/.local/bin` on PATH if starting it from a terminal. This archive does not bundle glibc or replace your system audio stack. It requires the listed runtime libraries; it is not an AppImage or a distribution-independent static binary.

For an old manually copied plugin, remove only the obsolete `Chimera Amp Matrix.vst3` bundle after confirming the renamed bundle works. Avoid simultaneous system and per-user copies of the same plugin.

## Document preservation (introduced in 1.3.1-beta.2)

The installer retires an obsolete document only when its exact path, byte count and SHA-256 match a verified previous package. Edited or unrecognized documents are retained. Personal presets, IRs and project files are outside this document migration, and required license notices remain available.

When an existing manual or other current product document has been edited, a verified copy is saved before repair installs the current version. Saved copies are kept across repair and uninstall:

| Installation | Saved document copies |
|---|---|
| Windows Setup | `PreservedDocuments` inside the selected application directory |
| macOS PKG | `/Library/Application Support/SpectralForge/Chimera/Documentation.preserved` |
| Linux DEB | `/usr/share/doc/chimera-amp-matrix.preserved` |
| Linux TAR | `~/.local/share/doc/chimera-amp-matrix.preserved` |

Each saved copy retains its original relative filename under a folder named for its SHA-256. If a linked or unreadable managed document prevents safe preservation, the package operation stops or preserves that file; review the installer log before retrying.

Windows Installed Apps and Linux package-manager removal apply the document-preservation rules automatically. macOS and Linux TAR do not provide a complete application uninstaller. Their document-only maintenance tool can remove verified, unchanged product documents while retaining edited documents, notices and unknown files:

```sh
# macOS, after removing the exact product app/plugin bundles:
sudo "/Library/Application Support/SpectralForge/Chimera/Tools/document-maintenance" remove-documents "/Library/Application Support/SpectralForge/Chimera/Documentation"

# Linux TAR, after removing the exact installed application/plugin files:
"$HOME/.local/libexec/chimera-document-maintenance" remove-documents "$HOME/.local/share/doc/chimera-amp-matrix"
```

These commands do not remove application or plugin bundles. Saved document copies are never automatically purged.

## 한국어 빠른 안내

설치 전 DAW와 Chimera를 종료합니다. Windows는 Setup, macOS는 universal PKG, Debian/Ubuntu는 DEB를 사용합니다. Linux TAR는 필요한 시스템 라이브러리를 별도로 요구하며 `install.sh`로 사용자 홈에 설치할 수 있습니다.
공개 베타는 아직 퍼블리셔 코드서명과 macOS 공증이 없습니다. 해시 일치는 파일 무결성 확인이며 서명과는 다릅니다. 기존 프로젝트를 복사해 보관한 뒤 새 버전의 모델 선택, IR과 자동화를 확인하세요. 개인 프리셋/IR은 제거 대상이 아닙니다.

