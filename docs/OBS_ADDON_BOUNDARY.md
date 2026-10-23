# OBS 애드온 A단계 — 경계·라이선스 검토 기록

[#131](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/131)
· [개발 계획](OBS_ADDON_PLAN.md). 작성 2026-10-23 KST.

**상태: 검토 기록과 권고안. 구조 최종 결정은 소유자 확인 대기(법률 검토 포함).**
OBS 모듈 바이너리·libobs 링크 코드는 아직 없다. 이 기록은 법률 자문이 아니다.

## 의존성·라이선스 표

| 구성 요소 | 라이선스(확인 시점 기준) | OBS 모듈에 미치는 조건 |
|---|---|---|
| Chimera 제품 소스/DSP/모델/IR/아트 | 독점, All rights reserved (`COPYRIGHT.txt`) | 소스 공개 의무가 생기는 결합에 넣을 수 없음 |
| OBS Studio / libobs | GPL-2.0-or-later | libobs를 링크해 OBS 프로세스에 로드되는 모듈은 GPL 파생물로 취급될 위험이 큼. 공식 플러그인 템플릿도 GPL-2.0 |
| JUCE 8 | AGPLv3 또는 상용 라이선스 | 제품은 상용 조건 전제. GPL 모듈 안에서 JUCE를 쓰면 두 조건을 함께 충족해야 함 |
| Steinberg VST3 SDK | 3.8부터 MIT | 호스팅 코드에 넣어도 GPL 모듈과 충돌하지 않음 |
| atkAudio | 확인 필요 | 호환 시험(#130) 대상일 뿐 애드온 의존성 아님 |

핵심 충돌: **독점 Chimera DSP를 libobs 링크 모듈 안에 직접 넣는 구성(계획의 후보 1)**은
GPL 결합 문제를 해결하지 않으면 배포할 수 없다. 프로세스 격리가 이를 자동으로 해결한다고
가정하지 않는다(계획 원칙 유지).

## 구조 후보 비교

| 구성 | 라이선스 경계 | 지연·안정성 | 배포 |
|---|---|---|---|
| 1. 독점 DSP를 OBS 모듈에 직접 링크 | 미해결 충돌. 소유자 결정·법률 검토 필수 | 추가 지연 없음. 크래시가 OBS 전체로 전파 | 단일 DLL |
| 2. GPL 공개 모듈 + Chimera VST3 프로세스 내 로딩 | 모듈만 공개, 제품은 별도 설치 VST3. OBS의 기존 VST2 필터·atkAudio가 쓰는 "GPL 호스트 + 독점 플러그인" 형태 | 추가 지연 없음(버퍼 직결). 크래시 전파 | 모듈과 VST3 버전 정합 필요 |
| 3. GPL 공개 모듈 + 별도 프로세스 엔진(IPC) | 경계는 가장 분리되나 해석 확인 필요 | IPC 버퍼만큼 지연 증가, 엔진 종료 복구 필요 | 두 프로세스 + 공유 메모리 |

**권고: 후보 2**, 단 모듈은 JUCE 없이 MIT VST3 SDK로 Chimera VST3만 호스팅한다(JUCE
AGPL/상용 조건을 모듈에 끌어오지 않음). 범용 VST3 호스트가 아니라 Chimera 번들만
식별·로딩하며, 기존 제품 VST3의 파라미터 ID·`.chimera` 상태·IR·베타 수명주기를 그대로
쓴다. 크래시 전파 위험은 후보 3으로 옮길 근거가 실측(B단계)으로 나오면 재검토한다.
최종 선택과 모듈의 공개 라이선스는 소유자 결정 사항이다.

## 이번에 구현한 공통 경계 (어느 후보든 사용)

`Source/OBSAudioBridge.h` — libobs·JUCE 비의존 실시간 경계.
`Tests/OBSAudioBridgeTests.cpp`와 CI(`OBS compatibility test kit`)에서 검사한다.

- OBS planar float, 최대 8채널 레이아웃. Chimera에는 앞 L/R(모노 레이아웃이면 1채널)만
  전달하고 서라운드 채널은 그대로 통과.
- 입력 매핑: Stereo · Left→양쪽 · Right→양쪽 · 합(L+R)/2. 오디오 인터페이스 입력 1의
  기타 신호용.
- 준비 용량보다 큰 OBS 틱은 용량 단위로 나눠 처리. 콜백에 할당·잠금·파일 I/O 없음.
- 미준비·샘플레이트/레이아웃 변경·채널 버퍼 누락·엔진 실패 시 정책 적용: 기본 **Mute**
  (원음이 조용히 방송에 나가지 않도록), 선택 시 Dry. 엔진 실패는 재준비까지 고정되어
  wet/dry가 번갈아 나오지 않는다. 상태(`BridgeStatus`)는 UI 표시에 사용.
- 설정 갱신 스레드의 재준비는 진행 중인 콜백이 끝날 때까지 기다리며 오디오 스레드는
  기다리지 않는다. 동시 재준비 2000회 중 반쯤 준비된 틱 0건(ThreadSanitizer 포함).
- 지연은 샘플·나노초로 노출. OBS에는 PDC가 없으므로 영상 싱크 보정 방식은 B단계 실측 후 정한다.

검증(이 환경): GCC·Clang `-Wall -Wextra -Werror`, ASan/UBSan, TSan 통과.
변형 시험으로 분할 처리 제거·재준비 대기 제거·서라운드 무음 누락·매핑 뒤바뀜을 각각 검출함을 확인.
실제 OBS 로딩·오디오·지연 측정은 미실행(OBS 없음).

## B단계 착수 조건

1. 소유자가 후보와 모듈 공개 라이선스를 확정.
2. 후보 2라면: Windows x64 OBS 모듈 템플릿(`obs_source_info`, `OBS_SOURCE_TYPE_FILTER`,
   `OBS_SOURCE_AUDIO`, `filter_audio`)에 이 경계를 연결하고, VST3 SDK로 설치된 Chimera
   번들의 해시·버전을 확인한 뒤 로딩. 동일 입력에서 기존 VST3(DAW/atkAudio) 대비 출력·지연 비교.
3. 지원 OBS 버전 범위(최소 libobs ABI)를 측정 대상 버전으로 고정.
