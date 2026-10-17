# PR #71 macOS 타이밍 실패 독립 분석 — 2026-10-17

**판정: 입력 공급 스레드의 긴 sleep-return 지연과 worker estimator 실행 구간의 비CPU 지연이 함께 관측됐다. estimator 연산량 폭증이나 결과 저장 비용으로 설명되는 실패가 아니다.**

`release_approved=false`. 실제 DAW 수용, 최종 통합 소스 검증 및 공개 승인은 이 분석으로 대체하지 않는다.

## 1. 조사 대상과 기존 수정

- 실패 소스: `dc57c10053fea0534f872818d74c5d6ccab298cc`.
- 실패 실행: [39029285672](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39029285672), macOS job `117386314109`, attempt 1.
- 조사 시작 시 [PR #71](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/71)의 실제 최신 소스는 이미 `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`이었다.
- `dc57 → 3bec` 변경은 `Tests/PrecisionTunerIntegrationTests.cpp`의 41행 추가뿐이다. `ScopedFeederQoS`를 detector `prepare()` 뒤에 적용하고 기존 QoS로 복구·검증한다. baseline/candidate/normal-worker control 모두 같은 feeder 정책을 사용한다.
- `a85 → dc57`는 같은 테스트 파일의 진단 변경(17 additions / 4 deletions)이다. 실패를 없애기 위한 시간 보정, 재시도 또는 임계값 변경은 없다.
- 이번 진단 브랜치는 이미 존재하는 feeder 수정 위에 **분석기와 증거만** 추가한다. 제품 DSP, 테스트 조건, 빌드/배포 workflow를 수정하지 않는다. 기존 수정의 작성자로 주장하지 않는다.

## 2. 실제 실패 경로와 시간 분해

`192000 Hz / 256 samples / 27.5 Hz`: acquisition **195.169ms**, settling **-1(미획득)**, 최종 error **99999(유효 읽기 없음 sentinel)**. 99999를 실측 센트 오차로 해석하면 안 된다. 32경로 중 이 1경로가 실패했고 CTest 실패를 그대로 유지한다.

아래 값은 `dc57-macos-failed-route.log`의 원시 레코드로 재현된다. 시각은 해당 경로 시작 기준 ms다.

| 구간 | 실측 | 해석 |
|---|---:|---|
| feeder sleep #1 | 416.183667 → 550.035958 = **133.852291ms** | 목표 복귀 417.333333ms 대비 132.702625ms 초과. 그동안 다음 입력 블록 공급 및 관측이 멈춤 |
| feeder sleep #2 | 550.918417 → 722.111542 = **171.193125ms** | 목표 복귀 552ms 대비 **170.111542ms** 초과 |
| worker wake 지연, 해당 경로 최대 | **12.796458ms** | 마지막 입력 블록 도착 → worker 반복 시작. 198ms 현상을 wake 지연으로 분류하면 틀림 |
| worker service 지연, 최대 | **0.002667ms** | wake/입력 도착 이후 FIFO 서비스 완료까지 |
| ordered window 준비, 최대 | **0.004667ms** | FIFO 서비스 완료 → estimator 시작 |
| 마지막 estimator, index 23 | 550.626542 → 748.852458 = **198.225917ms wall** | CPU는 **2.354791ms**, 차이 **195.871126ms** |
| estimate 종료 → publication 처리 | 마지막 **0.000167ms** | 결과 계산 후 저장/판정 비용은 원인이 아님. 이 행은 `published=0` |
| 입력 도착 → publication 처리, 마지막 | **198.235792ms** | published=false이므로 화면에 보인 결과의 나이가 아님 |
| sleep 복귀 → 읽기 완료, 최대 | **0.006625ms** | `frequency()/confidence()` 읽기 구간 자체는 짧음 |

첫 정상 27.5Hz 결과는 약 **187.607292ms**에 publication되고 첫 합격 관측은 **195.169ms**다. 약 7.562ms의 차이는 worker 결과 생성 이후 feeder 관측 주기의 지연이다. 두 시각은 각각 JUCE monotonic epoch와 `steady_clock` 시작에 상대적이므로 미세한 epoch 차이가 있고, 이 값을 샘플 단위 동기화 측정으로 주장하지 않는다.

10-cent step 후 마지막 올바른 후보값은 **27.6593056Hz**다. 이 estimator가 748.852ms에 끝날 때 테스트의 마지막 관측은 이미 722.297ms에 끝났고 disable/stop 경로에 진입해 `published=0`으로 남았다. 마지막으로 유효하게 publication된 입력 시각은 약 416.184ms다. 722.112ms 관측에서는 그 입력이 250ms보다 오래됐으므로 읽기가 0인 것은 stale 규칙과 일치한다. 정확도 판정을 완화하거나 stale 읽기를 유지해서 통과시킬 문제가 아니다.

각 최대값은 서로 다른 행일 수 있어 합산하지 않는다. 같은 estimator의 wall/CPU/비CPU 값은 index 23으로 묶어 비교했다. `TIMING`은 실제 estimation을 수행한 반복만 보존하므로 **입력이 없던 구간의 모든 worker wakeup을 기록하는 OS scheduler trace는 아니다**. 오디오 장치 timestamp, GUI 표시 및 DAW callback 지연도 측정하지 않는다.

## 3. 원인에 대한 확정 범위

1. 긴 feeder sleep-return 구간은 `FEED_WAIT`에서 직접 확인된다. 단순히 관측 함수가 느렸다는 설명은 맞지 않는다.
2. estimator wall 198.226ms 중 thread CPU는 2.355ms다. 계산 구간의 대부분을 worker가 CPU에서 실행하지 못한 현상과 일치한다. 특정 OS 원인(타이머 병합, 선점, VM 호스트 정지 등)은 이 로그만으로 구별할 수 없다.
3. 두 지연은 겹친다. feeder 정지만으로도, wake 지연만으로도 전체 실패를 설명하면 과도한 결론이다. **합성 입력/관측 스레드와 worker 실행 중 스케줄링 지연의 복합 실패**로 좁힐 수 있다.
4. CPU 연산량, service 비용, publication 저장 비용을 줄이는 DSP 수정이 필요하다는 근거는 없다. threshold/sleep 보정/실패 제외/retry-until-pass는 하지 않는다.
5. 기존 `3bec` feeder QoS 수정 후 이번 3회 반복은 모두 성공했다. 다만 이전 `dc57`의 normal-worker-QoS control도 32경로가 성공했고, 현재 control도 성공했다. 서로 다른 CI 환경의 전후 비교이므로 **feeder QoS만이 원인과 해결책이라고 인과적으로 확정할 수 없다**. 실제 DAW에서의 스케줄링 안정성은 별도 확인 대상이다.

## 4. 현재 제품 소스의 OS별 검증

검증 소스는 모두 **`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`**이다. 각 artifact의 `chimera-build-version.json`으로 확인했고 ZIP SHA-256을 GitHub artifact digest와 대조했다.

공통 실행: [39036409337](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39036409337), attempt 1, `--repeat until-fail:3`.

| OS / job ID | 집중 CTest | 후보 경로 | 최대 획득 ms | 최대 안정화 ms | 최종 절대 오차 최대 ct |
|---|---|---:|---:|---:|---:|
| Windows / 117407058127 | 5종 × 3회 = 15 PASS | 96/96 PASS | 198.421000 | 206.153600 | 0.0000514413 |
| Linux / 117407057942 | 5종 × 3회 = 15 PASS | 96/96 PASS | 194.534947 | 210.941469 | 0.0000514413 |
| macOS / 117407057837 | 5종 × 3회 = 15 PASS | 96/96 PASS | 267.163750 | 252.092500 | 0.0000514413 |

JUnit는 마지막 반복의 5종만 요약한다. 15회 성공 및 96경로는 전체 `tuner-ctest.log`를 파싱해 확인했다. 과거 실패를 성공으로 바꾸거나 새 소스 성공과 합산하지 않았다. Windows thread CPU 값은 지원되지 않아 분석표에서는 null로 유지한다.

실패했던 macOS 경로의 새 3회 획득/안정화는 각각 **186.328/180.796**, **190.192/201.294**, **186.432/186.448ms**다. 전체 macOS 최대 획득 267.164ms의 290ms 대비 여유는 약 22.836ms다. 합격 사실 이상으로 여유 또는 장기 안정성을 주장하지 않는다.

획득·안정화는 각각 **0 < value < 290ms**, 최종 경로 정확도 **abs(error) < 0.1ct**, 획득 관측 조건 **abs(error) < 0.5ct 및 confidence > 0.9**를 유지한다. 250ms stale 정책 및 1,105 frozen estimator 비교도 변경하지 않는다.

2026-10-17 13:57 UTC 상태 확인 기준, 별도 전체 [제품/패키지 실행 39036409533](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39036409533)의 Windows/macOS/Linux build 작업과 [Windows update candidate 39036409321](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39036409321)은 진행 중이다. 집중 실행의 Windows/Linux native-navigation은 성공, macOS native-navigation은 진행 중이다. 이는 **튜너 집중검사 성공이며 전체 제품 검증 완료가 아니다**. 최종 API 조회 스냅샷은 `ci-status.json`에 별도 보존한다.

## 5. PR #88와 중복 방지

[PR #88](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/88) 소스 `0f859400abfb944fc99ea9cfab842c77a6851a50`의 `GlobalDSP.h`를 직접 대조했다. #88은 per-sample `queueGenerations`, producer 입력 간격 invalidation, 세대 전환 시 fresh window 보존을 다룬다. 현재 #71과는 생산 코드 기준선이 달라 #88의 파일 전체를 덮어쓰면 이후 추가된 `MacTunerWakeTimer` 등도 잃을 수 있다.

이번 실패 경로의 estimation trace generation은 0으로 유지됐으며 마지막 결과는 테스트 종료 시 publication되지 않았다. 이것을 #88의 세대 전환 fresh-window 버그로 분류하지 않는다. #88 로직을 복제하거나 이 브랜치로 merge/cherry-pick하지 않았다. 추후 #88 통합은 최신 타이머와 feeder 설정을 유지하는 별도 통합 검증이 필요하다.

## 6. 재현과 보존

- `raw/*-ctest.log.xz`: 원본 CTest 로그 바이트를 lossless 압축. metadata/JUnit 및 macOS normal-QoS 로그도 보존.
- `manifest.json`: source SHA, run/job에 연결되는 artifact ID, 원본 ZIP/CTest/XZ SHA-256.
- `*.csv`: 모든 후보 경로의 각 반복별 결과와 구간 최대값. 반복을 합쳐 덮어쓰지 않음.
- `dc57-macos-failed-route.log`: 실패 경로의 모든 FEED/FEED_WAIT/WORKER_CPU/TIMING 레코드, 누락 및 drop 없음.
- `analyze.py`: 원본 ZIP 또는 보존 XZ에서 같은 결과를 재생성. 진단 도구이며 실패 경로가 있어도 분석 자체가 성공하면 종료코드 0이다. release gate가 아니다.

```bash
python docs/evidence/pr71-timing-attribution-20261017/analyze.py \
  docs/evidence/pr71-timing-attribution-20261017/raw/dc57-macos-ctest.log.xz \
  --source-sha dc57c10053fea0534f872818d74c5d6ccab298cc \
  --output-prefix /tmp/pr71-dc57
python docs/evidence/pr71-timing-attribution-20261017/analyze.py \
  docs/evidence/pr71-timing-attribution-20261017/raw/current-macos-ctest.log.xz \
  --source-sha 3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58 \
  --output-prefix /tmp/pr71-current-macos
```

4개 ZIP과 보존 XZ의 분석 결과 전체가 같음을 검증했다(원본 ZIP hash 필드 제외). 잘못된 source SHA는 거부된다. Linux CTest의 개행된 `Passed` 표기도 처리한다. 분석은 32개 과거 경로 및 288개 현재 경로를 보존하고, 과거의 1개 실패 및 현재의 45회 CTest 성공을 구분한다.

이 문서·분석기만 추가한 진단 커밋의 신규 CI는 위 제품 소스의 검증과 별개다. 문서 추가 커밋에 과거의 전체 제품 PASS나 공개 승인을 이전하지 않는다. 최종 공개할 소스가 변경되면 그 SHA와 실제 DAW 수용 결과를 새로 연결해야 한다.
