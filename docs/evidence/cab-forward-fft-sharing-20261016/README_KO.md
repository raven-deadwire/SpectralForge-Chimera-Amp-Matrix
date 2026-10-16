# CAB FFT 공유 조사 — 2026-10-16

**결론: 동일 FFT 입력의 중복 실행을 확인했다. 격리 후보는 출력 계약을 보존했지만, 전 경로 성능 개선은 입증하지 못했다. 제품 통합/릴리스 보류.**

고정 제품: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`. 후보 제어기: `b8595a1cc56e60f4ecba944cd7c01f52c2f09a23`.
[전체 경로 추적·후보 설계·재현](../../../Tools/CabFFTSharing/README_KO.md).
[새 연구 Draft PR #81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/81).

## 확인한 중복과 공유 제외 조건

- 같은 CAB의 Mic A/B가 같은 HP 계수와 실제 상태로 계속 처리될 때 동일 forward FFT가 반복된다.
- 3 rigs/6 mics 정상 경로에서 callback당 실제 forward FFT가 **6 → 3**으로 감소했다. MAC 호출 일정은 12/12 경로 모두 동일하다. 이는 MAC이나 inverse FFT의 50% 감소를 뜻하지 않는다.
- 원본 버퍼·HP 설정이 같아도 Mic B 재활성화 이후 HP 이력과 부분 블록 누적이 다르면 공유할 수 없다.
- active/fading은 같은 HP 출력에서 복사되지만 새 커널과 이전 커널의 누적 상태가 다를 수 있다.
- Dual/Matrix의 다른 CAB는 amp/crossover 결과가 달라 rig 간 공유를 제외했다. 개인 IR/legacy fallback은 원래 JUCE 처리를 유지한다.

대표 stress 경로(original-v1, max32, stereo)의 정확 입력 cache 관찰:

| 구간(callback index) | 재사용 hit | 원래 FFT miss |
|---|---:|---:|
| 같은 HP와 이력(1–99) | 453 | 453 |
| B 음소거(250–349) | 0 | 444 |
| B 재활성화 직후(350–449) | 0 | 909 |
| B HP 다름(450–649) | 0 | 1,794 |
| 설정 복귀, 이력 다름(650–699) | 0 | 447 |
| 양쪽 reset 이후(901–949) | 222 | 222 |
| A publication/fade를 포함한 구간(950–1299) | 1,272 | 3,150 |
| B publication/fade를 포함한 구간(1300–1649) | 0 | 4,419 |

후보는 FFT time buffer 전체 바이트가 같은 경우만 spectrum을 복사한다. ring·cursor·FFT plan·과거 partition 합·inverse·overlap·fade·필터와 gain/delay는 각각 독립이다. scratch는 CAB audio owner가 prepare에서 할당하고 callback 시작마다 무효화한다. callback cache path에는 동적 할당/해제·잠금·worker 접근·다음 블록 대기가 없다.

## 정확성·무결성

- 양쪽 모두 원래 144 realtime 경로, 2,645,088 frozen floats, 388,800 fade floats, immutable spectra 및 latency=0 검사 PASS.
- 추가 76경로, **92,054,040 floats**의 A/B 전체 출력 비트 일치. 3 모델 계열, 17/32/64/128/256/512 최대 블록, mono/stereo, partial blocks, B mute/wake, 서로 다른 HP, reset, publication/fade, gain/delay/polarity, bypass 및 tail 포함.
- 개인 IR fallback, 우측 무음, 서로 다른 processor의 동시 처리 포함. 모든 측정 callback에서 감시한 C++ new/delete/정렬 new 호출 0.
- 2,767 manifest 파일의 크기/SHA-256, 출력 전체 예상 길이·비트 일치, 1,843,200 raw timing callbacks와 quartet 통계 재계산 PASS. `local-audit.json` 참조.
- 별도 GCC ThreadSanitizer 빌드에서 두 processor를 동시에 실행한 stress 시험은 race report 없이 종료했고 두 출력 모두 원래 A의 비트와 일치했다(`tsan.json`). 동시 IRLibrary worker publication stress나 전체 race-free 증명을 의미하지 않는다. 직접 C malloc interception은 별도 미검증이다. 전체 PluginProcessor/비동기 worker/DAW 수용을 대신하지 않는다.

## Linux x86_64 예비 처리시간

GCC 13.3.0, JUCE 8.0.8, Release. A/B의 probe 컴파일 명령은 variant 경로·매크로·target 이름을 정규화하면 동일하다. 모든 빌드/정확성 검사 종료 후 한 환경에서 순차 측정했다. 96kHz, 32/64 samples, 각 3 rigs/6 mics. **steady matched-HP workload만** 반복 성능 판정 모집단이다.

control/profile 각각 6 ABBA/BAAB quartet와 교차 동일 A 바이너리 A/A, 총 96 trials. trial·route마다 1,600 callbacks 중 앞 200회 warmup을 보존하되 통계에서는 제외했다. CPU mean은 CAB 전체의 thread CPU다. 개선율 양수는 빨라짐이며 95% bootstrap 하한이 2%와 최대 A/A 효과를 모두 넘어야 통과한다.

| 계열 | Samples | Ch | CPU 개선 추정 | 95% 구간 | A/A 최대폭 | 통과 |
|---|---:|---:|---:|---:|---:|---|
| original-v1 | 32 | 1 | +17.10% | +10.00 ~ +28.32% | 4.90% | PASS |
| original-v1 | 32 | 2 | +9.94% | +8.42 ~ +11.61% | 11.77% | 미입증 |
| original-v1 | 64 | 1 | +14.07% | +9.71 ~ +18.69% | 30.86% | 미입증 |
| original-v1 | 64 | 2 | +11.14% | +7.75 ~ +14.91% | 8.70% | 미입증 |
| expanded-v2 | 32 | 1 | +7.38% | -2.83 ~ +13.57% | 7.50% | 미입증 |
| expanded-v2 | 32 | 2 | +11.25% | +5.76 ~ +19.02% | 10.03% | 미입증 |
| expanded-v2 | 64 | 1 | +11.60% | +8.44 ~ +14.39% | 9.23% | 미입증 |
| expanded-v2 | 64 | 2 | +8.88% | +4.51 ~ +13.23% | 12.45% | 미입증 |
| array-v3-6x10 | 32 | 1 | +7.52% | +0.63 ~ +12.21% | 18.72% | 미입증 |
| array-v3-6x10 | 32 | 2 | +6.54% | +0.82 ~ +11.82% | 10.18% | 미입증 |
| array-v3-6x10 | 64 | 1 | +13.95% | +12.11 ~ +15.59% | 15.96% | 미입증 |
| array-v3-6x10 | 64 | 2 | +5.45% | -1.91 ~ +11.01% | 13.46% | 미입증 |

- CPU 개선 통과: **1/12**. 사전 envelope를 넘는 control CPU/wall p99 회귀 표시는 0건이나 동등성/비열등성 보장은 아니다.
- cache lookup/복사 비용을 포함한다. FFT 호출 감소만으로 callback CPU 개선을 확정하지 않는다.
- B는 spectrum packing을 forward 계측 범위로 이동시켰다. stage 시간의 A/B 차이를 순수 FFT/MAC 최적화 수치로 해석하지 않는다.
- 다른 HP/재활성화 직후의 miss-heavy 경로는 정확성만 검증했으며, 그 CPU overhead의 paired 성능 판정은 남아 있다.

## 제외한 최초 로컬 모집단

첫 수집에서는 비교 시점 이후 일부 저장된 float 파일이 달라져 사후 출력 감사에 실패했다. 원인은 확정하지 않았다. 최초 전체 모집단은 최종 성능·정확성 근거에서 제외했고 선택적으로 trial/이상치를 삭제하지 않았다. 소스·후보·빌드·일정·통계 기준을 유지하고 별도 임시 위치에서 전체를 다시 수집했다. 이 문서와 첨부 JSON은 재수집 후 완전 감사를 통과한 모집단만 사용한다. 최초 수집에서 언급한 3/12는 최종 결과가 아니다.

## Apple Silicon 및 통합 결정

[전용 macOS arm64 실행 38836500865](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38836500865)은 현재 QUEUED이며 native 결과가 없다. Linux 수치를 Apple Silicon의 개선 근거로 사용하지 않는다. 실제 하드웨어·Xcode 16.4·컴파일 명령·source manifest·후보 patch·실패를 포함한 전체 raw population을 CI artifact로 보존하도록 구성했다.

- PR #73/S8T의 통합 보류 유지. 완료된 비교를 반복하지 않았다. 이번은 별도 exact-input FFT 재사용 후보다.
- 이번 후보도 **통합/릴리스 보류**, `release_approved=false`.
- 남은 조건: Apple Silicon 측정 완료, miss-heavy/publication 경로의 paired 비용 평가, allocator/race 검증 보강, 제품 전체 및 실제 DAW/물리 인터페이스 수용.

`report.json.gz`에 96 trial/전체 통계, `manifest.json.gz`에 로컬 원시 자료의 파일 해시, `source-manifest.json`에 고정 소스 해시, `.patch` 파일에 후보 변경을 보존한다. 이 폴더는 원시 float/CSV 파일 자체를 포함하지 않으며 JSON만으로 raw를 재감사할 수 있다고 주장하지 않는다. 원시 자료의 완전 재감사는 수집 디렉터리와 `Tools/CabFFTSharing/audit.py`가 필요하다. Native CI artifact는 성공/실패 모두 보존한다.
