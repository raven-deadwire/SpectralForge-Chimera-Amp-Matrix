# OBS 32.2.2 Windows 시작 정지 조사 — 2026-11-02

## 판정

초기 진단 보강 커밋 `6868afd8621f2d96262c12d67213cec5019ffe0a`의 native Windows
대조 시험에서 같은 형태의 정지를 재현했다. **OBS 32.2.2의 `CreateScene` 요청이
30초 동안 응답하지 않고, 이후 정상 종료도 60초를 초과했다. Chimera 모듈이
없는 대조군에서도 재현됐다.** 필터 인스턴스와 VST3 엔진은 생성하지 않았다.
따라서 이 정지는 Chimera 모듈·필터·DSP가 필수 원인인 결함으로 분류할 수 없다.
새 재현의 native 스택과 일치하는 공식 소스에서 OBS frontend 시작과 WebSocket
장면 생성 사이의 교착을 확인했다.

원래 #248 실행에는 요청 추적과 스택이 없으므로 새 재현과 당시 정지가 동일한
내부 스택이라는 소급 단정은 하지 않는다. 다만 새 재현의 요청·시간 경과·종료
실패·로그 경계가 일치한다. 원래 최종 60초 종료 오류가 최초 예외를 가리던
진단 결함도 회귀 시험과 재현 로그로 확인했다.

## 비교한 원본

| 실행 | 시험 소스 | Windows runner image | 결과 |
|---|---|---|---|
| [#248 1차](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41342999051/job/124956799236) | `ecdf943b8cd79faeb8f6b554543a38752f9905ee` | `windows-2025-vs2026` / `20261021.278.1` | 32.2.2 시작 요청 정지 후 종료 timeout |
| [#248 2차](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41342999051/job/124968966407) | 동일 | `windows-2025-vs2026` / `20261030.294.1` | 기능·재시작·정상 종료 통과 |
| [#247](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41302441325/job/124836020337) | `2bf884025e885e6bde3b249af1d70fb4fa104e96` | `windows-2025-vs2026` / `20261030.294.1` | 기능·재시작·정상 종료 통과 |

세 실행은 Windows Server 2025, Python 3.12.10, MSVC 19.51.36260.0을 사용했다.
실패와 성공의 **runner image는 다르다**. 이 차이는 교란 변수이며, 이미지가
원인이라는 증거는 아니다. 재실행 성공을 동일 환경 반복 안정성으로 읽지 않는다.

두 시험 소스에서 `OBS/tests/obs_app_test.py`의 blob은 동일한
`afda499e98288273e208f87f51a9b94b3a48a5a5`다.
[소스 비교](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/compare/2bf884025e885e6bde3b249af1d70fb4fa104e96...ecdf943b8cd79faeb8f6b554543a38752f9905ee)는
workflow, 새 다중 인스턴스 시험 및 문서만 바뀌었음을 보여준다.
기존 앱 시험과 제품 소스는 바뀌지 않았다. 앞선 OBS 31.1.2 다중 필터 시험은
실패 실행에서도 통과했고 10개 인스턴스 해제를 기록했다. 새 선행 시험의 실행
순서는 달라졌으므로 이것도 환경 변수로 유지한다.

## 1차 실패 타임라인

아래 시각은 2026-11-02 KST다. 원본 OBS 로그의 UTC에 9시간을 더했다.

| KST | 확인된 사실 |
|---|---|
| 04:51:53.241 | `spectralforge_chimera_filter` 종류 등록 및 모듈 정보 출력 |
| 04:51:53.245 | OBS `Startup complete` 출력 |
| 04:51:53.252 | 장면 파일 부재로 기본 장면 생성 시작 |
| 04:51:53.295 | 기본 `Scene` 전환 및 `Chimera` **컬렉션** 생성 |
| 04:51:53.296–.298 | WebSocket Hello / Identified 성공 |
| 04:51:53.306844 | `GetVersion` 기반 준비 확인 성공 |
| 04:51:53.598–.599 | `rtmp-services` 백그라운드 갱신 성공 |
| 04:52:23.306 | 클라이언트 EOF / 1006, 준비 확인 약 30초 뒤 |
| 04:52:23.313937 | 버퍼에 있던 버전 문자열 출력 |
| 04:52:23.313983 | 창을 발견하고 `WM_CLOSE` 게시 |
| 04:53:23.316017 | 정상 종료 대기 60초 초과 |

버전 출력에는 `flush=True`가 없었다. 종료 검사 `check()`가 stdout을 비우면서
뒤늦게 표시됐으므로 버전 요청이 30초 걸렸다는 뜻이 아니다. 두 번째
`GetVersion`도 성공했지만, 정확한 완료 시각은 남아 있지 않다.

시험의 다음 순서는 `CreateScene("Chimera OBS test")` →
`SetCurrentProgramScene` → `CreateInput("DI loop")` → `CreateSourceFilter`다.
`CreateSceneCollection`은 호출하지 않는다. `Chimera` 컬렉션 생성은 OBS의
초기화이며 시험 장면 생성 성공 증거가 아니다.

[실패 artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41342999051/artifacts/13097733646)의
전체 로그에는 시험 장면 전환, DI 입력, Chimera loaded/session/unload 또는
녹음 증거가 없다. `Shutting down` 표식도 없다. 따라서 필터 해제 중의 정지로
분류할 근거가 없으며, UI가 종료 요청을 처리하지 못했거나 보류한 상태가 더
잘 맞는다. 30초 소켓 timeout 추정은 관측과 부합하나 원래 예외는 유실됐다.

원본 artifact에는 텍스트 10개만 있고 PNG·dump·traceback은 없다. ZIP SHA-256:
`572dfbb9802571dd2c6ff1efb5f9731583c04244b0cd4ee578a9f19183d0f107`.
강제 kill 직후 DLL 삭제 실패도 기록됐다. 기존 코드는 kill 후 wait하지 않아
정리 경합이 가능하므로 이를 별도 제품 결함 증거로 쓰지 않는다.

## 공식 소스로 확인한 경쟁 조건 경로

다음은 재현 증상과 일치하는 소스상의 순환 대기 경로다. 네이티브 스택 대조 결과와
원래 #248 실행의 증거 한계를 구분한다.

1. [obs-websocket 5.7.4 EventHandler](https://github.com/obsproject/obs-websocket/blob/5.7.4/src/eventhandler/EventHandler.cpp)는
   `SCENE_COLLECTION_CHANGED`에서 ready를 활성화한다. 이는 전체 frontend 초기화
   완료보다 빠를 수 있다. `GetVersion`은 UI 스레드 왕복을 보장하지 않는다.
2. [OBS 32.2.2 OBSBasic::OBSInit](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasic.cpp)는
   컬렉션 준비 이벤트 이후 `AudioMixer`를 만든다.
3. [AudioMixer 생성자](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/AudioMixer.cpp)는
   core `source_create` signal callback을 연결한다.
4. [CreateScene](https://github.com/obsproject/obs-websocket/blob/5.7.4/src/requesthandler/RequestHandler_Scenes.cpp)은
   source 생성 signal을 발행한다.
   [OBSBasic::SourceCreated](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasic_SceneItems.cpp)는
   worker에서 [BlockingQueuedConnection](https://github.com/obsproject/obs-studio/blob/32.2.2/shared/qt/wrappers/qt-wrappers.hpp)으로
   UI의 `AddScene`을 기다린다.
5. [libobs signal 구현](https://github.com/obsproject/obs-studio/blob/32.2.2/libobs/callback/signal.c)은
   callback 실행 중 signal mutex를 보유하며, 새 callback 연결도 같은 mutex를
   잡는다. 타이밍이 겹치면 UI는 mutex를, WebSocket worker는 UI를 기다릴 수 있다.

이 순환 대기는 Chimera 필터가 없어도 성립한다. OBS 31.1.2에는 새 `AudioMixer`
클래스의 이 늦은 등록 경로가 없다. 아래 신규 재현의 양쪽 스레드에서 이 경로를
확인했다. 알려진 upstream 이슈 번호가 있는 버그로 단정하지 않는다.

## 이번 변경

- 요청별 JSONL: UTC·단조 시계 경과시간·PID·phase·request ID/type·응답 코드·실패 종류.
  VST3 state 및 요청/응답 payload는 기록하지 않는다.
- 최초 예외·traceback을 보존하고 종료 실패는 부가 오류로 남긴다. 종료만 실패해도
  시험은 실패한다. 강제 종료 후 bounded wait로 실제 종료를 확인한다.
- 요청 30초, 정상 종료 60초 예산을 유지한다. 무관한 프레임 또는 부분 프레임이
  요청 deadline을 늘리지 못하게 한다. 실패를 숨기는 retry나 시작 sleep은 없다.
- kill 전 로그·창 상태·PNG·스레드 minidump를 수집한다. dump는 별도 프로세스에서
  최대 10초, UI 수집은 각 5초로 제한한다. full-memory dump를 요청하지 않는다.
- 성공과 실패의 증거를 run ID/attempt별 artifact에 남긴다.

## 최소 재현 시험

`OBS/tests/obs_startup_probe.py`는 native Windows에서 원래 설치 경로를 사용한다.
기존 사용자 OBS 설정이나 Chimera 설치가 있으면 거부한다. 각 관측은 새 프로필을
만들고, 소유 확인 및 프로세스 종료 확인 후에만 정리한다.

- A: Chimera 모듈 미설치.
- B: 같은 모듈을 설치·등록하되 **필터 인스턴스를 만들지 않음**.
- AB/BA 순서를 교대하고 원래 초기 RPC 순서를 유지한다.
- 종류 목록과 전체 OBS 로그로 실제 모듈 유무를 검증한다. 모든 소스에 필터가
  없음을 확인한다. VST3 바이너리는 필요 없으며 기본값은 존재하지 않는 경로다.
- 각 실패는 후속 성공과 별개로 남고 전체 종료 코드도 실패다. 안전하게 정리할
  수 없으면 나머지는 미실행으로 남긴다.
- 바이너리 SHA-256, 소스 SHA, Python/OS/runner image, run/attempt 및 전체 OBS 로그를
  관측별로 보존한다.

```powershell
python OBS/tests/obs_startup_probe.py `
  --obs C:/test/OBS/bin/64bit/obs64.exe `
  --module C:/test/package/chimera-obs `
  --work C:/test/new-startup-evidence --repeats 5
```

CI의 독립 `startup-controls` 작업은 제품 VST3를 빌드하지 않고 OBS 모듈만 31.1.2
기준으로 빌드한다. 같은 러너에서 32.2.2 10회(A/B 각 5회), 31.1.2 6회(A/B 각 3회)를
관측한다. 해당 작업에서 동일 DLL을 사용하며 해시를 남긴다. 이는 이전 실행의
바이너리·runner image를 완전히 재현하는 시험은 아니다. 기존 제품 빌드 및 실제
필터 녹음 시험도 별도로 유지한다.

## 실제 재현 결과 — 진단 커밋 `6868afd`

[Windows startup-controls 작업](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41377205562/job/125059928546),
[전체 증거 artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41377205562/artifacts/13111019789).
러너는 원래 실패와 같은 image `20261021.278.1`이었다.

| OBS | 모듈 조건 | 성공 | 실패 | 실패 요청 |
|---|---|---:|---:|---|
| 32.2.2 | Chimera 미설치 | 4 | 1 | `CreateScene`, id=3 |
| 32.2.2 | 모듈 등록, 인스턴스 0 | 2 | 3 | 모두 `CreateScene`, id=3 |
| 31.1.2 | Chimera 미설치 | 3 | 0 | 없음 |
| 31.1.2 | 모듈 등록, 인스턴스 0 | 3 | 0 | 없음 |

실패 case는 `03-module`, `06-module`, `09-absent`, `10-module`이다. 모두 원래
30초 요청 timeout을 보존했으며 종료 60초 실패는 별도 note로 남았다. 모든
관측에서 실제 모듈 등록 유무와 인스턴스 로그 부재를 확인했다. 특히 `09-absent`는
Chimera 등록·모듈 로그 모두 없고 VST3 파일도 존재하지 않는다. 각 실패에서
kill 전 thread minidump, PNG, 요청 trace, 전체 OBS 로그를 확보했다.

표본 5회씩의 실패 수 차이로 모듈의 위험도 또는 실패 확률을 비교하지 않는다.
앞선 실패를 성공으로 바꾸는 재실행도 하지 않았다. 전체 작업은 실패로 남겼다.
같은 커밋의 별도 Windows 기능 작업은 image `20261030.294.1`에서 31.1.2/32.2.2
실제 필터·녹음·복원 시험을 통과했다. 이 성공은 대조 시험의 4개 실패를 무효화하지 않는다.

원본 증거 ZIP SHA-256:
`b2cda823744cd68bd35336a9f289d1ac478da4784664c9a7f3f1475159e0776f`.
OBS 32.2.2 `obs64.exe` SHA-256:
`e79066670cd8e8b95662f4cd417c4fa6f5b0c77e856a7b41c7de0e00059ba6f2`.
대조군 공통 staged Chimera DLL SHA-256:
`8a281dd66704b61ae19550078f4aa9c8ddc946adacf914e55bd873d8b7be1985`.

## 신규 재현의 스레드 증거

`09-absent`의 요청 timeout 때와 종료 timeout 때 수집한 두 minidump를 실제 Windows
x64 unwind 정보로 풀었다. 공식 32.2.2 바이너리·PDB와 dump의 timestamp,
SizeOfImage, CodeView GUID/age를 대조했다. 주소를 stack에서 검색한 결과가 아니다.

| 스레드 | 실제 대기 스택의 핵심 | 의미 |
|---|---|---|
| 메인/UI, TID 8420 | `pthread_mutex_lock` ← `signal_handler_connect_ref` ← `AudioMixer::AudioMixer` (`AudioMixer.cpp:208`) ← `OBSBasic::OBSInit` | frontend 초기화 중 `source_create` callback을 연결하려고 signal mutex를 기다림 |
| WebSocket worker, TID 2404 | Qt 대기 ← `OBSBasic::SourceCreated` (`OBSBasic_SceneItems.cpp:205`) ← `signal_handler_signal` (`signal.c:305`) ← source/scene 생성 ← `RequestHandler::CreateScene` | signal callback을 실행하며 UI의 `AddScene` 완료를 기다림 |

공식 `signal.c`는 callback 실행 중 해당 signal mutex를 보유한다. 따라서 UI는
worker가 반환해야 풀리는 mutex를, worker는 UI 실행을 기다린다. 두 dump의
시각은 02:26:31.010 / 02:27:36.529 UTC이며, **65.519초 뒤에도 두 스레드의
전체 unwound PC 목록이 동일**했다. dump의 모듈 목록에도 Chimera가 없다.

[주석 스택](evidence/obs-startup-20261102/annotated-stack.txt),
[분석 방법·원본 식별·한계](evidence/obs-startup-20261102/provenance.md),
[개별 dump·바이너리·PDB 해시와 프레임](evidence/obs-startup-20261102/provenance.json)을
함께 보존한다. Qt PDB가 없어 Qt 프레임 이름은 가장 가까운 PE export와 offset으로
표시했다. mutex owner를 Windows kernel에서 별도로 조회한 것은 아니며,
순환 대기는 동시점의 두 실제 스택과 정확히 일치하는 소스의 lock 순서에 근거한다.
원래 #248에는 이 dump가 없으므로 같은 내부 원인이라는 판단은 추론으로 구분한다.

## 원인에 맞춘 시험 준비 수정

Windows의 시험 전용 observer 모듈이 실제 `OBS_FRONTEND_EVENT_FINISHED_LOADING`
콜백을 기록하도록 하고, 기능 시험은 **해당 OBS 프로세스의** 완료 표식을 확인한 뒤
장면 생성 요청을 보낸다. `GetVersion`만으로 frontend 준비를 판단하던 부분을
보완하는 것이다. observer는 Chimera 모듈과 분리하며, 미설치/등록 대조군 양쪽에
똑같이 설치한다. 제품 모듈 또는 배포 패키지에 포함하지 않는다.

시작 sleep이나 재시도, timeout 연장은 적용하지 않는다. 표식이 없으면 실패한다.
원래 준비 조건의 재현은 `--startup-observer`를 생략하여 계속 실행할 수 있다.
CI는 같은 observer를 사용하는 완료 조건을 검증하고, 앞선 무보완 재현 결과는
위의 원본 실행과 artifact로 보존한다. 이 수정은 시험 클라이언트의 준비 조건
보강이며 OBS 자체의 모든 외부 WebSocket 클라이언트 문제를 해결하는 패치는 아니다.

## 검증 상태

로컬 Linux에서 요청 deadline·최초 예외 보존·정리·대조군 안전성·실패 보존 및
새 frontend gate 회귀 시험 30개를 통과했다. Windows 전용 실제 dump 생성 시험
1개는 Linux에서 건너뛰며 앞선 native Windows CI에서 통과했다. 수정된 observer의
native 빌드와 32.2.2/31.1.2 대조 시험 및 실제 필터 기능 시험 결과는 PR의 새 커밋
CI에 기록한다. 이전 진단 커밋의 대조 시험 실패는 위 링크에 그대로 남긴다.
소수 관측 통과를 일반적 안정성 인증으로 확대하지 않는다. 이번 변경은 제품 DSP
또는 OBS 자체 수정이 아니라 시험 준비 조건·진단 결함 수정 및 재현 도구다.
