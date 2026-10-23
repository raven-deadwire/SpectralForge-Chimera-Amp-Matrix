# CAB 계산 비용·스케줄링 조사 — 2026-10-19

## 결론

PR #101의 17회 miss를 모두 스케줄러 문제로 설명할 수 없다. 같은 callback으로 짝지으면
10회는 thread CPU도 예산을 넘고, 7회는 wall만 넘는다. CPU 초과는 실행 중 비용이
충분히 컸다는 증거이며 순수 DSP 산술 비용만의 증거는 아니다. wall-only 역시 OS·VM 대기,
clock envelope, interrupt 등을 구분하는 scheduler trace 없이 특정 원인으로 확정하지 않는다.

원인 하나와 제품 수정 효과는 아직 입증되지 않았다. 제품 FFT/MAC, prefetch, worker priority,
IR 상태, fade, deadline 예산을 바꾸지 않았다. 변경 범위는 compile-time opt-in 계측과
독립 실행 실험·검증 도구다. 이 PR은 성능 수정 승인이나 AAX/실장비 출시 승인이 아니다.

## 원본 증거

| 증거 | 확인 결과 |
|---|---|
| PR #101 run 39127354198, artifact 12137142606 | manifest 107개, 후보 66 routes 재검증 |
| 96k/32/stereo expanded-v2 | wall p99 348.875µs, CPU p99 284.834µs, 17/1600 miss |
| 동일 callback 분류 | CPU 동반 10, wall-only 7 |
| worker/automation | 13/1200 miss, sleep 직후 6/150 대 나머지 7/1050 |
| worker 정지 구간 | 4/400 miss, 모두 CPU 동반. publication 요청 직후라고 볼 수 없는 위치 |
| PR #96 run 39053904562, artifact 12109529952 | manifest 2177개, 96 trials, 3456 raw summaries, 72 통계 재계산 일치 |
| 기존 prefetch 결정 | 성능 개선 미입증·deadline 비증가 실패 유지; 후보 재실행 없음 |

각 p99는 독립적으로 정렬한 값이다. wall p99 표본 351의 CPU는 146.625µs다.
분석 코드와 exact CSV는 이 디렉터리에 보존했다. 더 자세한 원시 archive 식별자는
`Tools/CabScheduling/README_KO.md`에 있다.

## 실행한 Linux 검증

이 환경에는 pkg-config가 없어 전체 CMake configure가 막혔다. JUCE 8.0.8의 정확한
commit `d6181bde38d858c283c3b7bf699ce6340c050b5d` 모듈을 GCC 13.3, Release로 직접
컴파일·링크했다. 실제 CAB 구현을 실행했지만 공식 CMake/CTest 환경과 macOS 재현은 아니다.

- 기존 integration entry 네 조합: 원래 threshold 그대로 66 timing routes 통과.
- realtime 회귀 144 routes, direct-FIR 8 six-mic routes 통과.
- frozen reference float 2,645,088개, fade sample 388,800개 bit 일치.
- reset/reprepare, immutable spectra, stale/six-ready/reuse, 50ms fade, zero-added-latency 통과.
- trace 계약 Release 및 ASan/UBSan 통과. 누수 검사는 이 실행에 포함하지 않았다.
- timing parser 20개, 독립 attribution verifier 24개, 새 join/연관 분석 7개 테스트 통과.

첫 portable original gate는 pilot의 오프라인 검증 종료와 잠시 겹쳤다. 이 실행은 별도
예비 gate 기록으로 보존하고 인과 효과 추정에 사용하지 않았다. 아래 반복 측정은 빌드,
예비 gate 및 pilot 검증이 종료한 다음 순차 수행했다.

| 데이터셋 | trials / callbacks | 의미 |
|---|---:|---|
| portable-repeats | 24 / 38,400 | observer·cadence 각각 ABBA/BAAB. 일부 cadence 비교는 출력 이력이 달라 동등 노출 아님 |
| portable-canonical | 16 / 25,600 | reset-only 진단. 한 quartet 출력 불일치; 충분하지 않은 통제로 보존 |
| portable-canonical-v2 | 16 / 25,600 | 측정 밖 reprepare·동일 IR 설치로 모든 quartet의 정지 구간 출력 일치 |

56회 전체의 원시 구조 검증은 완료했다. 이는 성능 gate 통과를 뜻하지 않는다. 계측 자체가
CPU 비용을 추가한다. 첫 observer worker 구간은 A/off 6 대 B/stages 90 miss, 다음은
22 대 25였으며, 두 정지 구간의 방향도 2 대 9와 11 대 3으로 바뀌었다. 계측 결과를
원래 callback 비용으로 환산하거나 clock 오버헤드를 일괄 차감하지 않는다.

재현용 source checkpoint는 아래처럼 연결된다. 로컬 컴파일 metadata에 기록한 commit과
GitHub 저장 commit의 **전체 tree SHA가 일치**한다. 커밋 작성 시각·메시지 때문에 commit
SHA만 다르다. 첫 24회는 공개 `f8fac565…`의 C++ 소스다.

| 데이터셋 | 컴파일 시 로컬 checkpoint | 동일 tree의 GitHub commit |
|---|---|---|
| reset-only | `71cc212be0972ae931ae8673f6107023fb3bf022` | `be79c5e86e935abc37bc269c9897889626946ca8` |
| canonical-v2 | `fe1ff0baad32792a1696f3c02fa1a545ffd656ab` | `751d37d17fa8f070d16b6da0224001e7c0e6d26c` |

전체 portable·원본·새 기존 gate 원시 증거 묶음은
`CAB_CPU_Scheduling_Evidence_20261019.zip` (262,842,930 bytes,
SHA256 `9c7c5aa183d740a167b37377536dde72c5aaadcaf8b13d71607eb2838dd69da4`)이다.
683개 파일과 내부 SHA256 manifest, 실행·컴파일 명령, 실패·초기 실험을 포함한다.

## 상태 고정 후 cadence 결과

A=기존 sleep 패턴, B=연속 실행. 각 칸은 두 실행을 합친 miss 개수이며 괄호는 표본 수다.
서로 다른 trace level 및 구간을 합쳐 효과를 추정하지 않는다. 고정 구간은 양 arm 모두
연속 실행이므로 앞 구간의 이력·실행 환경 잔여 영향 검사로만 읽는다.

| trace / 순서 | worker A | worker B | 고정 구간 A | 고정 구간 B |
|---|---:|---:|---:|---:|
| off / ABBA | 2 (2400) | 1 (2400) | 0 (800) | 3 (800) |
| off / BAAB | 7 (2400) | 9 (2400) | 1 (800) | 5 (800) |
| stages / ABBA | 47 (2400) | 19 (2400) | 10 (800) | 22 (800) |
| stages / BAAB | 44 (2400) | 8 (2400) | 26 (800) | 5 (800) |

trace-off worker 비교는 일관된 개선 방향이 없다. trace-on에서 감소하더라도 계측 영향과
비동기 publication/fade 노출이 섞여 제품 scheduling 수정의 근거가 되지 않는다.

각 callback의 FFT forward/inverse, spectral MAC, active/fading convolution, fade mix,
kernel swap 및 worker publish/wait를 별도로 기록했다. 최종 trace-on 8회에서 MAC CPU와
miss의 point-biserial 상관 범위는 worker 0.205–0.532, 고정 구간 0.027–0.648이었다.
FFT·fade·swap의 연관은 실행별 편차가 컸다. 이는 추적할 계산 구간을 보여주지만 PR #101의
과거 miss 원인을 역으로 입증하지 않는다. 중첩 stage 시간을 더하지 않는다.

worker timeout wait 42회의 return overshoot는 79.080–818.925µs였다. notify로 끝난 wait는
timeout 계산에서 제외했다. 이 값에는 sleep 구현과 OS·VM 영향이 포함될 수 있으며
실제 runnable→running 지연은 아니다. 원시 publication overlap·fade·kernel accept별
miss 분모와 상관계수는 `portable-summary.json`과 전체 trial associations에 보존했다.

## 새 macOS 기존 gate

run [39225842684](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39225842684),
job 117972124194, 후보 `3fbf54749908be5a47ff120bdcd6c3eafb1ef06b`, artifact 12175540135.
SHA256 manifest 107개와 후보 66 routes를 다시 확인했다. Apple M1 Virtual, 3 cores / 7GB,
macos-15이며 물리 오디오 장치는 없다. 이 기존 workflow의 baseline은 `aaa1330d…`로,
별도 상세 attribution 실험의 frozen PR #101 `1fa3b0e…`와 구분한다.

| 후보 route | 예산 µs | wall / CPU p99 µs | miss | 동일 callback CPU 동반 / wall-only |
|---|---:|---:|---:|---:|
| 96k/32/stereo expanded-v2 | 333.333 | 213.667 / 207.625 | 7/1600 | 6 / 1 |
| 96k/64/mono expanded-v2 | 666.667 | 674.833 / 681.000 | 16/1600 | 16 / 0 |

후보 기존 5 CTest 중 extended timing 1개가 실패했다. 다른 네 CTest는 통과했다.
이번 집중 route의 p99 통과는 이전 실패를 지우거나 제품 수정 효과를 증명하지 않는다.
실패 route 이동 및 CPU 동반 miss는 scheduler-only 결론을 지지하지 않는다.

## 상세 macOS 반복 실험

run [39225842790](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39225842790),
source `3fbf54749908be5a47ff120bdcd6c3eafb1ef06b`. ABBA와 BAAB는 각각 별도 VM allocation이며,
서로 합쳐 paired 효과를 계산하지 않는다. canonical-v2 옵션 추가 전 소스이므로 아래 결과를
새 상태 고정 실험으로 부르지 않는다.

ABBA artifact 12176030441의 SHA256은
`0cb456e9ae3d22836e1cef11cb6669bb256b8aebe64d7d2f691fcd06d786424c`다.
723개 manifest 파일을 검증했고, 68개 route associations를 원시 CSV에서 다시 계산해
CI 결과와 일치함을 확인했다. 13개 실험 population과 모든 independent verifier가 완결됐다.
job의 failure는 **frozen PR #101의 원래 extended timing gate 실패**를 보존한 결과다.

- frozen `1fa3b0e…`의 96k/32/stereo: wall p99 **414.166µs**, CPU p99 **437.042µs**,
  23/1600 miss, 23개 모두 같은 callback CPU도 예산 초과. 원본 실패 재현이다.
- 같은 allocation의 current hooks-off 5 CTest는 통과했다. 이것은 한 번의 시간 순서 비교이며
  제품 DSP 수정이 없어 성능 개선으로 해석하지 않는다. 별도 기존 workflow에서는 current도
  다른 route에서 실패했다.
- hooks-on realtime semantic 회귀와 trace 계약도 통과했다.
- observer focused worker 구간 A/off 두 실행은 2·0 miss, B/stages는 60·11 miss였다.
  stopped-worker 출력 checksum도 quartet 전체가 일치하지 않아 동등 workload의 속도 효과로
  해석할 수 없다.
- cadence worker 구간 off A/legacy 17/2400 대 B/continuous 7/2400,
  stages A 308/2400 대 B 5/2400. 그러나 trace-on continuous 한 실행에는 **kernel accept와
  fade가 모두 0**이었다. 실제 publication/fade 노출 감소가 있으므로 sleep 제거의 제품 효과로
  주장하지 않는다. 각 phase별 분모·checksum·slot 상태를 `mac-abba-audit.json`에 보존했다.

`02-B`의 expanded-v2 / 96k / 32 / stereo에서 원인 구분을 보여주는 표본:

| callback | wall / CPU µs | 관측 |
|---|---:|---|
| 380 | 23,899 / 187.625 | kernel-swap 구간 wall 23,726 대 CPU 15.417; 실제 kernel accept 0, sleep 직후 아님 |
| 641 | 1,439 / 1,439.666 | spectral MAC CPU 합 367.206; sleep 직후, fade·kernel accept 0 |

첫 표본은 계산량만으로 설명하기 어려운 실행 지연, 둘째는 MAC 구간의 실행 비용이 예산을
넘은 관측이다. 두 표본은 이 trial의 wall-only 최대 및 CPU 최대를 선택한 예시다.
계측·clock accounting 영향이 있으므로 원본 제품의 순수 비용이나 단일 원인으로 일반화하지
않는다. CPU endpoint 차이는 stage 사이에 가산적이지 않을 수 있고 `cab_process` 같은
재귀 scope는 같은 이름도 중첩된다. 예시 원시 합은 `mac-abba-examples.json`에 있다.

동일 allocation의 arithmetic/memory/clock 대조군에도 wall-only miss가 있었다. 시계 대조군의
CPU 동반 miss도 있어, thread CPU 초과를 모두 순수 DSP 계산으로 부르는 것은 부정확하다.
이 대조군은 환경·계측 민감도 증거이며 CAB 시간에서 빼는 보정값이 아니다.

ABBA focused stages-on 6회에서 각 실행별 CPU 시간 대 miss의 point-biserial 범위는 아래와
같다. miss가 0인 구간의 상관은 정의되지 않아 범위에서 제외했다. 반복을 합친 유의확률이나
인과 계수로 해석하지 않는다.

| CPU stage | worker 구간 | worker 정지 구간 |
|---|---:|---:|
| FFT forward | 0.106–0.497 | 0.203–0.543 |
| FFT inverse | 0.098–0.382 | 0.160–0.484 |
| spectral MAC | 0.057–0.534 | 0.168–0.614 |
| kernel swap | −0.006–0.301 | 0.134–0.351 |
| fade mix | −0.024–0.214 | 0.102–0.369 |

observer `02-B`·`03-B`의 worker publication 직접 overlap은 각각 0 callbacks였다.
kernel accept가 있던 4·2 callbacks에는 miss가 없었다. 이것은 해당 표본의 직접 동시 발생
결과일 뿐, 간접 cache/worker 영향이나 다른 실행의 publication 비용을 배제하지 않는다.
focused stages-on worker lifetime의 timeout wait 88회 overshoot는 0.531–155.299ms였다.
측정 밖 준비·drain 중 wait도 포함하며, callback과의 시간 관계는 별도로 join했다.
이 큰 wait-return 편차도 OS runnable trace를 대체하지 않는다.

BAAB artifact 12176677291도 완료됐다. SHA256은
`74c157572e609f1cbe2000a4bc4efd261ef08dc1e938551cc26e9da6d98fc493`이며,
manifest 723개 및 68개 route associations를 재검증·재계산했다. 13 populations가 완결됐고
frozen/current 각각의 기존 5 CTest, trace 계약, hooks-on semantic 회귀가 모두 통과했다.

| BAAB의 96k/32/stereo | wall / CPU p99 µs | miss | CPU 동반 / wall-only |
|---|---:|---:|---:|
| frozen PR #101 | 117.959 / 120.708 | 0/1600 | 0 / 0 |
| current hooks-off | 183.833 / 180.917 | 5/1600 | 4 / 1 |

동일 frozen 소스가 ABBA allocation에서는 실패, BAAB allocation에서는 통과했다.
순서 자체의 효과로 단정할 수 없으며 두 VM 수치를 합쳐 평균 개선율을 만들지 않는다.

BAAB observer의 worker 구간은 A/off 11/2400 대 B/stages 8/2400이었다. ABBA의
A/off 2 대 B/stages 71과 방향이 다르다. cadence worker 구간은 off legacy 1/2400 대
continuous 0/2400, stages legacy 1162/2400 대 continuous 7/2400이었다. 그러나 앞서
확인한 비동기 publication/fade 노출 차이와 state/output checksum 차이가 있어
sleep 제거·prefetch·QoS 변경의 성능 증거로 사용할 수 없다. 모든 phase별 데이터는
`mac-baab-audit.json`에 보존했다.

원시 macOS archive는 `CAB_MacOS_ABBA_3fbf547.zip` (133,341,412 bytes)와
`CAB_MacOS_BAAB_3fbf547.zip` (132,685,929 bytes)이다. 독립 재계산 명령:

```sh
python Tools/CabScheduling/audit_native.py --input /path/to/extracted-artifact \
  --output /path/to/audit.json
```

검증 범위는 명시된 source SHA에 한정한다. 최종 보고·재현 명령 추가 후 head의 CI는
후속 실행으로 구분한다. optional canonical-v2는 Linux에서 실행했으며, macOS canonical
replay는 재현 명령을 준비한 상태다. 위 macOS 데이터를 canonical replay 결과로 대체하지
않는다. 제품 수정 효과·순수 scheduler latency·실장비 dropout은 여전히 미입증이다.

## 실제 장비가 필요한 검사

CI `deadline_miss`와 물리 인터페이스 dropout을 별도 결과로 관리한다. 현재 physical-device
dropout은 **미검사**이며 0회로 기록하지 않는다. 실제 Mac에서 source/binary hash, OS/CPU,
interface·driver, DAW/format, 동일 DI/IR/automation을 고정하고 96k/32 및 64를 반복한다.
장치 xrun/host overload와 loopback 녹음 discontinuity를 각자 수집한다. System Trace에서
callback·worker의 runnable/running/blocked 구간을 기록해 worker wait-return proxy와
대조해야 순수 스케줄링 지연을 검증할 수 있다. 새 원인이 입증될 때만 최소 제품 수정과
같은 원래 deadline·출력·IR·실시간 안전성 회귀를 진행한다.
