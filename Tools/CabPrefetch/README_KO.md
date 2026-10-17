# CAB 병목 조사 — PR #79·#81

2026-10-18 KST. 상태: #79·#81 원시 자료 재계산 완료, 독립 prefetch 후보 native CI 완료·성능 미입증. 통합·릴리스 승인 없음.

## 판단

96kHz·32 samples의 예산은 333.333µs다. #79에서 반복되는 계산 비용의 우선 조사 대상은 spectral MAC이다. 그러나 이 구간은 pack/unpack도 포함하며, 메모리 대기와 산술 처리량을 분리하는 hardware counter는 없다. deadline miss에는 CPU 예산 초과와 CPU는 짧지만 wall만 긴 사례가 함께 존재한다. 따라서 FFT 단독, DRAM 대역폭 단독 또는 worker scheduling 단독 병목으로 확정할 수 없다.

실행 환경도 구분해야 한다. #79 네 runner와 #81 hardware 기록은 Apple M1 (Virtual), 3 cores, 7GB RAM의 GitHub-hosted Apple Virtual Machine이다. ARM64 네이티브 바이너리 실행은 확인되지만 bare-metal 실제 DAW/device 시험은 아니다. guest thread CPU와 wall 수치만으로 호스트 vCPU 배치·OS preemption·캐시 지연·산술 stall을 완전히 분리할 수 없다. 가상 runner의 큰 A/A 변동과 tail을 제품 고유의 스케줄러 결함으로 바로 옮기지 않는다.

## 원시 자료 검증

측정 대상은 독립 실행형 CAB-only harness다. 세 lane 입력 복사는 측정 밖에 있고 전체 앰프 체인·DAW·오디오 장치 callback의 부하를 포함하지 않는다. 여기서 deadline miss는 측정한 CAB 구간의 wall 시간이 333.333µs를 넘은 횟수이며, 실제 장치의 dropout 기록과 동일한 자료는 아니다.

| 지표 | 측정 의미 | 해석 경계 |
|---|---|---|
| callback CPU | Apple runner의 `CLOCK_THREAD_CPUTIME_ID` 차이 | callback thread에 청구된 CPU 시간이며 worker thread CPU를 합산하지 않음 |
| wall p99 | callback wall 시간의 99백분위 | trial별 p99의 중앙값을 보고하며 전체 raw를 합친 p99가 아님 |
| deadline miss | 개별 wall 시간 > block/sample-rate | 평균이나 p99가 낮아도 희소 miss가 존재할 수 있음 |
| spectral MAC | 별도 profile 빌드의 MAC scope wall 시간 | pack/unpack·계측 비용·scope 안의 선점이 포함될 수 있으며 순수 MAC CPU 시간은 아님 |

후보별 manifest 2,177개 파일의 크기·SHA-256을 확인하고, 96 trial의 raw CSV에서 3,456개 요약과 72개 통계행을 재계산했다. 네 후보 모두 저장된 분석과 일치했다. 사전 지정된 고정 publication primary 경로의 32/64 samples 전체에서 후보별 CPU 개선 0/12, MAC 개선 0/12 경로다. 원래 CI의 감사 결과를 단순 인용한 것이 아니라 이번 조사에서 다시 수행했다. 각 후보의 CAB 계약은 통과했고 performance_demonstrated, review_eligible, integration_eligible, release_approved는 모두 false다. deadline_nonincrease도 네 후보 모두 false다.

아래 수치는 32 samples, 3 rigs/6 mics, 3 모델 계열 × mono/stereo의 6경로다. CPU와 wall p99는 계측 OFF control의 경로별 12회 trial 중앙값 범위이며, miss는 그 6경로의 원시 건수를 합산했다. 고정 publication 구간의 분모는 arm별 28,800 callbacks, worker/automation 구간은 86,400 callbacks다. 후보끼리는 서로 다른 runner이므로 수치 크기로 순위를 매기지 않는다.

| 후보 | 구간 | CPU 평균 A→B (µs, 경로 범위) | wall p99 A→B (µs, 경로 범위) | deadline miss A→B | CPU over-budget A→B |
|---|---|---|---|---:|---:|
| split | 고정 publication | 20.26–37.65 → 20.01–39.51 | 29.60–66.31 → 30.94–73.85 | 3 → 7 | 2 → 6 |
| split | worker/automation | 51.30–98.60 → 47.44–93.44 | 132.06–229.81 → 129.00–226.94 | 104 → 104 | 84 → 93 |
| clearhead | 고정 publication | 20.47–38.11 → 20.19–40.05 | 31.42–83.56 → 30.96–86.31 | 0 → 1 | 0 → 1 |
| clearhead | worker/automation | 47.91–84.40 → 51.87–90.96 | 122.42–221.94 → 125.02–210.29 | 80 → 124 | 65 → 116 |
| fusedlead | 고정 publication | 20.21–36.17 → 20.01–37.01 | 29.02–53.88 → 29.02–61.96 | 3 → 10 | 0 → 3 |
| fusedlead | worker/automation | 47.62–91.51 → 49.40–104.36 | 123.04–220.12 → 124.79–217.98 | 76 → 111 | 60 → 91 |
| tile32 | 고정 publication | 20.75–38.11 → 26.14–48.78 | 31.17–56.00 → 41.00–79.21 | 1 → 7 | 0 → 7 |
| tile32 | worker/automation | 58.34–98.08 → 73.64–155.26 | 130.98–229.79 → 183.48–431.04 | 87 → 1414 | 68 → 1428 |

CPU over-budget는 wall miss와 독립적으로 센 값이다. CPU 타이머가 wall 구간을 바깥에서 감싸므로 두 수치를 빼서 순수 스케줄러 대기 시간으로 정의하지 않는다.

## spectral MAC과 FFT 비용

다음은 각 실험 baseline A의 별도 profile 모집단에서 얻은 32-sample 고정 publication 구간의 경로별 trial 중앙값 범위다. 단계 타이머는 `steady_clock`의 wall 경과 시간이며 thread CPU가 아니다. 따라서 MAC scope 안의 preemption도 포함할 수 있다. control CPU에서 이 값을 빼거나 비율로 나누어 제품의 CPU 비용 비중을 계산하지 않는다. profile 빌드는 매 callback 여섯 mic의 `setCuts`도 실행하므로 추가 시계 비용만 있는 control과 동일한 workload라고 가정하지 않는다. 상위 active/fading convolution 시간에는 하위 FFT/MAC이 포함되므로 단계들을 전부 더하지 않는다.

| 실험의 baseline A | spectral MAC (µs/callback) | Forward FFT (µs/callback) | Inverse FFT (µs/callback) |
|---|---:|---:|---:|
| split | 15.58–31.58 | 1.14–1.38 | 1.09–1.26 |
| clearhead | 16.13–31.14 | 1.23–1.37 | 1.18–1.28 |
| fusedlead | 15.32–30.32 | 1.19–1.33 | 1.17–1.22 |
| tile32 | 15.48–32.38 | 1.19–1.33 | 1.12–1.26 |

이 계측 범위에서 MAC은 반복되는 큰 단계다. 동일 profile 관찰이 FFT 횟수만 줄이는 접근의 한계를 지지하지만, 이 수치를 #81의 다른 workload에 그대로 대입한 Amdahl 상한으로 쓰지는 않는다.

worker/automation 구간의 profile도 분리하면 다음과 같다. 모든 값은 µs/callback이며 같은 방식의 6경로 범위다. A/B 범위의 양 끝은 서로 다른 경로일 수 있으므로 이 표의 비율 자체가 paired 효과량은 아니다.

| 후보 | MAC A→B | Forward FFT A→B | Inverse FFT A→B |
|---|---:|---:|---:|
| split | 44.58–80.51 → 37.42–72.18 | 4.05–4.73 → 3.94–4.53 | 3.47–4.01 → 3.29–3.90 |
| clearhead | 39.55–75.30 → 38.35–82.46 | 3.86–4.74 → 4.12–4.70 | 3.05–3.84 → 3.38–3.94 |
| fusedlead | 38.12–75.45 → 37.59–81.47 | 4.16–4.62 → 4.30–4.68 | 3.28–3.68 → 3.14–3.84 |
| tile32 | 35.65–81.09 → 64.42–142.81 | 4.07–4.70 → 4.10–4.84 | 3.38–3.94 → 3.35–4.24 |

이 구간에서 모든 계측 범주가 함께 느려지거나 변하는 현상도 보인다. 이는 MAC만을 독립적으로 바꾼 인과 비교가 아니며 worker on/off, 모델 준비 상태 및 runner 영향을 구분하는 후속 진단이 필요하다.

## miss의 두 형태

32 samples의 A/B control에서 split은 218건 중 178건, clearhead는 205건 중 174건, fusedlead는 200건 중 150건, tile32는 1,509건 중 1,465건의 wall miss가 같은 콜백의 CPU 예산 초과를 동반했다. 이는 연산 실행 시간이 긴 사례가 많다는 뜻이며, 캐시 stall·코어 이동·실행환경 간섭을 배제하는 근거는 아니다.

split의 original-v1 stereo r0/control/AB/slot2/B, callback 455는 wall 15,909.5µs, CPU 85.792µs다. expanded-v2 stereo r0/control/AB/slot3/A, callback 203은 wall 11,251.916µs, CPU 85.125µs다. 이들은 MAC 산술량만으로 설명할 수 없는 wall 지연 사례다. 반면 original-v1 stereo r3/control/AB/slot1/A, callback 777은 wall 18,255.792µs와 CPU 11,076.333µs가 함께 크다. 극단값을 삭제하지 않았다.

worker/automation 구간과 고정 publication 구간은 worker 실행 여부뿐 아니라 요청 변화, 캐시/커널 상태, fade 노출, 콜백 사이 sleep과 순서가 다르다. 두 평균의 차이를 worker의 인과 비용으로 해석하면 안 된다. profile CSV의 MAC 호출 일정은 고정 구간에서 같지만 비동기 구간은 별도로 검토해야 한다.

### 실행 간격과 연관된 tail — 탐색적 분석

원래 harness는 worker 구간에서 `n % 8 == 0` callback을 마친 뒤 1ms sleep한다. 다음 callback(`n % 8 == 1`)은 각 arm의 86,400개 중 10,800개(12.5%)다. 아래는 해당 callback과 나머지 75,600개의 miss를 분리한 결과다. 이 분할은 사후 탐색이며 원래 판정 기준을 대체하지 않는다.

| 후보 | A: sleep 직후 / 나머지 miss | B: sleep 직후 / 나머지 miss | 직후/나머지 miss 발생률 비 A / B |
|---|---:|---:|---:|
| split | 33 / 71 | 36 / 68 | 3.25 / 3.71 |
| clearhead | 30 / 50 | 47 / 77 | 4.20 / 4.27 |
| fusedlead | 29 / 47 | 39 / 72 | 4.32 / 3.79 |
| tile32 | 30 / 57 | 932 / 482 | 3.68 / 13.54 |

처음 80개 automation 요청 callback을 분리한 뒤에도 baseline A의 경로별 CPU 평균 중앙값은 sleep 직후가 나머지의 1.22–1.84배다. profile의 MAC wall 평균도 A에서 1.07–1.26배지만 서로 다른 모집단이므로 두 증가분을 빼지 않는다. sleep 자체는 측정 구간 밖에 있다. 따라서 1ms를 callback에 잘못 더한 결과는 아니다. 다만 wake 이후 캐시 상태, 주파수, vCPU 배치, worker와의 경합, 남아 있는 fade 중 원인을 식별하는 대조 실험은 아니다. 실제 오디오 장치 주기의 재현도 아니다. 이 관찰은 실행 간격을 고정하지 않은 worker on/off 비교가 왜 부족한지를 보여준다. 어떤 callback도 원래 판정에서 제외하지 않았다.

## 반복 미세 최적화의 실패 해석

32-sample 소스의 FFT 크기는 128, IR segment 길이는 96 samples다. mono는 65 bins, stereo는 128 bins를 누산한다. 커널 길이가 L일 때 partition 수는 `floor(L/96)+1`이며 정상 full callback에서 각 partition의 각 bin에 네 곱셈·네 덧셈/뺄셈을 수행한다. 여섯 mic의 독립 history와 fade 중 추가 엔진이 이 반복량을 늘린다. 아래 후보들은 이 partition×bin 반복량을 줄이지 않는다. 누산 배열 하나는 mono 520 bytes/stereo 1,024 bytes지만, 전체 IR/history working set은 별도이므로 이 작은 배열 크기만으로 cache residency를 확정하지 않는다.

- split은 주소 계산과 wrap 분기를 줄이지만 전체 partition MAC 수를 줄이지 않는다. 고정 구간 32-sample CPU 효과는 −5.27%~+3.95%이며 신뢰구간/A/A envelope를 통과하지 못했다.
- clearhead는 작은 FFT 입력 버퍼 초기화만 줄인다. FFT·MAC·inverse·fade 연산량은 유지된다. CPU 효과 −17.45%~+5.53%는 효과 입증이 아니다.
- fusedlead는 한 번의 tail 복사와 선행 partition만 결합한다. delayed partition의 반복 MAC은 유지된다. CPU 효과 −3.71%~+9.15% 역시 기존 기준 미달이다.
- tile32는 출력 누산 버퍼가 이미 작은 상태에서 partition 순회를 반복한다. 동일 runner 내 고정 original-v1 stereo CPU 효과 −29.33%, 95% CI [−41.20%, −18.30%], A/A envelope 12.51%로 회귀가 확정됐다. worker/automation의 B miss 1,414건을 단순 잡음으로 숨길 수 없다. 추가 주소·순회 비용과 접근 패턴 악화는 소스에 근거한 설명 후보이며 cache miss 증가를 직접 측정한 것은 아니다.
- A/A CPU envelope가 수십 %인 경로가 존재한다. 아주 작은 최적화 효과가 있다고 하더라도 현재 반복 설계에서 확증할 수 없는 경우와, 실제로 느려진 경우를 구별해야 한다. threshold를 낮추거나 유리한 route만 선택하지 않는다.

## PR #81 원시 자료와 해석 경계

사용자가 내려받은 ZIP의 SHA-256 63fc5fefb526a76bf7f1651825ccb9c2ffd08de01da06ea4bffd09b3ff12d67c가 GitHub artifact digest와 일치했다. manifest 2,767개 파일을 검증하고 96 trial × 12경로의 원시 요약 1,152개와 통계를 재계산했다. 76개 A/B 출력 파일 쌍의 바이트 수·SHA-256 일치와 96 trial 순서도 재확인했다. 원본 보고서와 일치하며 CPU 개선 입증은 0/12, 기존 기준으로 확정된 회귀는 0이다. complete=true, apple_silicon_performance_demonstrated=false, release_approved=false다.

아래는 32 samples의 control이다. 시간은 경로별 12회 trial 중앙값이며 miss 분모는 각 arm·경로 16,800 callbacks(200 warmup 제외)다. 모델 계열 0=original-v1, 1=expanded-v2, 2=array-v3-6x10이다.

| 계열/ch | CPU 평균 A→B µs | wall p99 A→B µs | deadline miss A→B | CPU over-budget A→B | CPU 효과·95% CI·A/A envelope (%) |
|---|---:|---:|---:|---:|---|
| 0/1 | 14.02 → 13.30 | 18.27 → 17.81 | 0 → 2 | 0 → 1 | 2.86; [-5.75, 9.27]; 5.48 |
| 0/2 | 24.59 → 24.60 | 31.77 → 32.77 | 0 → 0 | 0 → 0 | -0.65; [-2.58, 1.20]; 15.32 |
| 1/1 | 14.20 → 13.45 | 18.81 → 17.88 | 0 → 0 | 0 → 0 | 5.03; [2.64, 6.95]; 12.23 |
| 1/2 | 24.34 → 24.50 | 30.83 → 31.88 | 2 → 0 | 1 → 0 | 1.13; [-2.84, 4.79]; 18.24 |
| 2/1 | 13.54 → 13.39 | 18.08 → 17.67 | 0 → 3 | 0 → 0 | -0.17; [-2.93, 2.52]; 29.92 |
| 2/2 | 24.13 → 24.37 | 30.83 → 32.06 | 1 → 22 | 0 → 0 | -1.28; [-2.53, -0.20]; 15.66 |

예를 들어 expanded mono의 CPU 효과 +5.03%, CI [2.64%, 6.95%]는 양수지만 A/A envelope 12.23%를 넘지 못한다. 이를 성능 입증으로 바꾸면 원래 기준을 완화하는 것이다.

| 계열/ch | profile Forward FFT A→B µs | profile MAC A→B µs |
|---|---:|---:|
| 0/1 | 0.872 → 0.555 | 10.203 → 10.762 |
| 0/2 | 0.842 → 0.595 | 20.690 → 19.952 |
| 1/1 | 0.884 → 0.557 | 10.265 → 10.445 |
| 1/2 | 0.831 → 0.580 | 20.649 → 19.763 |
| 2/1 | 0.857 → 0.552 | 10.100 → 10.229 |
| 2/2 | 0.796 → 0.610 | 19.840 → 20.560 |

profile의 각 trial·route에서 Forward FFT는 8,400→4,200회, MAC은 8,400→8,400회다. 호출 감소는 확실하지만 작은 Forward 단계 일부를 줄였고, full-input memcmp·cache lookup·spectrum copy가 추가된다. 이 부가 비용이 정확히 얼마인지는 별도 계측이 없어 수치로 귀속하지 않는다. MAC/inverse/history는 유지된다. B에서 pack이 FFTForward 계측으로 옮겨졌으므로 위 단계 시간의 A/B 차이는 순수 transform 또는 MAC 개선율이 아니다.

#81은 동일 HP의 steady state, 동기식 publication, 200 warmup 제외 후 1,400 callbacks이며 #79의 고정 fade 구간과 다르다. 두 PR의 callback 시간을 직접 나누어 최적화 효과로 제시하지 않는다. #81의 32-sample control miss는 A 3건/B 27건이며, CPU over-budget은 A 1건/B 1건이다. 특히 array stereo B의 22건은 모두 CPU 예산 이내다. r0/control/AB/slot1/B callback 531은 wall 3,252.209µs, CPU 62.292µs다. worker가 없는 이 경로에서도 wall tail이 있으므로 worker 경쟁은 유일하거나 필수적인 설명이 아니다. 코어 이동, OS preemption, VM/runner 간섭 중 어느 것인지는 이 자료만으로 분리할 수 없다. 희소 miss의 증가 자체를 통계적으로 확정된 회귀라고 부르지 않되 원시 건수는 유지한다.

#79·#81 모두 p99가 deadline 안에 있어도 1%보다 드문 miss가 남을 수 있다. 평균 CPU, p99, 최대값, miss count는 서로 대체 가능한 지표가 아니다.

## 다음 독립 가설과 실험

선택: delayed partition MAC의 입력/IR read latency 일부를 bounded prefetch로 숨길 수 있는지 검증한다. 두 partition 앞의 input real/imag와 IR real/imag 시작 주소에 read-only prefetch 네 개만 넣는다. bank 범위와 유효 IR partition을 검사하고, 기존 float 식·누산 순서·partition geometry·history·FFT·fade·worker 정책은 유지한다. 기존 split/clearhead/fusedlead/tile32를 조합하거나 반복하지 않는다.

이것은 memory-bound 확정 진단이 아니다. profile의 큰 MAC 비용에 대한 작고 반증 가능한 개입이다. 하드웨어 prefetch가 이미 충분하거나 주소 생성/명령 비용과 cache pollution이 증가하면 무효 또는 회귀할 수 있다. 한 개의 사전 고정 거리만 검증하고 결과를 보고 여러 거리를 골라내지 않는다. 무효이면 추가 미세 조합보다 동일 publication·IR 노출을 고정한 worker on/off 실험과 cycles/instructions/cache-miss 기반 진단을 우선한다. 실제 audio workgroup/DAW 별도 검증은 남는다.

원래 96kHz 32/64 × 3계열 × mono/stereo, control/profile 각 6 ABBA/BAAB와 6 A/A, 모든 raw callbacks를 유지한다. CPU와 MAC 각 12/12 경로의 95% 하한이 2% 및 A/A 최대 변동폭을 넘고 양쪽 순서 효과가 양수여야 한다. 기존 control 회귀 판정, 5 CTest·66 deadline행·144 realtime route·frozen/fade exact bits·추가 지연 0·callback allocation/deletion·worker 계약을 유지한다. 고정 구간 miss와 CPU over-budget 비증가도 유지한다. integration_eligible와 release_approved는 항상 false다.

### 독립 prefetch 실험 결과 — 채택하지 않음

[Draft PR #96](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/96), 실험 controller `87523297c63db453f15e6a7b0238f9aefe63f8ca`, 생성 candidate `a79d76961c0bba5a7aa09a96a828156409335a54`. [native CI 39053904562](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39053904562)는 2026-10-18 03:19 KST에 SUCCESS로 완료됐다. Artifact 12109529952, ZIP SHA-256 `4b9f93a5386c79ad77db5c9ad608475a8e0f91a4ec7f1c062da3afcf751399c5`다. 이번 runner도 Apple M1 Virtual, 3 cores, 7GB다.

로컬 Python 계약 검사 7개, native exact-method 비교 9,397,248개, ASan/UBSan에서 같은 비교 수를 통과했다. A/B 각각 5 CTest와 66 timing행, frozen bits·추가 지연 0 등의 기존 CAB 계약을 통과했다. 96 trial·294 command 순서, 2,177 manifest 파일, 3,456 raw 요약, 72 통계행의 CI 감사가 통과했다. 이번 조사에서도 artifact 무결성과 3,456개 raw 요약·72개 통계행을 재계산해 동일 판정을 확인했다. 진단 실행 중 2회의 p99 초과 종료도 보존됐으며 삭제하거나 재시도하지 않았다.

최종 판정은 `complete=true`, `cab_contracts_pass=true`, `performance_demonstrated=false`, `deadline_nonincrease=false`, `review_eligible=false`, `integration_eligible=false`, `release_approved=false`다. primary 32/64 경로 CPU 개선 0/12, MAC 개선 0/12다. 기존 통계 기준으로 확정된 control 회귀도 0이므로, 효과 미입증과 확정 회귀를 구별한다.

| 32-sample 구간 | control CPU A→B µs | control wall p99 A→B µs | miss A→B | CPU over-budget A→B | 별도 profile MAC wall A→B µs |
|---|---:|---:|---:|---:|---:|
| 고정 publication | 21.17–38.75 → 21.87–39.95 | 37.67–77.65 → 48.48–104.52 | 9 → 5 | 2 → 1 | 16.17–31.80 → 16.35–33.39 |
| worker/automation | 41.89–74.47 → 45.26–75.62 | 103.29–173.96 → 104.46–183.06 | 123 → 143 | 110 → 131 | 28.74–57.67 → 32.62–60.10 |

분모와 경로 범위는 #79 표와 같다. 고정 구간의 합산 miss 9→5만 보고 성공으로 바꾸지 않는다. original-v1/32/mono는 0→1, stereo는 0→2, expanded-v2/64/mono는 0→4로 증가했기 때문에 경로별 비증가 기준을 통과하지 못했다. 32-sample 고정 CPU 효과량은 여섯 경로 모두 음수(−14.53%~−1.28%)지만 신뢰구간/A/A envelope를 함께 적용한 회귀 확증 기준은 충족하지 않았다.

이 한정된 prefetch 개입의 실패는 DRAM 또는 모든 memory bottleneck의 부재를 증명하지 않는다. 다만 해당 거리의 read hint를 제품에 넣거나 다른 거리들을 사후 탐색할 실측 근거는 없다. 이 후보는 채택하지 않고 Draft 연구 기록으로 유지한다.

다음 단계는 추가 미세 최적화보다 병목 분리 진단을 우선한다. 같은 IR·입력·publication callback·fade 노출을 재생하면서 callback 실행 간격을 동일하게 유지하고, 별도 worker 부하만 켜고 끄는 대조 실험이 필요하다. 가능하면 bare-metal Apple Silicon에서 cycles/instructions/cache-miss와 thread scheduling trace를 함께 관측한다. 이 진단은 원래 acceptance gate를 대체하지 않으며, worker 우선순위·sleep 제거·오디오 지연 증가는 승인된 최적화가 아니다. MAC의 산술 처리량과 cache/wake 영향을 분리한 뒤 다음 제품 최적화를 선정한다.

## 출처

- [PR #79](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/79), controller 740e7b2d4916c8c67a1d0699fca1498cb790799b. [원시 CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38836445224). Artifacts: split 12023886425, clearhead 12024316559, fusedlead 12024302584, tile32 12025279165.
- [PR #81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/81), controller b8595a1cc56e60f4ecba944cd7c01f52c2f09a23. [원시 CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38836500865), artifact 12021354235.
- 측정 경계: Tools/CabNeonAB/run.py, Tests/OriginalCabIntegrationTests.cpp, Tests/CabTimingDiagnostics.h, Source/CabRealtimeProfile.h. #81 측정 경계: Tools/CabFFTSharing/run.py, probe.cpp, prepare.py, README.md.

## 재계산 자료

`reanalyze_scalar.py --artifacts <split/clearhead/fusedlead/tile32 폴더의 상위> --output <json>`으로 #79 raw 요약과 통계를 재현한다. #81 원시 재계산은 `reanalyze_fft.py --archive <원본 ZIP> --output <json>`을 사용한다. 두 스크립트는 기존 실험의 통계 함수를 호출한다. 별도 profile과 control 모집단을 유지한다.

`analyze_cadence.py --artifacts <같은 상위 폴더> --output <json>`은 실행 간격별 탐색 통계를 재현한다. performance 판정에는 연결되지 않는다.
