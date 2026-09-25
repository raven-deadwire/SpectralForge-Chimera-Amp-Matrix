# SpectralForge Chimera 1.0 — Open Beta 1

**Version 1.0.0-beta.1 · Windows, macOS, Linux · 한국어 매뉴얼 / English quick reference**

기타·베이스용 앰프 및 이펙트 제품군 SpectralForge Chimera의 첫 공개 베타입니다. Classic 단일 앰프, Dual 병렬 앰프, Matrix 멀티밴드 모드를 지원합니다.

## 다운로드 / Downloads

| 운영체제 / Platform | 설치 파일 / Installer |
| --- | --- |
| Windows 10/11 x64 | [Windows Setup](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe) — Standalone + VST3 |
| macOS 12+ · Apple Silicon / Intel | [Universal PKG](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/SpectralForge-Chimera-1.0.0-beta.1-macos-universal.pkg) — App + VST3 + AU |
| Debian / Ubuntu x86_64 | [Linux DEB](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/SpectralForge-Chimera-1.0.0-beta.1-linux-x86_64.deb) — Standalone + VST3 |

- **[매뉴얼 / Offline manual (HTML)](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/MANUAL.html)** — 다운로드한 파일을 브라우저로 열면 인터넷 연결 없이 읽을 수 있습니다. 설치 패키지와 앱의 **SETTINGS → Manual**에도 포함되어 있습니다.
- [설치 안내 / Installation guide](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/INSTALLATION.md)
- [SHA-256 체크섬 / Checksums](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/SHA256SUMS.txt)
- Windows portable ZIP 및 Linux TAR.GZ는 아래 **Assets**에서 받을 수 있습니다. Linux는 Ubuntu 22.04 / glibc 2.35 기준이며 필요한 런타임 라이브러리가 별도로 필요합니다.

## 주요 기능 / Included

- **15 amp heads, 46 effect models, 31 factory presets** for guitar and bass.
- Classic / Dual / Matrix routing, PRE pedalboard and POST rack effects.
- Selectable boost/overdrive order and envelope/compressor order.
- Cabinet IR import and preset recall with embedded user IR data.
- SpectralForge / CHIMERA interface, offline manual, bug-report shortcut and beta update checks with SHA-256 download verification.

공개 패키지에는 라이선스가 확인된 기본 IR 2개가 포함됩니다. 개인 IR, NAM 또는 별도 다운로드가 필요한 외부 IR 파일은 배포하지 않습니다. 본인이 보유한 IR은 직접 불러올 수 있습니다.

The public package includes two licensed factory IRs. Personal IR/NAM captures and externally licensed IR packs are not redistributed. Import your own IR files locally.

## 검증 / Verification

These are the unchanged distribution binaries from successful [build #136](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/36107249747), built from the same source tree as tag `v1.0.0-beta.1`.

- Windows, macOS and Linux DSP/release-support checks and package assembly passed.
- Windows editor checks, install/repair/uninstall, installed VST3 loading, component-only installation, Unicode paths and personal-data preservation passed.
- Windows Defender scanned 80 distribution files with no detections; file hashes remained unchanged.
- macOS universal architectures/ad-hoc seals and Linux package layouts were verified. Actual hardware listening and DAW compatibility remain areas for beta feedback.
- All five downloadable binary packages are listed in `SHA256SUMS.txt`. The update manifest describes the three primary installers.

Windows Setup SHA-256:

```text
29f1e12501ff64140879b0d17eea15b48aa881969388ad35a5eab2fd0d99c899
```

## 베타 안내 / Known limitations

- **Windows 설치 파일은 퍼블리셔 서명이 없으며, macOS는 Developer ID 서명·공증 전입니다.** 운영체제에서 경고하거나 실행을 차단할 수 있습니다. Windows packages are unsigned; macOS uses ad-hoc signatures and is not notarized.
- 설치 전 앱과 DAW를 종료하고 중요한 세션을 복사해 보관하세요. 이전 개발 버전의 모델 선택 자동화는 새 버전에서 확인해야 합니다. Close Chimera and your DAW before installing; back up important sessions and review model-selection automation from development builds.
- Transpose can add latency or artifacts. CPU use depends on oversampling, routing and instance count. Host-specific behavior remains part of the beta evaluation.
- Amp/effect models are reference-inspired algorithmic models, without manufacturer endorsement or a claim of exact hardware reproduction. Matchless/Dumble clone references are identified; EICH Taste Punch has no exact T900 NAM validation. See the [amp validation report](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/v1.0.0-beta.1/docs/OPEN_BETA_NAM_VALIDATION.md).

문제는 앱의 **SETTINGS → Bug report** 또는 [GitHub Issues](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues)로 알려주세요. OS, DAW/플러그인 형식, 샘플레이트·버퍼 크기와 재현 순서를 함께 적어주시면 도움이 됩니다.

---

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.

Third-party software and assets are subject to their respective copyright notices and license terms.
