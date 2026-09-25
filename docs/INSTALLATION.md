# SpectralForge Chimera — Open Beta 1.0 installation

Version `1.0.0-beta.1`, release tag `v1.0.0-beta.1`.
Close the application and all DAWs before installing or updating. Save a copy of important sessions first.
Compare the downloaded installer against `SHA256SUMS.txt` from the same release.
The beta packages currently have no verified publisher code signature. The macOS binaries use an ad-hoc build signature for local code integrity; they are not Developer ID signed or notarized. No Windows Store certification is claimed.

## Windows 10/11 · x64

Run `SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe`.
Select VST3, standalone and optional reference tools. Setup requires administrator privileges for the standard VST3 folder.

- VST3: `C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3`
- Standalone: the selected application directory. The default retains `C:\Program Files\SpectralForge\Chimera Amp Matrix` for upgrade compatibility with development builds.
- Offline manual: Start Menu → SpectralForge → SpectralForge Chimera → Manual.
- The MSVC runtime is linked statically; a separate Visual C++ runtime installer is not required.
- Setup uses the same application identity as earlier builds and retires only the exact legacy executable/VST3 binary. Personal presets and IR folders are preserved.
- Uninstall from Windows Installed Apps. Personal user files remain.

The ZIP is a portable layout for manual inspection/use. Its VST3 bundle must be copied to a host-scanned folder; running the standalone does not register the plugin. MSIX output is an internal review package only and is not a public beta installer.

## macOS 12 or later · Apple Silicon and Intel

Run `SpectralForge-Chimera-1.0.0-beta.1-macos-universal.pkg`.
The universal app, VST3 and AU contain both arm64 and x86_64 code. Package installation requires administrator authorization.

- App: `/Applications/SpectralForge Chimera.app`
- VST3: `/Library/Audio/Plug-Ins/VST3/SpectralForge Chimera.vst3`
- AU: `/Library/Audio/Plug-Ins/Components/SpectralForge Chimera.component`
- Manual: `/Library/Application Support/SpectralForge/Chimera/Documentation/MANUAL.html`

The unsigned, unnotarized beta can be blocked by Gatekeeper. Use only an intentionally downloaded, hash-verified official package and review macOS's security information. Do not disable system security globally. The beta is not represented as a frictionless signed public release.
If macOS requests microphone access, permit it for the standalone app or host you use for recording. Refresh the DAW plugin scan after installation.

Before the renamed beta, macOS builds were distributed as loose bundles. If you manually copied `Chimera Amp Matrix.app`, `.vst3` or `.component`, remove those obsolete exact bundles after confirming the new version and backing up sessions; the pkg does not delete arbitrary manually installed folders.
Uninstall the three installed product bundles and the product documentation folder. Keep personal presets/IRs in your user data folder.

## Linux · x86_64 Debian/Ubuntu

The release builder targets Ubuntu 22.04 with glibc 2.35 baseline. Other distributions are not certified by the build.
Install the `.deb` with a dependency-resolving package manager:

```sh
sudo apt install ./SpectralForge-Chimera-1.0.0-beta.1-linux-x86_64.deb
```

- Standalone: `/usr/bin/chimera-amp-matrix`
- VST3: `/usr/lib/vst3/SpectralForge Chimera.vst3`
- Desktop launcher: SpectralForge Chimera
- Manual: `/usr/share/doc/chimera-amp-matrix/MANUAL.html`
- Uninstall: `sudo apt remove chimera-amp-matrix`; personal user data is not removed.

The tarball contains the same `usr` layout and a per-user `install.sh`. Extract it, read `RUNTIME-DEPENDENCIES.txt`, then run `sh install.sh` from the extracted directory. It installs only beneath your home directory: `~/.local/bin`, `~/.vst3`, and `~/.local/share`. Put `~/.local/bin` on PATH if starting it from a terminal. This archive does not bundle glibc or replace your system audio stack. It requires the listed runtime libraries; it is not an AppImage or a distribution-independent static binary.

For an old manually copied plugin, remove only the obsolete `Chimera Amp Matrix.vst3` bundle after confirming the renamed bundle works. Avoid simultaneous system and per-user copies of the same plugin.

## 한국어 빠른 안내

설치 전 DAW와 Chimera를 종료합니다. Windows는 Setup, macOS는 universal PKG, Debian/Ubuntu는 DEB를 사용합니다. Linux TAR는 필요한 시스템 라이브러리를 별도로 요구하며 `install.sh`로 사용자 홈에 설치할 수 있습니다.
공개 베타는 아직 퍼블리셔 코드서명과 macOS 공증이 없습니다. 해시 일치는 파일 무결성 확인이며 서명과는 다릅니다. 기존 프로젝트를 복사해 보관한 뒤 새 버전의 모델 선택, IR과 자동화를 확인하세요. 개인 프리셋/IR은 제거 대상이 아닙니다.
