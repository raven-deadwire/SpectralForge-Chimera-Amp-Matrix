# OBS 32.2.2 Windows 시작 정지 조사 — 2026-11-02

## 판정

PR #248의 1차 Windows 실패는 WebSocket 연결과 `GetVersion` 응답 이후,
초기 장면 설정 요청 경로에서 시작된 것으로 좁혀진다. `CreateScene` 또는
`SetCurrentProgramScene`이 우선 조사 대상이다. 기존 로그에는 요청별 추적과
네이티브 스택이 없어 정확한 요청 및 원인은 확정할 수 없다.

최종 표시된 60초 종료 실패는 최초 오류의 신뢰할 수 있는 식별자가 아니다.
기존 `finally: obs.quit()`에서 추가로 발생한 오류가 원래 예외를 가리고,
`main()`은 최종 `str(e)`만 출력했다. 이 진단 결함은 코드와 회귀 시험으로
확인했다. Chimera DSP 또는 필터 인스턴스 결함은 입증되지 않았다.

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

## 공식 소스로 확인한 가설

다음은 가능한 정지 경로이며 **당시 원인 확정이 아니다**.

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
클래스의 이 늦은 등록 경로가 없다. 원인 확정에는 정지한 프로세스의 양쪽
스레드 스택이 필요하다. 알려진 upstream 버그로 단정하지 않는다.

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

## 검증 상태

로컬 Linux에서는 요청 timeout/예외 보존/정리, 대조군 안전성과 실패 보존을
검증했다. native Windows OBS 및 Windows dump 생성 결과는 해당 PR의 CI로 기록한다.
재현이 없더라도 안정성 또는 원인 해결 판정으로 확대하지 않는다. 이번 변경은
제품 DSP 수정이나 OBS 자체 수정이 아니라 진단 결함 수정 및 재현 도구다.
