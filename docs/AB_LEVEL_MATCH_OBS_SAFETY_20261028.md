# PR #183 — OBS 녹화·송출과 A/B MATCH 출력 안전성

기준일: 2026-10-28 KST. 조사 시작 PR head `369107be345732c428d57ba051e31553a279d3e6`.
동시 변경을 보존하기 위해 main `1d3e345`를 작업 브랜치에 병합했다. CMake의 충돌은
A/B MATCH 시험과 StateRecallAudio 시험을 모두 유지하여 해결했다.

## 신호 경로와 감쇠가 기록되는 조건

MATCH는 `Source/PluginProcessor.cpp`에서 최종 limiter 뒤, output meter 앞의 실제
오디오 버퍼에 적용된다. 두 슬롯을 순차 측정하며 양쪽 기준이 준비되고 MATCH가 켜진 경우
큰 쪽에만 최대 24 dB 감쇠한다. 일반 ON/OFF·A/B 전환은 50 ms linear-gain ramp다.
샘플이 감쇠된 뒤 downstream에 전달되면 나중에 녹음 상태를 알아도 되돌릴 수 없다.

| 경로 | 신호/상태 전달 | 기존 감쇠 포함 조건 | 이번 보호 |
|---|---|---|---|
| 일반 VST3 DAW, 녹음 상태 보고 | 입력 → Chimera DSP/MATCH → DAW bus; VST3 `ProcessContext::kRecording` → JUCE playhead `getIsRecording()` | 플래그가 오기 전 callback, 늦게 보고한 punch-in, 보호 전에 큐에 들어간 샘플 | 기존 guard 유지: 보고된 첫 callback부터 unity, 재활성화 차단, 자동 복귀 없음 |
| 오프라인 bounce/freeze | VST3 `kOffline` → JUCE `setNonRealtime` → `isNonRealtime()` | 호스트가 offline을 실제 전달하지 않으면 탐지 불가 | 기존 guard 유지; 실제 호스트 전달은 별도 검증 |
| 실시간 export, track resample, 외부 레코더 | ordinary playback과 같은 realtime callback일 수 있음 | MATCH가 켜져 있고 해당 출력이 녹음 입력이면 포함 | 수동 Capture safety; 전송 상태만으로 export를 추정하지 않음 |
| DAW/Standalone → loopback/desktop/app capture → OBS | Chimera 출력 → 장치/가상 케이블/OS mixer → OBS source → mixer/track/encoder | 호스트가 자신의 녹음을 보고하지 않으면 포함. OBS의 녹화/방송 시작은 upstream 플러그인에 전달되지 않음 | 해당 upstream Chimera에서 수동 보호. OBS 전용 애드온 업데이트만으로 이 경로를 보호할 수 없음 |
| 조사 시점 Chimera OBS 애드온 | OBS source planar float → `filterAudio` → `AudioBridge::process` → `ChimeraHost::process` → 별도 VST3 출력 → 원래 OBS planes → mixer/track/encoder | `ProcessSetup`/`ProcessData`는 `kRealtime`; context는 tempo/time-signature만 valid, `kRecording` 없음. 그러므로 활성 MATCH 감쇠가 기록/송출/monitor에 포함 가능 | 아래의 초기화 시 상시 보호 계약 적용 |
| 업데이트된 OBS 애드온 + 업데이트된 VST3 | 같은 audio path; 초기화에서 명시적 capture policy 요청/확인 | 지원 확인이 완료된 인스턴스에서는 MATCH 활성화 불가 | 첫 처리 전 lifetime lock, 모든 callback unity; A/B 선택·COPY 유지 |

VST3 transport는 해당 호스트가 보고한 상태다. `kRecording=false`, playhead 부재 또는
position 부재는 **외부 녹음이 없다는 증거가 아니다**. 반대로 DAW가 dry input을 플러그인
이전에서 녹음하면 그 dry 파일에는 MATCH가 없을 수 있다. post-FX bus, monitor capture,
마스터 export 등 실제 라우팅을 확인해야 한다. OBS에서 `Monitor Only`인 소스를 출력 트랙에서
제외하더라도 monitor 장치가 desktop capture로 재유입되면 다시 녹음될 수 있다.

직접 확인한 소스:
- `Source/PluginProcessor.cpp`: `updateComparisonProtection`, 최종 `levelMatch.process`.
- JUCE **8.0.8** `juce_audio_plugin_client_VST3.cpp`: `getPosition`의 `kRecording` 변환,
  `setupProcessing` 및 `process`의 `kOffline` 변환, `installAudioProcessor`의 host extension 연결.
- `OBS/src/plugin-main.cpp`: `filterAudio`가 planes를 받아 bridge 처리 후 같은 audio 객체를 반환.
- `OBS/src/audio-bridge.hpp`: input mapping, mono/stereo chunk 처리, 별도 monitor 출력 없음.
- `OBS/src/chimera-host.cpp`: 별도 output buffers를 다시 input planes로 복사; realtime context.

## OBS 보호 방안 비교와 채택

| 방안 | 장점 | 제약 / 판단 |
|---|---|---|
| MATCH 기본 OFF + 경고만 | 기존 비교 사용법 그대로 | 작업 중 켠 상태에서 녹화를 시작할 수 있음. 단독 보호로 부족 |
| OBS frontend 이벤트/active 상태 연동 | idle 때 MATCH를 허용할 수 있음 | STARTING 이벤트와 active snapshot, 초기 로드, recording pause, replay buffer, streaming+recording 동시 상태, stop 순서, 종료 수명 관리 필요. 이미 큐에 들어간 감쇠 샘플과 외부 캡처를 해결하지 못함 |
| 전용 애드온 상시 차단 | 녹화 시작 시점·API 누락·버퍼 상태에 의존하지 않음 | OBS 인스턴스에서는 레벨 매칭 불가. A/B/COPY는 유지. **현재 채택** |
| 별도 monitor bus/별도 comparison instance | program은 원래 출력, monitor만 매칭 가능 | 현 구조에는 분리된 출력이 없음. OBS routing, CPU, monitor 재캡처 방지 및 사용자 UX가 필요한 후속 작업 |

OBS frontend API는 `STREAMING_STARTING/STARTED/STOPPING/STOPPED`, recording 및 replay
buffer 이벤트와 active 조회를 제공한다. 현재 애드온은 libobs만 연결하고 frontend API를
사용하지 않는다. 이를 추가하면 OBS 내부 상태를 얻을 수는 있지만, 그 이벤트가 downstream
첫 기록 샘플 이전 모든 큐를 정리한다는 보장은 확인되지 않았다. 이번 변경은 이벤트 연동을
구현하지 않으며, `kRecording`을 항상 true로 꾸미거나 `kOffline`을 오용하지 않는다.

공식 계약 참조:
- [Steinberg ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)
- [OBS Studio frontend API](https://docs.obsproject.com/reference-frontend-api)

## 구현 및 호환성

`Shared/ChimeraCapturePolicy.h`는 MIT로 분리한 작은 VST3 ABI다. 인터페이스 존재는
이 인스턴스에서 MATCH gain을 평생 차단하라는 요청을 뜻한다. 제품은
`VST3ClientExtensions::setIHostApplication`에서 query → lock → acknowledge → release를
수행한다. 애드온은 controller 연결 후 응답을 검사하고, 없으면 load를 실패시킨다.
매번 새 host context를 만들어 이전 로드의 응답이 재사용되지 않도록 했다.
오디오 스레드에서 host query, 문자열 검색, frontend API, 새 할당·잠금을 추가하지 않는다.

- 애드온과 VST3를 함께 업데이트한다. 구형 VST3의 MATCH 존재 여부를 버전 문자열로 추정하지
  않는다. 미지원 VST3는 명시적인 업데이트 오류, 기본 무음/선택된 dry 실패 정책을 따른다.
- 새 VST3 + 구형 애드온은 상시 보호 계약이 없다. 수동 Capture safety를 써야 한다.
- 일반 VST3/AU/AAX/Standalone: 기존 비교 기능을 유지한다. **SETTINGS > Capture safety:
  lock MATCH off**로 외부 캡처 전 명시적으로 잠글 수 있다. 새 일반 인스턴스의 MATCH는 OFF다.
- 수동 잠금은 인스턴스의 재준비·프로젝트 복원에도 유지된다. 새 인스턴스로는 저장되지 않는다.
  OBS lifetime lock은 UI에서 풀 수 없다. `SAFE`는 감쇠 차단이며 녹화 감지의 주장이 아니다.
- 잠금 해제·녹음 종료로 MATCH가 자동으로 다시 켜지지 않는다. 측정은 보호 중 정지한다.
- 보호 진입은 다음 callback부터 즉시 unity다. 최대 24 dB 레벨 상승/불연속 가능성은 기존
  녹음 guard와 같다. 녹화 전 잠금과 출력 레벨 확인, downstream 큐 소진이 필요하다.
  일반 MATCH ON/OFF·A/B의 50 ms ramp 및 24 dB 상한은 유지한다.
- parameter ID, preset/project/scene schema, IR, DSP tone, host bypass PDC는 바꾸지 않는다.

## 실행 증거

Linux GCC 13.3 Release / JUCE 8.0.8 / VST3 hosting SDK 3.8.0 / libobs 30.0.2.
소스와 로그 해시는 [manifest](evidence/ab-match-obs-20261028/manifest.json)에 고정했다.
qualification 전용 로컬 빌드이며, 배포 설치본이나 상용 호스트 승인이 아니다.

- `ChimeraABLevelMatchTests` + `ChimeraHostBypassTests`: **2/2 PASS (26.12 s)**.
  MATCH 13개 검증 그룹; 기존 BS.1770/50 ms/24 dB, 새 수동/OBS 보호,
  24개 rate·block·channel 조합의 보호 첫 블록 byte identity를 포함한다.
- `ChimeraStateRecallAudioTests`: **1/1 PASS (27.33 s)**. main 병합 후 회상 오디오 회귀.
- OBS CTest: **4 PASS / 1 실행 환경 실패 (7.50 s)**. bridge, stats, base64,
  실제 Chimera VST3 host 통과. 초기화 보호 응답, mono/stereo, 가변 블록, state round-trip,
  tail reset, 다중 인스턴스, 반복 reload 포함. libobs filter는 `obs_startup`에서 실패했다.
- 음성 처리 전 거부 검증: 지원하는 실제 VST3를 로딩한 동일 host에 미지원 시험 복사본을
  다시 로드해 실패 확인. 이전 응답을 재사용하지 않고 prepare/process를 거부했다.
  시험 복사본은 Linux ELF의 유일한 policy IID 16 bytes 중 첫 byte만 바꾸어 지원 부재를
  만들었으며, 생산 bundle은 수정하지 않았다. 실제 구버전 제품 시험이라고 주장하지 않는다.
- 네이티브 UI 시험: **실행 환경 실패 유지**. Xvfb 설치 경로를 보완했지만 X display를
  열 수 없어 `MATCH UI test requires a working native display`로 실패했다.
  경고·SAFE·SETTINGS 실제 UI 승인을 주장하지 않는다.
- Packaging: **29 cases, 26 PASS / Windows 전용 3 SKIP**. 최초 실행의 cmake PATH 오류를
  수정한 재실행 결과다. CI ownership map 21개와 CTest evidence producer 계약 PASS.
- 제품 VST3 및 OBS 모듈 컴파일 성공, `git diff --check` PASS.

재현 명령은 `ctest --test-dir <product-build> -R
'^(ChimeraABLevelMatchTests|ChimeraHostBypassTests|ChimeraStateRecallAudioTests)$'
--output-on-failure`, `ctest --test-dir <obs-build> --output-on-failure`다.
실제 UI/libobs에는 작동하는 display가 필요하며, 실패 항목을 skip/pass로 치환하지 않았다.

## 실제 OBS·DAW 승인 항목 — 미실행

| 항목 | 확인 방법 / 합격 조건 |
|---|---|
| OBS 실제 앱 + Windows 에디터 | 정확한 module/VST3 SHA 기록; 로딩 직후 SAFE, MATCH 비활성, SETTINGS 잠금 해제 불가. A/B·COPY·preset·scene 복제/전환, hide/show, disable/enable, 장치 rate 변경 후 유지 |
| OBS 녹화·송출·replay buffer | 6/12/24 dB 차이 A/B와 동일 DI 사용. 대기 → recording/streaming, 동시 시작·한쪽만 정지·pause/resume, replay buffer 시작/저장, 장면 전환. PCM 녹화와 MATCH-off 기준을 시간 정렬해 비교하고 첫 packet/전환부 확인 |
| OBS 실제 encoder/네트워크 | lossless PCM 비교와 압축 송출을 분리. AAC/Opus에는 bit-exact를 요구하지 않고 level·파형·dropout을 비교. program/monitor track routing과 재캡처 확인 |
| 구버전 혼합 | 새 애드온 + 미지원 VST3: 명확한 오류와 선택된 mute/dry 동작. 새 VST3 + 구형 애드온: 수동 보호 없이는 자동 안전으로 표시하지 않음 |
| DAW VST3 | record/punch-in/out, record-arm만 된 상태, stop, offline bounce/freeze, realtime export; 실제 flag 시점과 처리 첫 sample을 기록. 이미 큐에 있는 오디오와 track pre/post-FX 구분 |
| DAW AU/AAX | 실제 host wrapper의 recording/offline 전달, bypass/reset/PDC, 첫 sample 확인. Linux 공유 processor 시험으로 승인하지 않음 |
| Standalone/DAW 외부 캡처 | capture lock 전 감쇠가 녹음되는 조건 재현 → 수동 lock과 queue drain 후 기준 출력 비교; 플러그인 재생성 때 보호 상태 재확인 |
| 장시간·청감 | 물리 장치로 1시간 이상 녹화/송출, CPU peak·overrun·unprocessed·input gaps 로그, 보호 진입 레벨 점프와 A/B 전환 청취 |

## GitHub 상태

조사 시작 head `369107b`는 Open, non-Draft, main과 충돌로 보고됐다.
[Open Beta Build and Test 40531695066](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40531695066)는
validation-contract, Windows/macOS/Linux build, assemble-candidate, consolidate-release-gate 모두 success.
조회된 PR workflow 15개는 14 success / 조건부 factory preset publisher 1 skipped였다.
`get_commit_combined_status`의 legacy statuses는 빈 목록이다. 이는 Actions가 없다는 뜻이 아니다.
repository rulesets 조회는 빈 목록이었다. Legacy branch protection 조회는 GitHub integration 403으로 거부되어 서버의 필수 검사 목록 전체를
인증하지 못했다. 저장소 validation producer ownership 검사와 서버 보호 설정은 별개다.
이전 head 성공은 이번 변경 head의 성공으로 승계하지 않는다. 병합·릴리스·배포는 하지 않는다.
