# PR #108 macOS 튜너 실패 조사 및 진단 출력 분리

## 판정과 적용 범위

조사 소스는 `5a5521f49bba8d00039a0c94e0d575ea5db8ba6e`, 실행은
[39281515993](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39281515993)이다.
Attempt 1과 이미 수행돼 있던 attempt 2 모두 macOS 반복 CTest에 실패했다.
두 실행을 합격으로 변경하거나 실패 경로를 제외하지 않는다. 유지보수 공개는
수정 소스의 모든 필수 CI와 배포 계약이 충족되기 전까지 차단한다.

## PR #70·#94 재사용

- #70의 최신 `3569bf3553e59c46fb1af47eaa2b3f04f79022cf`와 조사 소스의
  `PrecisionTunerIntegrationTests.cpp`, `GlobalDSP.h`, `MacTunerWakeTimer.h`는 동일하다.
  lifecycle의 source-paced 120-block 재획득, feeder QoS 복원, macOS strict wake timer를 보존한다.
- #94 `92bd01409146842d758c6b70249a8c18a3f5fa5b`의 분석기를 원문 그대로 재사용했다.
  `analyze-pr94.py.txt`는 원래 `docs/evidence/pr71-timing-attribution-20261017/analyze.py`이다.
  원본 ZIP SHA-256과 artifact의 `chimera-build-version.json` source SHA를 대조했다.
- #94와 마찬가지로 feeder와 worker의 지연을 분리한다. 특정 OS 원인(선점,
  타이머 병합, VM 호스트 지연 등)을 이 로그만으로 확정하지 않는다.

## 원시 결과

| 실행 / 세 번째 반복 경로 | 획득 ms | 안정화 ms | 최종 오차 ct | feeder 최대 sleep ms | worker 최대 wall ms / 같은 행 CPU ms |
|---|---:|---:|---:|---:|---:|
| attempt 1 / 48k·64·27.5Hz | 203.254792 | -1 | 99999 sentinel | 374.659959 | 376.221208 / 2.313375 |
| attempt 1 / 48k·256·30.867706Hz | 322.289375 | 202.158042 | -0.000031707 | 150.837250 | 150.513125 / 4.761250 |
| attempt 2 / 96k·256·82.406889Hz | 202.976375 | -1 | 31.74892933 | 189.873833 | 184.671042 / 1.744792 |

각 attempt는 96개 후보 경로와 14개 성공 CTest 실행을 보존한다. 실패 경로는 각각 2개와 1개다.
두 normal-worker-QoS control의 종료코드는 0이다. 따라서 실패가 특정 worker QoS만의
문제라거나 프리셋 변경이 estimator 연산량을 증가시켰다고 단정하지 않는다.

Attempt 2의 마지막 estimator는 416.531125→601.202167ms 동안 실행됐으나
CPU 시간은 1.744792ms다. 그 사이 feeder도 412.568750→602.442583ms 멈췄다.
결과 84.418296814Hz는 pitch step이 섞인 오래된 분석 창에서 나왔고, 이후 새 입력은
원래 600ms의 샘플 수를 채우기 위해 급히 공급됐다. 마지막 관측 전에 새 창의 정확한
결과가 나오지 않았다. 이 31.75ct는 지속 입력에 대한 estimator 자체 정확도 측정이 아니다.
그렇다고 정상 읽기로 처리하지 않으며 원래 경로 실패를 유지한다.

## 제한된 수정

기존 테스트는 후보 경로를 stop/join한 뒤 매 필드마다 unit-buffered `std::cerr`로
수천 번 출력하고 곧바로 다음 baseline/candidate 타이밍을 시작했다. CI 로그 수집기의
후속 작업이 다음 측정과 경쟁할 수 있는 구조다. 이를 제거하기 위해 모든 32개 A/B
타이밍 경로가 끝난 후 증거를 한 번에 출력한다. CSV 결과의 stdout과 진단의 stderr는
유지하고, 예외 발생 시에도 이미 수집한 로그를 RAII로 내보낸다.

이는 진단 I/O 간섭을 줄이는 테스트 수정이며, 과거 실패의 OS 원인이 로그 수집이었다는
인과 증명은 아니다. 새 CI가 실패하면 다시 공개를 차단하고 그 실패도 조사한다.

변경하지 않은 항목:

- 모든 입력 샘플, 경로 순서, baseline/candidate, 600ms 입력·300ms step.
- 실제 wall-clock 측정, sleep_until, worker/feeder QoS, 250ms stale 처리.
- 획득·안정화 `0 < value < 290ms`, 최종 정확도 `abs(error) < 0.1ct`.
- 모든 FEED/FEED_WAIT/WORKER_CPU/TIMING 기록, drop 검사, 실패 수와 종료코드.
- 생산 `Source/`의 튜너·엔진·serializer, 48개 프리셋 recipe, A/B·프로젝트·개인 IR,
  beta.1 제품 버전 및 `2027-02-11T14:59:00Z` 만료.
- 일곱 필수 workflow, 패키지 검증, 승인 게이트 및 beta.2 제외 정책.

유지보수 소스 검사는 수정된 튜너 테스트의 정확한 SHA-256만 추가로 허용한다.
일반적인 tuner 테스트 변경 허용 목록으로 넓히지 않는다. 시간/정확도 완화,
실패 라벨 제거, 시간 보정 mutation이 계속 차단되는 회귀 검사를 추가했다.

## 원본 보존과 재현

`provenance.json`은 artifact ID·job·원본 ZIP 해시와 모든 보존 member 해시를 연결한다.
`attempt*-*.json`의 `encoding: xz+base64` 파일은 원시 member 바이트의 무손실 포장이다.
decoded bytes의 길이·SHA-256을 검증한 뒤 `lzma.decompress(base64.b64decode(data))`로
복원할 수 있다. 원본 CTest, JUnit, normal-QoS 로그/종료코드와 build identity를 포함한다.
분석 CSV/JSON과 실패 경로의 전체 레코드도 별도로 보존한다.

원본 ZIP이 있다면 다음 명령으로 동일 결과를 얻는다. 분석기 자체의 종료코드 0은
경로 PASS가 아니라 분석 완료를 뜻한다.

```sh
python analyze-pr94.py.txt pr108-macos-attempt2.zip \
  --source-sha 5a5521f49bba8d00039a0c94e0d575ea5db8ba6e --output-prefix attempt2
```

수정 소스의 CI 결과는 위 과거 소스 증거와 별도로 기록해야 한다. 이 문서는 공개 승인이나
실제 DAW/청감 검증을 대체하지 않는다.
