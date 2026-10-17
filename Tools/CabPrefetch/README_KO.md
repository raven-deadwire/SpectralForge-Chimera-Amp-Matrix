# CAB 병목 조사 — PR #79·#81

2026-10-18 KST. 상태: #79 원시 자료 재계산 완료, #81 대용량 원시 ZIP 확보 대기, 독립 prefetch 후보 검증 준비. 통합·릴리스 승인 없음.

## 판단

96kHz·32 samples의 예산은 333.333µs다. #79에서 반복되는 계산 비용의 우선 조사 대상은 spectral MAC이다. 그러나 이 구간은 pack/unpack도 포함하며, 메모리 대기와 산술 처리량을 분리하는 hardware counter는 없다. deadline miss에는 CPU 예산 초과와 CPU는 짧지만 wall만 긴 사례가 함께 존재한다. 따라서 FFT 단독, DRAM 대역폭 단독 또는 worker scheduling 단독 병목으로 확정할 수 없다.

## 원시 자료 검증

후보별 manifest 2,177개 파일의 크기·SHA-256을 확인하고, 96 trial의 raw CSV에서 3,456개 요약과 72개 통계행을 재계산했다. 네 후보 모두 저장된 분석과 일치했다. 원래 CI의 감사 결과를 단순 인용한 것이 아니라 이번 조사에서 다시 수행했다. 각 후보의 CAB 계약은 통과했고 performance_demonstrated, review_eligible, integration_eligible, release_approved는 모두 false다. deadline_nonincrease도 네 후보 모두 false다.

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

다음은 각 실험 baseline A의 별도 profile 모집단에서 얻은 32-sample 고정 publication 구간의 경로별 trial 중앙값 범위다. control CPU에서 이 값을 빼거나 비율로 나누어 제품의 비용 비중을 계산하지 않는다. 상위 active/fading convolution 시간에는 하위 FFT/MAC이 포함되므로 단계들을 전부 더하지 않는다.

| 실험의 baseline A | spectral MAC (µs/callback) | Forward FFT (µs/callback) | Inverse FFT (µs/callback) |
|---|---:|---:|---:|
| split | 15.58–31.58 | 1.14–1.38 | 1.09–1.26 |
| clearhead | 16.13–31.14 | 1.23–1.37 | 1.18–1.28 |
| fusedlead | 15.32–30.32 | 1.19–1.33 | 1.17–1.22 |
| tile32 | 15.48–32.38 | 1.19–1.33 | 1.12–1.26 |

이 계측 범위에서 MAC은 반복되는 큰 단계다. 동일 profile 관찰이 FFT 횟수만 줄이는 접근의 한계를 지지하지만, 이 수치를 #81의 다른 workload에 그대로 대입한 Amdahl 상한으로 쓰지는 않는다.

## miss의 두 형태

32 samples의 A/B control에서 split은 218건 중 178건, clearhead는 205건 중 174건, fusedlead는 200건 중 150건, tile32는 1,509건 중 1,465건의 wall miss가 같은 콜백의 CPU 예산 초과를 동반했다. 이는 연산 실행 시간이 긴 사례가 많다는 뜻이며, 캐시 stall·코어 이동·실행환경 간섭을 배제하는 근거는 아니다.

split의 original-v1 stereo r0/control/AB/slot2/B, callback 455는 wall 15,909.5µs, CPU 85.792µs다. expanded-v2 stereo r0/control/AB/slot3/A, callback 203은 wall 11,251.916µs, CPU 85.125µs다. 이들은 MAC 산술량만으로 설명할 수 없는 wall 지연 사례다. 반면 original-v1 stereo r3/control/AB/slot1/A, callback 777은 wall 18,255.792µs와 CPU 11,076.333µs가 함께 크다. 극단값을 삭제하지 않았다.

worker/automation 구간과 고정 publication 구간은 worker 실행 여부뿐 아니라 요청 변화, 캐시/커널 상태, fade 노출, 콜백 사이 sleep과 순서가 다르다. 두 평균의 차이를 worker의 인과 비용으로 해석하면 안 된다. profile CSV의 MAC 호출 일정은 고정 구간에서 같지만 비동기 구간은 별도로 검토해야 한다.

## 반복 미세 최적화의 실패 해석

- split은 주소 계산과 wrap 분기를 줄이지만 전체 partition MAC 수를 줄이지 않는다. 고정 구간 32-sample CPU 효과는 −5.27%~+3.95%이며 신뢰구간/A/A envelope를 통과하지 못했다.
- clearhead는 작은 FFT 입력 버퍼 초기화만 줄인다. FFT·MAC·inverse·fade 연산량은 유지된다. CPU 효과 −17.45%~+5.53%는 효과 입증이 아니다.
- fusedlead는 한 번의 tail 복사와 선행 partition만 결합한다. delayed partition의 반복 MAC은 유지된다. CPU 효과 −3.71%~+9.15% 역시 기존 기준 미달이다.
- tile32는 출력 누산 버퍼가 이미 작은 상태에서 partition 순회를 반복한다. 동일 runner 내 고정 original-v1 stereo CPU 효과 −29.33%, 95% CI [−41.20%, −18.30%], A/A envelope 12.51%로 회귀가 확정됐다. worker/automation의 B miss 1,414건을 단순 잡음으로 숨길 수 없다. 추가 주소·순회 비용과 접근 패턴 악화는 소스에 근거한 설명 후보이며 cache miss 증가를 직접 측정한 것은 아니다.
- A/A CPU envelope가 수십 %인 경로가 존재한다. 아주 작은 최적화 효과가 있다고 하더라도 현재 반복 설계에서 확증할 수 없는 경우와, 실제로 느려진 경우를 구별해야 한다. threshold를 낮추거나 유리한 route만 선택하지 않는다.

## PR #81의 해석 경계

네이티브 로그의 complete=true, apple_silicon_performance_demonstrated=false를 확인했다. 고정 소스는 c2263e4c90bffb9a56cc41285ebf8207e641b3aa, JUCE는 d6181bde38d858c283c3b7bf699ce6340c050b5d다. 실행기는 모든 profile route에서 A 8,400회/B 4,200회 Forward FFT와 동일 MAC 호출 일정을 assert하며 성공했다. 이 호출 감소 확인과 CPU 성능 향상은 별개다. 후보는 full-input memcmp·cache lookup·spectrum copy를 추가하고 MAC/inverse/history는 그대로 유지한다.

#81은 동일 HP의 steady state, 동기식 publication, 200 warmup 제외 후 1,400 callbacks를 쓰며 #79의 고정 fade 구간과 다르다. 또한 B에서 pack이 FFTForward 계측으로 이동한다. 따라서 A/B stage 시간 차이를 순수 FFT 또는 MAC 변화로 읽지 않는다. 이 보고서 작성 시 563MiB ZIP은 연결 도구의 512MiB 한도를 초과했고 브라우저 다운로드도 시간 초과여서, #81의 CPU·p99·miss·단계 시간 원시 수치는 아직 재계산하지 않았다. 숫자를 추정해 채우지 않는다.

## 다음 독립 가설과 실험

선택: delayed partition MAC의 입력/IR read latency 일부를 bounded prefetch로 숨길 수 있는지 검증한다. 두 partition 앞의 input real/imag와 IR real/imag 시작 주소에 read-only prefetch 네 개만 넣는다. bank 범위와 유효 IR partition을 검사하고, 기존 float 식·누산 순서·partition geometry·history·FFT·fade·worker 정책은 유지한다. 기존 split/clearhead/fusedlead/tile32를 조합하거나 반복하지 않는다.

이것은 memory-bound 확정 진단이 아니다. profile의 큰 MAC 비용에 대한 작고 반증 가능한 개입이다. 하드웨어 prefetch가 이미 충분하거나 주소 생성/명령 비용과 cache pollution이 증가하면 무효 또는 회귀할 수 있다. 한 개의 사전 고정 거리만 검증하고 결과를 보고 여러 거리를 골라내지 않는다. 무효이면 추가 미세 조합보다 동일 publication·IR 노출을 고정한 worker on/off 실험과 cycles/instructions/cache-miss 기반 진단을 우선한다. 실제 audio workgroup/DAW 별도 검증은 남는다.

원래 96kHz 32/64 × 3계열 × mono/stereo, control/profile 각 6 ABBA/BAAB와 6 A/A, 모든 raw callbacks를 유지한다. CPU와 MAC 각 12/12 경로의 95% 하한이 2% 및 A/A 최대 변동폭을 넘고 양쪽 순서 효과가 양수여야 한다. 기존 control 회귀 판정, 5 CTest·66 deadline행·144 realtime route·frozen/fade exact bits·추가 지연 0·callback allocation/deletion·worker 계약을 유지한다. 고정 구간 miss와 CPU over-budget 비증가도 유지한다. integration_eligible와 release_approved는 항상 false다.

로컬 Python 계약 검사 7개 통과. native exact-method, ASan/UBSan, 전체 CAB 및 Apple 성능 결과는 CI 실행 후에만 기록한다.

## 출처

- [PR #79](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/79), controller 740e7b2d4916c8c67a1d0699fca1498cb790799b. [원시 CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38836445224). Artifacts: split 12023886425, clearhead 12024316559, fusedlead 12024302584, tile32 12025279165.
- [PR #81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/81), controller b8595a1cc56e60f4ecba944cd7c01f52c2f09a23. [원시 CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38836500865), artifact 12021354235.
- 측정 경계: Tools/CabNeonAB/run.py, Tests/OriginalCabIntegrationTests.cpp, Tests/CabTimingDiagnostics.h, Source/CabRealtimeProfile.h. #81 측정 경계: Tools/CabFFTSharing/run.py, probe.cpp, prepare.py, README.md.
