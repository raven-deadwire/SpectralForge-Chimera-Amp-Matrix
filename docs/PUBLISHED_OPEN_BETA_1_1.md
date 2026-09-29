# SpectralForge Chimera — Open Beta 1.1

**Version `1.1.0-beta.1` · Windows / macOS / Linux · 2026-09-29**

베타 1.0 이후의 앰프·페달 확장, 조절 화면 개선, Windows 설치 경로 선택과 종료 안정성 수정을 포함합니다.

## 다운로드

| 운영체제 | 설치 파일 |
| --- | --- |
| Windows 10/11 x64 | [Windows Setup — Standalone + VST3](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.0-beta.1/SpectralForge-Chimera-1.1.0-beta.1-win64-Setup.exe) |
| macOS 12+ · Apple Silicon / Intel | [Universal PKG — App + VST3 + AU](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.0-beta.1/SpectralForge-Chimera-1.1.0-beta.1-macos-universal.pkg) |
| Debian / Ubuntu x86_64 | [Linux DEB — Standalone + VST3](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.0-beta.1/SpectralForge-Chimera-1.1.0-beta.1-linux-x86_64.deb) |

[한국어 / English 오프라인 매뉴얼](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.0-beta.1/MANUAL.html) · [SHA-256](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.0-beta.1/SHA256SUMS.txt)

Windows portable ZIP과 Linux TAR.GZ는 Assets에서 받을 수 있습니다. Windows 설치 전에 DAW와 Chimera를 종료하고 앱·VST3 경로를 선택하세요. 사용자 지정 VST3 폴더는 DAW 검색 경로에도 추가해야 합니다. 설치 후 SETTINGS의 버전이 **Open Beta 1.1**인지 확인하세요.

## 주요 변경

- **23개 앰프, 39개 PRE 페달.** 5슬롯 보드에서 모델 선택·복제·순서 변경과 모델별 설정 저장을 지원합니다. 앰프별 전체 조절부와 채널·입력 경로를 연결했습니다.
- 선택한 페달의 원본 참조명을 작게 표시합니다. 4노브는 2×2, 5노브는 위 3개·아래 좌우 2개로 배치합니다.
- Matrix LOW에 **BAND TONE / LOW DI COMP / DI / AMP MIX**를 우선 표시하고 전체 앰프 조절부는 **ALL**로 엽니다.
- POST는 기존 6행 랙 구성과 **ALL** 상세 조절을 제공합니다. 컴프레서·프리앰프·EQ는 중간 카테고리 없이 모델을 선택하며 참조 표시는 짧은 모델명으로 정리했습니다.
- Windows Setup에서 앱과 VST3 설치 경로를 각각 선택하고 기억합니다. 복구·제거 시 사용자 프리셋과 IR을 보존합니다.
- 사용자 IR 카탈로그·ZIP 가져오기 및 별도 동반 IR 폴더 인식을 개선했습니다. **공개 패키지에는 개인 Raven IR이나 NAM 캡처를 포함하지 않습니다.** 내장 factory IR 2개와 참조 메타데이터를 제공하며 추가 IR은 사용자가 직접 가져옵니다.

## 종료 안정성 및 확인 범위

JUCE 화면 동기화 호출이 계속 실패할 때 종료 신호를 확인하지 않아 무한 대기하던 코드 경로를 수정했습니다. 에디터 종료 시 공유 이미지 자원, 오디오 중단 시 IR·튜너 작업과 convolution 커널을 정리합니다. 호스트 안의 지원 창은 HTTP 업데이트 작업을 만들지 않고 공식 릴리스 페이지를 엽니다. Standalone의 업데이트 확인·다운로드 무결성 검증은 유지합니다.

**사용자 Studio One 환경에서 종료 멈춤이 해결됐다고 확정한 릴리스는 아닙니다.** 원본 코드의 실패 조건 재현, 수정본 회귀 검사, Windows VST3 종료·DLL 해제 검사를 통과한 범위의 수정입니다. 동일 프로젝트로 다시 확인해야 하며, 재발 시 `STUDIO_ONE_TEARDOWN.md`와 선택형 `Trace-Chimera-Session.ps1`로 종료 상태를 수집할 수 있습니다. 기본 실행에서는 진단 로그를 남기지 않습니다.

기존 프로젝트와 factory preset은 내부 호환 경로를 사용하며 새 보드·패널을 편집하면 해당 새 처리로 전환됩니다. 중요한 세션의 사본에서 모델 선택·자동화·IR·출력 레벨을 확인하세요. Transpose의 지연·아티팩트, CPU 부하와 호스트별 호환성은 계속 베타 검증 대상입니다.

## 배포 검증

이 릴리스는 해당 소스의 Windows·macOS·Linux 빌드와 자동 검사, Windows 설치·복구·제거 검사 및 Defender 검사, 각 운영체제 패키지 검증을 모두 통과해야 공개됩니다. 최종 패키지의 SHA-256과 업데이트 명세를 확인한 뒤 업로드하며, 공개 다운로드와 베타 채널 노출도 점검합니다. 자동 검사 통과는 모든 DAW 환경의 호환성이나 실물 장비 재현도를 보장하지 않습니다.

## 베타 안내

- 이번 공개 베타는 무료입니다. 향후 유료화될 수 있으며 베타 종료 후 무료 지원이 종료될 수 있습니다.
- Windows 파일은 퍼블리셔 코드서명 전이며 macOS는 Developer ID 서명·공증 전입니다. 운영체제에서 설치 또는 실행 경고가 나올 수 있습니다.
- 앰프·이펙트는 원본 장비를 참고한 소프트웨어 모델입니다. 제조사와 제휴하거나 하드웨어와 동일한 재현도를 인증한 제품이 아닙니다. 실물 DI·리앰프 기반 검증이 충분하지 않아 일부 모델의 재현도가 낮을 수 있습니다.
- 오류 보고에는 OS, DAW 버전, 샘플레이트·버퍼, 사용 모델과 재현 순서를 포함해 주세요. [GitHub Issues](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues) 또는 SETTINGS → Bug report를 이용할 수 있습니다.

## English summary

Open Beta 1.1 adds the 23-head amplifier collection and 39-model, five-slot PRE board, model-specific amp/POST controls, compact Matrix LOW controls, the six-row POST rack with ALL panels, reference-name captions, and selectable Windows app/VST3 installation paths. Shutdown changes address a reproduced JUCE VBlank failure loop and resource cleanup; resolution of the reported Studio One hang remains unconfirmed on the user's system. Personal IR/NAM captures are not redistributed. Windows packages are unsigned; macOS packages are not Developer ID signed or notarized. This beta is free, may become paid, and free support may end after beta.

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.

Third-party software and assets remain subject to their respective notices and licenses.
