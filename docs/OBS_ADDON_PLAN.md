# Chimera 전용 OBS 애드온 개발 계획

사용자 명시 승인: 2026-10-23 KST.
[프로젝트 #129](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/129)
· [애드온 #131](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/131).

**상태: A단계 완료(구조 2·GPL 모듈 결정), B단계 시제품(#134)·C단계 상태 처리(#138) main 반영,
D단계 실시간 통계·장시간 시험과 실제 OBS 1시간 녹화 자동 시험(Linux·Windows·macOS Apple) 구현, E단계 Windows 배포 보호·버전 표기·사용 안내(#146), F단계 macOS(Universal, macOS 12+)·Linux 빌드와 사용자 설치 패키지, macOS Chimera PKG 선택 설치·Linux DEB/TAR 동봉(TAR는 설치 시 질문)·Windows·macOS·Linux 에디터 창·Linux·Windows·macOS 실제 OBS 앱 자동 시험 구현 중.
Windows Setup 선택 설치 외 공개 배포 없음.**
경계 검토: [OBS_ADDON_BOUNDARY.md](OBS_ADDON_BOUNDARY.md) · 모듈과 검증 기록: [OBS/README.md](../OBS/README.md).
atkAudio 호환 성공 여부와 관계없이 독립적으로 개발한다.

## 제품 범위

- OBS 오디오 필터 메뉴에 `SpectralForge Chimera` 직접 노출.
- Windows x64 우선. macOS Universal·Linux는 후속 네이티브 빌드/실사용 시험.
- 공통 DSP/파라미터/`.chimera` 프리셋/개인 IR/베타 수명주기 재사용.
- Classic/Dual/Matrix와 기존 PRE/POST/EQ/CAB의 기능 동등성을 목표로 하되,
  성능·지원 범위는 실측 후 명시한다. 방송용 경량 기본 프리셋은 기능 삭제와 별개다.
- OBS 설정 창에서 프리셋 선택·에디터 열기·우회·입출력 레벨·지연 정보 접근.
- 장면 저장/재시작 복원·소스 복제/삭제·다중 인스턴스의 독립성.

## 구조 결정과 첫 시제품

OBS 오디오 필터 API를 외부 경계로 사용한다. 기존 오디오 엔진을 복사해
다른 보이싱으로 분기하지 않는다. 다음 두 구조를 측정하고 결정 기록을 남긴다.

| 후보 | 장점 | 결정 전 확인 |
|---|---|---|
| 공통 DSP/프로세서 직접 연결 | 추가 VST3 검색 단계 없이 직접 사용, 공통 엔진 제어 | libobs와 제품/의존성의 배포 라이선스, 엔진 분리, JUCE 메시지 루프/UI 수명주기 |
| Chimera 전용 VST3 로딩 어댑터 | 기존 검증된 제품 바이너리 재사용, 제품 업데이트 분리 | 누락/중복 설치/버전 불일치, 스캔·호스트 스레드, 지연 및 장애 처리 |

별도 프로세스 격리가 필요하면 IPC 버퍼·지연·프로세스 종료 복구 비용을 측정한다.
프로세스 격리가 라이선스 조건을 자동 해결한다고 가정하지 않는다.
최종 구조는 OBS·JUCE·VST3·NAM/IR·아트 자산 조건을 확인한 뒤 결정한다.
설계 검토는 지금 시작하는 작업이며 추가 사용자 허가를 기다리는 단계가 아니다.

## 구현 단계와 검증

| 단계 | 구현 산출물 | 통과 조건 |
|---|---|---|
| A. 경계 확정 ✅ | 의존성/라이선스 표, OBS 버전·ABI·모듈/엔진 경계 | 빌드/배포 가능한 구성과 미해결 조건 명시 |
| B. Windows 오디오 시제품 ✅ (실제 OBS 31.1.2 앱: 자동 시험으로 로딩·처리·녹화 확인, 기존 VST3 대비 측정은 OBS 밖 시험만) | 필터 등록, 준비/처리/해제, 공통 엔진 출력 | 동일 DI/프리셋의 기존 VST3 대비 출력과 지연 측정 |
| C. 상태와 UI ✅ (실제 OBS 앱: 자동 시험으로 상태 적용·저장·재시작 복원 확인, 복제·다중 인스턴스·화면 조작 확인 전) | 전용 설정·공통 프리셋·IR·장면 상태 버전 | 저장/재시작/복제/다중 인스턴스 일치, 누락 IR 처리 |
| D. 실시간·방송 (진행: 도구·헤드리스 시험 완료, Linux 30.0.2·Windows 31.1.2·macOS 31.1.2 Apple 실제 OBS 1시간 녹화 자동 시험(`obs-long-recording.yml`, CI 가상 머신), 실제 장비·오디오 장치 1시간 녹화 전) | 가변 블록/중단 재개/샘플레이트·채널 변경, 실시간 통계·로그, 테스트 킷 전용 필터 세션 | 1시간 녹화, 시작/끝 영상 싱크, CPU·메모리·드롭아웃 기록 |
| E. 배포 (진행: Windows Setup 선택 설치·OBS 실행 중 보호·장면 보존·버전 표기·사용 안내, 서명 인증서 전) | 설치·업그레이드·제거·서명·사용 안내 | 사용자 장면/프리셋/IR 보존, 검증한 OBS 버전 명시 |
| F. 다른 OS (진행: macOS Universal(Apple silicon·Intel, macOS 12+)·Linux x86_64 빌드·사용자 설치 패키지·설치 위치 시험, macOS Chimera PKG 선택 설치, Linux DEB/TAR 동봉, 에디터 창, Linux 실제 OBS 30.0.2·Windows·macOS(Apple silicon) 실제 OBS 31.1.2 앱 자동 시험(필터 로딩·녹화·상태 적용·저장·재시작 복원), macOS Intel 실제 앱 확인 전) | macOS/Linux별 빌드·패키지 | 해당 OS의 실제 OBS에서 B–E 재검증 |

DSP 처리용 버퍼는 준비 단계에서 확보한다. OBS planar audio와 프로세서의 채널/
가변 블록 계약, 샘플레이트 변경 시 재준비, 소스 제거 시 오디오/UI/worker 종료
순서를 설계한다. 콜백에서 파일 I/O·IR 로딩·힙 할당/해제·UI 호출·잠금 대기를 피한다.
상태 로딩은 준비한 상태를 안전한 경계에서 적용하며 자동화/스무딩을 보존한다.

OBS 타임스탬프와 영상 오프셋, 내부 알고리즘 지연, 어댑터/호스트 버퍼 지연을
분리 측정한다. DAW의 PDC와 같은 동작을 OBS가 제공한다고 가정하지 않는다.
크래시/처리 실패 시 dry passthrough·mute 정책은 소스 종류와 사용자의 설정을
고려해 명시적으로 정한다. 조용히 dry로 바뀌어 프라이버시/소음 문제가 생기지
않도록 화면 상태와 기록을 일치시킨다.

## 완료 판단

기존 [13개 OBS 검사](OBS_COMPATIBILITY.md)를 전용 애드온용으로 재사용하되
호스트 식별은 atkAudio와 별도 기록한다. 기존 테스트 킷의 `init`는 현재
atkAudio/VST3 경로만 지원하므로 애드온 구현 때 별도 스키마를 추가한다.
빌드 성공, 실제 오디오 동등성, 방송 승인, 공개 배포를 각각 기록한다.
이 계획은 AAX Native P0나 기존 DSP 목표를 대체하지 않는다.

근거: [OBS Source/Filter API](https://docs.obsproject.com/reference-sources),
[OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate),
[OBS 소스/라이선스](https://github.com/obsproject/obs-studio),
[JUCE 라이선스](https://juce.com/legal/juce-8-licence/).
