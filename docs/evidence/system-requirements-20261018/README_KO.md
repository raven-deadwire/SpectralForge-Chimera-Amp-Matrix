# 시스템 요구사항 근거 대조 — 2026-10-18 KST

## 판정과 변경 범위

- 공개판: `v1.3.2-beta.1`, 제품 SHA `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, 공개 `2026-10-17T16:51:47Z`.
- main 확인: `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`.
- 문서 기반: PR #98 `b7923ea8b7a0f85cfbfafb0e0338238adc09b181` / `codex/beta-settings-countdown-20261018`.
- #98 위의 별도 브랜치로 제안한다. #98의 버전·만료·카운트다운·생명주기 변경은 재작성하지 않는다. 이 변경의 차이는 사양 문서, 공개 문서 목록과 패키지 포함 목록뿐이다. 제품 Source/Tests/CMake/VERSION/만료·승인 정책은 변경하지 않는다.
- CPU/RAM/화면 사양은 권장 후보. 현재 공개판과 다음 통합 빌드의 전체 실시간 용량 인증은 미확정이다. `realtime-qualification.json`의 9개 mode/instance 항목 모두 NOT_RUN이며 null 계측값을 0 또는 PASS로 바꾸지 않았다.
- README, 설치 안내, EN/DE/KO 매뉴얼, 릴리스 노트 원문과 별도 `SYSTEM_REQUIREMENTS.md`에 같은 분류를 적용했다. 사양 원문은 기존 public-doc stager/publisher를 통해 다음 패키지에 포함된다. 공개 beta.1 자산/태그/릴리스 본문을 덮어쓰지 않는다.

## 확인한 소스와 실제 패키지

| 근거 | 확인 내용 / 한계 |
| --- | --- |
| 공개판 및 #98의 `CMakeLists.txt` | JUCE 8.0.8; VST3/Standalone + Apple AU; static MSVC runtime; ASIO 활성화 정의 없음; AAX 타깃 없음 |
| `Installer/Chimera.iss` | `MinVersion=10.0`, x64compatible; Standalone/VST3/ReferenceTools 선택 설치 |
| `.github/workflows/build.yml` | Ubuntu 22.04, macos-15, windows-latest; universal arm64/x86_64, deployment target 12.0 |
| `Source/PluginEditor.cpp` | 기본 1180×780; 크기 제한 885×585~1770×1170; UI 75/100/125/150%. OS DPI는 별도 |
| `Source/PluginProcessor.cpp` | 앰프 오버샘플링 1×/2×/4×/8×; index 2 = 기본 4× |
| `Tools/package_release.py`, `Tools/install-linux.sh` | ZIP/Setup 공통 stage; macOS app+AU+VST3; Linux `usr` payload를 사용자 경로에 복사. 다운로드, 전개, 설치 복사본은 별도 |
| JUCE 공식 8.0.8 `modules/juce_audio_devices/juce_audio_devices.h` | `JUCE_ASIO` 기본값 0. Windows ZIP exe의 ASCII `ASIO` 문자열도 미검출했으나 문자열 검색을 장치 열거 시험으로 계산하지 않는다 |
| 공개 `InstallerVerification.txt` | Windows 설치·복구·제거·앱 실행·VST3 DLL 로더 검증. 실제 DAW/드라이버 성능이나 디스크 최대 점유량 검사는 아님 |
| 공개 `Linux-package-verification.txt` 및 DEB control | glibc >=2.35 및 libstdc++6 >=12 등 실제 의존성. Ubuntu 빌드에서 링크 확인; 모든 Debian/Ubuntu 버전 인증 아님 |
| 공개 `macOS-package-verification.txt` 및 PKG | 두 아키텍처와 payload 대조. 실제 macOS 설치·하드웨어 오디오·Developer ID/공증은 미인증 |

위 CMake/installer/workflow/editor 소스는 공개판과 #98 사이에서 변경되지 않았음을 `git diff`로 확인했다. 다음 제품 변경에는 이 결과를 자동 승계하지 않는다.

공식 JUCE 소스: https://github.com/juce-framework/JUCE/blob/8.0.8/modules/juce_audio_devices/juce_audio_devices.h

## 저장공간 측정과 재현

`release-assets.json`: GitHub Releases API에서 조회한 실제 파일 이름·바이트·digest·URL.
`package-footprints.json`: SHA-256 일치한 ZIP/PKG/DEB/TAR의 **일반 파일 크기 합계**.
`macOS-PackageInfo.xml`: PKG XAR 안의 설치 메타데이터 원문.
네이티브 설치를 실행하거나 파일시스템 블록 점유량을 측정한 결과가 아니다.

1. 같은 릴리스의 ZIP/PKG/DEB/TAR와 SHA256SUMS를 별도 디렉터리에 다운로드한다.
2. 각 파일을 `release-assets.json` / SHA256SUMS의 digest와 대조한다.
3. `python docs/evidence/system-requirements-20261018/inspect_packages.py /path/to/packages` 실행. Python 표준 라이브러리와 `dpkg-deb`가 필요하다. 바이너리나 설치 스크립트는 실행하지 않는다.
4. 생성된 `footprints.json`을 이 폴더의 `package-footprints.json`과 대조한다. ZIP CRC, TAR/DEB regular file sizes, PKG XAR → gzip/odc CPIO 크기를 읽는다. PKG의 767247 KiB metadata와 785636516 file bytes는 서로 다른 집계이므로 강제로 같게 만들지 않는다.

실행 결과: ZIP 255230463 bytes / 27 files; DEB 263138580 / 22;
TAR 263151065 / 25 (`usr`=263138580); PKG 785636516 / 34.
추가로 Windows ZIP payload manifest 26개와 macOS manifest 33개의 크기·SHA-256을 실제 파일 바이트와 대조했다. 네 아카이브 전체 SHA-256도 공개 digest와 일치했다.
Setup EXE는 API 크기와 공개 checksum까지만 확인했으며, 별도 unpack/설치 공간 실측으로 표시하지 않는다.

Windows/Linux 1.5 GB와 macOS 4 GB는 `D+3P`의 현재 크기 예와 미측정 임시공간을 고려한 **작업공간 제안**이다. 실제 clean install/repair/update high-water mark가 아니며, 기존 설치를 추가 필요 여유 공간에 이중 계산하지 않는다. 사용자 자료/보존본/OS 의존성 캐시에는 별도 공간이 필요하다.

## 기존 CPU·메모리 자료 활용 범위

- 공개 제품 CI [39036409533](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39036409533): 실제 job 로그에서 Windows 64/64, Linux 58/58, macOS 57/57를 확인했다. `ci-excerpts.json`은 환경·시험 요약의 원문 발췌다. 확인한 환경 발췌에는 정확한 CPU 모델/RAM 할당이 없어 기본 runner 사양을 추정 기입하지 않았다.
- #76 `86c6fc0016c0c5794e99cb6fd2341bc5d55b90e5`: Apple M1 Virtual/3 cores/7 GB. 측정 source A `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`, B `20c2be7ab2bf4f72683dbf163055b8a7467901e3`. 96k/32·64, 세 CAB/여섯 마이크. 66 deadline rows PASS지만 개별 miss 유지. **전체 Matrix PluginProcessor 아님**. 원시 결과의 독립 감사를 다시 실행했다고 주장하지 않으며 기존 검증 보고서를 범위 한정해 활용했다.
- #96 `84d5fbc7561397e25d7109d7b5d5d1de322d5f7a`: bounded-prefetch 연구도 CAB-only, CPU/MAC 개선 0/12, 통합 적격 false. 최신 연구가 소비자 사양을 낮출 근거가 되지 않는다.
- `docs/ISSUE8_UI_REFRESH.md`, `docs/verification/issue8-ui/{candidate,comparison,manifest}.json`: 2026-10-03의 짧은 Matrix/filter-only-CAB 합성 입력, AMD EPYC 9V74 호스트 모델(실제 할당 코어/RAM 미기록), Linux/Xvfb, 48k/128. 후보 source `bf1e7da83614a5260c7eda869a3f26de40e83823`, 게시 `2eea78c3a08fa5a9c56ee303ded4bdca7d3b02ad`는 동일 tree. closed_audio p99는 반복별 p99의 중앙값 1533.17µs, 합산 miss 7/4500이다. 기존 compare_ui_refresh.py로 원자료를 재집계해 저장된 comparison.json과 완전히 일치함을 확인했다. 2개 GUI의 idle 3.03%는 message thread / 한 logical core 기준, 오디오 2인스턴스 성능이 아니다.
- `docs/GRAPHICAL_EQ_1_3_1_VALIDATION.md`: 두 EQ만의 24개 조건. 전체 플러그인 CPU로 확대하지 않았다.
- #99 `83f6bd7c1404415beb51042f159e6ed1e8ba6316`, #100 `6ec78c475311827ccfd1949c9ae9ce364f79c0b4`: 자체 Transpose 연구, 제품 Source 미변경, 목표 지연 미달. 다음 통합판 개선 성능으로 표시하지 않았다.
- `rg`로 Tools/Tests/docs/Validation의 `peak.?rss`, `working.?set`, `resident.?set`, `memory.?growth`, `memory.?peak`, `peak.?memory`, `rss_bytes`를 확인했지만 검토 범위에서 전체 30분 DAW 세션의 peak/growth 자료는 발견되지 않았다. allocation-free callback, 제한된 IR cache, 상태 저장 크기 등은 RAM 최소 인증과 다르다.

CPU와 메모리 공백은 새 공유 컨테이너 단기 수치로 메우지 않았다. 이 작업에서 실제 오디오 장치/DAW/Pro Tools를 실행하지 않았다. 재현 가능한 소규모 합성 결과의 존재와 대표 실기 30분 검증 완료는 별개다.

## 다음 실측 기록

`realtime-qualification.json`은 실행 전 기록이다. 48kHz/128·256/4×를 기본으로 각 mode·1/2/4인스턴스, Dual Blend/Crossover, Transpose bypass/−2, UI/analyzer closed/open을 실제로 수행한 조건별로 기록한다. 프리셋/IR 해시·CAB/마이크 개수·mono/stereo·전원모드를 고정하고 30분 재생/녹음, 프로젝트 재열기를 제안한다. 미수행 토글 조합을 수행한 것으로 확대하지 않는다.

장치 xrun과 host CPU 미터, callback wall p99/peak/deadline miss, thread CPU, UI CPU를 분리한다. 전체 host 프로세스 RAM 시작/최대/종료 및 테스트 전 세션 기준선, 인스턴스 추가 후 증가량, swap/메모리 압박도 함께 남긴다. 계측 없는 항목은 null/unavailable. 기존 acceptance 기준을 바꾸지 않는다.

## AAX 별도 관리

AAX Native Windows x64/macOS arm64+x86_64는 #97의 목표이며 공개 지원이 아니다. 서명된 패키지·해시·PACE/플랫폼 서명과 실제 Pro Tools 세션을 확보한 뒤 정확한 Pro Tools 버전에 맞춰 OS/CPU/RAM/저장공간 표를 확정한다. 일반 VST3/AU 요구사항을 Pro Tools 요구사항으로 복사하지 않는다.

- 공식 호스트 요구사항 확인: https://kb.avid.com/pkb/articles/en_US/Knowledge/Pro-Tools-System-Requirements (2026-10-18 열람; 페이지 Last Updated July 9, 2026). 호스트 사양은 Chimera 인증표로 편입하지 않았다.
- 공식 AAX SDK/서명 안내: https://developer.avid.com/aax/ (2026-10-18 열람). 개발자 서명 요구를 최종 사용자 Chimera iLok 활성화 정책으로 바꾸지 않았다.

## 검증 경계

문서·패키징 검사는 별도 `validation.json`에 기록한다. 새 제품 DSP/DAW 성능시험이나 공개 승인은 수행하지 않는다. 공개판의 strict 21 PASS / 107 BLOCKED와 owner-accepted beta 상태는 변경하지 않는다.
