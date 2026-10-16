# PR #68 튜너 획득 회귀 조사 — 2026-10-16

기반 소스: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
이 변경은 진단·수정 후보이며 **릴리스 승인이 아니다**.

## 원시 로그의 구분

- Signal-path run 38572999588, macOS job 115882776633: 192k/64/440Hz 안정화 300.430208ms, 192k/256/82.406889Hz 획득 -1. 스테레오·관측 만료 회귀는 PASS.
- Full-product run 38572999910, macOS job 115883246585: 전체 54/55 PASS. 44.1k/256/30.867706Hz 획득 296.005458ms, 48k/256/27.5Hz 안정화 319.809542ms, **48k/256/440Hz 및 96k/256/27.5Hz 획득 실패**. 후자는 요청 요약에 없던 추가 실패다.
- 두 실행은 같은 기반 소스지만 서로 다른 실행·호스트 이미지다. 지정된 signal-path 실행은 macos-26-arm64, 전체 검사는 macos-15다. 관련 원시 로그/발췌를 보존했다. 전체 로그 발췌는 튜너 CSV 시작부터 CTest 요약까지다.

## 원인별 판단

| 경로 | 확인한 사실 | 판단 |
| --- | --- | --- |
| 신호·정적 정확도 | 기존 로그의 최종 route 오차는 작은 값이고 pure estimator 검사는 성공 | 정적 추정기 오류만으로 획득/안정화 실패를 설명할 수 없음 |
| 분석 창 갱신 | 첫 worker 관측·활성화/채널 세대 변경 시 FIFO 전체를 버림 | 지연된 worker가 이미 수신한 정상 입력까지 폐기해 171–186ms 창을 다시 채우게 되는 경로 재현 |
| worker 스케줄링 | 정상 우선순위, 분석 완료 후 10ms wait. 과거 로그에는 worker 실행 간격/큐 계측 없음 | 과거 실패 전부의 OS 스케줄링 원인은 확정 불가. 새 계측의 iteration/compute는 wall time이며 thread CPU가 아님 |
| 큐 상태 | 모든 시작/세대 변경이 큐 폐기를 유발하므로 정상 backlog와 구세대 입력을 구분하지 못함 | 샘플별 생산자 세대 표시로 현재 입력만 보존 |
| 관측 만료 | 기존 stale/혼합큐/재획득 검사는 통과 | 250ms reader 만료와 worker의 expired-gap 전체 폐기 유지. cold-start 혼합큐 보호 추가 |
| 전체 릴리스 | 별도 CAB realtime 실패, full-product macOS 튜너 실패 | 튜너 부분 PASS를 전체 제품/릴리스 PASS로 승격하지 않음 |

통제 재현: 활성화 전에 worker를 210ms 지연시키되 입력은 정상 속도로 공급했다.
수정 전 389.422ms로 FAIL, 초기 수정안 218.758ms로 PASS. 최종 수정안의 전체 검사에서는
218.728ms로 PASS. 이는 **불필요한 창 재충전 경로**의 인과 근거다. 과거 macOS
각 실패가 모두 이 경로에서 발생했다는 증거는 아니며, 특히 안정화 지연의 직접 원인은
새 macOS 계측으로 추가 판단해야 한다.

## 최소 런타임 수정

1. SPSC 샘플에 세대 번호를 함께 발행하고, worker는 현재 세대 샘플만 소비한다.
   첫 실행/세대 변경 시 history는 초기화하되 현재 세대 FIFO를 통째로 버리지 않는다.
2. 아직 첫 입력을 소비하지 못한 worker도 오래된 입력을 되살리지 못하도록 생산자가
   250ms 초과 입력 단절을 세대 변경으로 표시한다. 필터·decimation phase도 초기화한다.
3. 실제 expired gap에서의 worker history/FIFO 폐기와 input-arrival timestamp 기반
   reader 만료는 그대로 유지한다. 오디오 콜백에 notify/lock/할당/추정 연산을 추가하지 않는다.

추가 고정 메모리는 Tuner당 queueGeneration 64KiB이며 샘플당 정수 write/read가 추가된다.
실시간 전체 성능 무회귀는 별도 검증 대상이다. worker polling/priority, estimator,
창 길이, 290ms 획득/안정화 기준, 0.1ct 정확도 기준, 호스트 파라미터·프로젝트 직렬화,
스트로브·TT·BFTS 보정 코드는 변경하지 않았다.

테스트 빌드에서만 큐 최대 occupancy, 폐기 샘플, 만료 reset 횟수, 분석 횟수,
최대 분석 wall time/iteration wall time을 출력한다. 세대 폐기와 스케줄 지연의 구분을
지원하며, 개별 입력별 추적·OS scheduler trace는 아니다.

## 로컬 검증

이 호스트에는 초기 CMake/JUCE 의존성이 없었다. JUCE 8.0.8 실제 소스를 가져와
GNU C++ 13.3.0 / C++20 / -O3 / pthread로 **실제 네이티브 JUCE worker**를 직접 빌드했다.
모의 Thread/FIFO를 사용하지 않았다. 파일별 SHA-256과 JUCE 커밋은 manifest.json 참조.

- 기반 소스 전체 native integration: 108 kernel / 32 A/B routes 및 lifecycle PASS.
- 수정 전 + 동일 신규 delayed-enable 회귀: FAIL (389.422ms), 원시 결과 유지.
- 중간 수정안 attempt1: 전체 PASS. 이후 cold-start 혼합큐 보호를 추가했으므로 최종 통과 근거와 구분한다.
- 최종 수정안: 108 kernel / 32 A/B routes, delayed-enable, cold mixed queue,
  stereo continuity/true switch, disable/re-enable, stale/nonfinite/rate-change,
  stalled-worker 3개 시나리오 모두 PASS.
- 최종 최대 acquire 194.565737ms / settle 205.149580ms / 최종 route |오차| 0.0000514413ct.
- Pure: 576 accuracy / 18 stress PASS. 최대 오차 0.0599684ct. TT/BFTS/custom/strobe 검사 유지.
- POST native: 9 models / 64 audio controls / 3 meter controls / 288 rate-switch stress blocks PASS.
- Windows/macOS: 별도 Draft PR의 소스 일치 CI 결과 대기. 로컬 PASS를 이들 플랫폼에 대입하지 않는다.
- 전체 제품 CTest, 실제 오디오 장치/DAW, CAB realtime, 설치·서명·공개 배포: 이 로컬 검사로 판정하지 않음.

## 직접 빌드 재현

JUCE 소스는 `/tmp/chimera-juce`에 8.0.8 태그로 checkout했다. 다음 정의를 공통 적용한다:
`-std=c++20 -O3 -pthread -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -I/tmp/chimera-juce/modules`.

JUCE의 `juce_core.cpp`, `juce_core_CompilationTime.cpp`, `juce_audio_basics.cpp`,
`juce_audio_formats.cpp`, `juce_dsp.cpp`를 각 모듈 폴더에서 object로 컴파일하고,
`Tests/PrecisionTunerIntegrationTests.cpp`는 `-ISource -DCHIMERA_TUNER_TEST_HOOKS=1`로
컴파일한다. 이 object들을 `-pthread -ldl -lrt -lz`로 링크한 실행 파일의 인자 없는
실행이 전체 검사다. `--delayed-enable-only`, `--cold-queue-only`, `--stale-only`로
관련 회귀를 분리 실행할 수 있다. CI는 저장소의 기존 CMake/CTest 경로를 사용한다.

공개/병합/기준 완화/릴리스 승인 flag 변경을 수행하지 않는다.
