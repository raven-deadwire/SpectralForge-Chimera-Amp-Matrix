# PR #101 CAB 계산 비용·스케줄링 분리

기준: 실패 소스 `1fa3b0ee924e40e1c5e089e0361f7cecaf6adc92`, 96 kHz / 32 samples / stereo,
expanded-v2, 3 CAB / 6 mics, 원래 333.333333 µs 예산. 이 브랜치는 PR #101 위에 쌓은
진단용 변경이며 AAX 공개 승인이나 제품 성능 수정이 아니다.

## 원본 재분석

- PR #101 run 39127354198 / macOS job 117675422043 / artifact 12137142606.
  ZIP SHA256 `f5714bfea7b161bdc4002f9479ced322bc1b48b33cc44f8ec1bd46e9bbbca0e9`.
  내부 manifest 107개와 66개 후보 timing route를 재검증했다.
- 17/1600 wall miss 중 **동일 callback CPU 초과 10개**, wall만 초과 7개.
  전체 wall p99 348.875 µs의 해당 callback 351 CPU는 146.625 µs다.
  별도로 정렬한 CPU p99 284.834 µs와 같은 표본이 아니다.
  wall 최대 callback 711은 7649.042 / CPU 57.583 µs.
  callback 729는 wall 1346.583 / CPU 1351.584 µs다.
  CPU 측정 구간이 wall 구간을 둘러싸므로 CPU>wall 소량 차이를 보정하거나 삭제하지 않는다.
- worker/automation 구간 13/1200, worker 정지 뒤 publication 구간 4/400 miss.
  뒤 구간의 네 miss는 모두 CPU도 예산 초과. publication 요청은 1200번 앞의 1회이고,
  miss는 1538/1553/1559/1584번이다. 구간 이름만으로 publication이나 fade 비용으로 귀속하지 않는다.
- 기존 sleep(1 ms)은 worker 구간 n%8==0 **처리 후**에 있다. 직후 표본은 n%8==1이다.
  이번 기록은 직후 6/150 (4.0%), 같은 worker 구간 나머지 7/1050 (0.667%).
  사후 관측이며 cache/frequency/worker 노출/OS·VM 대기 중 원인을 분리하지 못한다.
- PR #96 artifact 12109529952 ZIP SHA256
  `4b9f93a5386c79ad77db5c9ad608475a8e0f91a4ec7f1c062da3afcf751399c5`.
  내부 manifest 2177개, trial 96개, raw summary 3456개, 통계 72개 재계산 일치.
  prefetch 성능 미입증 및 deadline 비증가 실패 유지. 후보를 재실행하거나 채택하지 않는다.
  32-sample control A/B 각각 115200개: wall miss 132/148, CPU 동반 108/128,
  wall-only 24/20. 여러 엔진·채널 합산은 기술 통계이며 성능 효과 추정에 사용하지 않는다.

[원본 audit](../../docs/evidence/cab-scheduling-20261019/original-audit.json),
[실패 route CSV](../../docs/evidence/cab-scheduling-20261019/pr101-failed-route.csv).

재계산은 PR #96의 고정 `Tools/CabNeonAB/run.py`를 **분석에만** 사용한다.
고정 ref `84d5fbc7561397e25d7109d7b5d5d1de322d5f7a`, analyzer SHA256
`45ce016e4ed9620f4f3ac2a1ea25f6b8d0fafd57f4917a92786b74e6e27580bd`.

```sh
python Tools/CabScheduling/audit_original.py --pr101 /path/pr101 \
  --pr96 /path/pr96/evidence --pr96-analyzer /path/pinned-run.py --output audit.json
```

## 계측과 실험

PR #72의 bounded recorder를 실패 소스에 이식했다. 원래 callback/DSP 계산, worker
priority, 20ms polling, IR 길이, FFT/MAC 순서, 50ms fade, 상태 계약, 테스트 예산은 유지한다.
`CHIMERA_CAB_ATTRIBUTION`은 별도 진단 실행 파일에만 켠다. 기본 제품에서는 모든 hook이
컴파일 제거된다. 오디오/worker 각자 미리 할당한 buffer를 기록하고 worker join 이후에
CSV를 출력한다. overflow·clock 오류·누락·allocation이 있으면 불완전 증거로 처리한다.

- callback wall/thread CPU 원시 endpoints, FFT forward/inverse, spectral MAC,
  active/fading convolution, fade mix, kernel accept/reject/retire, IR worker
  generate/prepare/publish/collect를 callback ID·lane·mic·model/generation으로 연결한다.
- 별도 `.pacing.csv`는 호출 밖 sleep 시작/복귀 및 다음 callback 시작을 기록한다.
- `worker_wait`의 requested 20ms, wait 반환 시각, notified/timeout을 기록한다.
  timeout 복귀 지연은 `wait duration - requested timeout`으로 보고한다. notified wait는
  이 계산에서 제외한다. **OS가 runnable로 만든 시각은 수집하지 못하므로 순수 scheduler
  wake latency로 부르지 않는다.** 실제 scheduler trace가 있어야 해당 구간을 분리할 수 있다.
- stage 범위는 중첩된다. 서로 다른 stage inclusive 시간을 더해 callback 총비용으로
  사용하지 않는다. 재귀적인 `cab_process`는 같은 이름의 parent/child도 겹친다.
  wall-CPU 차이도 순수 scheduler 시간으로 간주하지 않는다.
- `analyze_trace.py`는 phase별 miss, CPU 초과, sleep 직후, worker build/publish overlap,
  timeout 복귀 직후, 실제 fade·kernel accept와의 연관성을 계산한다. Pearson/point-biserial은
  기술 통계다. 자기상관 callback을 독립 반복으로 간주해 유의확률을 만들지 않는다.

macos-15의 ABBA / BAAB **각각 별도 allocation**에서 다음을 실행한다. 서로 다른 allocation
수치를 합쳐 paired 효과로 계산하지 않는다. 모든 빌드는 측정 전에 마친다.

1. 실패 원본과 현재 hooks-off의 기존 5 CTest / 66 routes / 144 realtime routes.
   실패해도 그대로 보존하고 이후 진단을 수행한다. 원본 통과로 이전 실패가 지워지지 않는다.
2. recorder 계약·hooks-on direct-FIR/residue-bank/fade/latency/immutable spectra 회귀.
3. observer A=hooks-off, B=stages-on: ABBA 또는 BAAB; 세 엔진, mono/stereo 및 독립
   arithmetic/memory/clock controls. 추가 resources population은 별도 보관한다.
4. cadence A=원래 sleep 패턴, B=연속 실행: 같은 실행 파일에서 ABBA 또는 BAAB.
   hooks-off와 hooks-on 각각 반복. 집중 경로는 expanded-v2 / 96k / 32 / stereo.
   cadence는 진단 harness만 바꾸며 제품 scheduling 수정안이 아니다.
5. 원시 CSV, 모든 실패, source/JUCE/compiler/binary hashes 및 manifest를 업로드한다.

원래 acceptance 측정은 수정하지 않는다. focused probe의 준비·render·fade drain callback 수는
실패 테스트의 200/150/200에 맞췄다. probe의 checksum·slot·clock 부하는 원래 CTest와 다르며,
그 결과를 원본 성능 수치로 대체하지 않는다. 비동기 worker 구간은 실제 publication/fade
노출이 같지 않을 수 있다. stopped-worker 구간도 실제 slot/출력 checksum 일치 확인 전에는
동등 노출로 주장하지 않는다.

### 출력·상태를 고정하는 추가 진단

`--canonical-reset yes`는 worker를 정지한 뒤, 측정 밖에서 CAB를 다시 prepare하고
동일한 outgoing IR 두 개를 각 lane에 설치한다. convolution 이력과 mic swap 순서까지
고정한다. 단순 `reset()`만으로는 swap 순서가 남아 일부 출력이 달랐다. 초기 실패 시도는
폐기하지 않고 `portable-canonical`에 보존했다. 수정한 진단은 `portable-canonical-v2`다.
기본값은 `no`이며 기존 acceptance 또는 제품 상태 흐름을 바꾸지 않는다.

추가 16회 실험의 stopped-worker 400 callbacks는 모든 ABBA/BAAB 조합에서 checksum이
일치했다. 앞의 비동기 worker 1200 callbacks는 여전히 동등 노출이 아니다. sleep 조작은
앞 구간에만 적용되므로 뒤 구간의 결과를 직접적인 sleep A/B 효과라고 해석하지 않는다.

모든 컴파일을 마친 후, 다른 벤치마크·컴파일과 겹치지 않게 실행한다.

```sh
cmake -S Tools/CabCallbackAttribution -B /tmp/cab-attribution-build \
  -DCMAKE_BUILD_TYPE=Release -DCHIMERA_ATTRIBUTION_BLOCK_SIZE=32
cmake --build /tmp/cab-attribution-build --config Release --parallel 2 \
  --target ChimeraCabCallbackProbe ChimeraCabCallbackProbeControl
python Tools/CabScheduling/run_focused.py \
  --probe /tmp/cab-attribution-build/ChimeraCabCallbackProbe_artefacts/Release/ChimeraCabCallbackProbe \
  --control /tmp/cab-attribution-build/ChimeraCabCallbackProbeControl_artefacts/Release/ChimeraCabCallbackProbeControl \
  --output /tmp/cab-focused-new --experiments cadence --canonical-reset yes
```

수집 후 각 trial을 독립 검증한다. `off`/`stages`를 해당 trial의 trace level에 맞춘다.
반복을 합친 디렉터리를 하나의 trial로 검증하지 않는다.

```sh
python Tools/CabCallbackAttribution/verify_attribution.py \
  --input /tmp/cab-focused-new/cadence-off-0-0-A --output /tmp/verification.json \
  --expected-block-size 32 --expected-mode cab --expected-trace-level off \
  --expected-engines expanded-v2 --expected-channels 2
python Tools/CabScheduling/analyze_trace.py \
  --input /tmp/cab-focused-new/cadence-off-0-0-A --output /tmp/associations.json
```

[수행 결과와 해석 한계](../../docs/evidence/cab-scheduling-20261019/RESULTS_KO.md),
[portable 반복 요약](../../docs/evidence/cab-scheduling-20261019/portable-summary.json),
[새 macOS acceptance audit](../../docs/evidence/cab-scheduling-20261019/native-realtime-audit.json).

## 판단 기준과 실장비 검사

현재 원인 확정·제품 수정 효과는 **미입증**이다. 기존 prefetch 후보나 timeout 완화 없이,
새 기록에서 동일 phase·노출·실행 파일의 반복 결과가 특정 비용과 일치할 때만 좁은 수정안을
만든다. 수정한다면 기존 direct-FIR/bit/fade/IR 상태/latency/allocation 및 원래 deadline gate를
모두 유지하고 같은 조건의 A/B로 재검증해야 한다.

CI `deadline_miss`와 실제 dropout은 별도 증거다. 물리 장비 검사에는 macOS 버전/CPU,
오디오 인터페이스·driver, DAW·plugin 형식, 96k/32·64, 동일 DI/IR/automation, 장치 xrun,
loopback 녹음의 discontinuity, 최소 반복시간과 source/binary hash가 필요하다.
Instruments/System Trace의 callback/worker runnable·running·blocked 기록과 CPU frequency/
cache counters는 가능한 실제 Mac에서 별도 수집한다. 현재 콘솔 CI는 이를 대신하지 않는다.
