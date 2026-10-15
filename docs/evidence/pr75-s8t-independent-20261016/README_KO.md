# PR75 S8T 독립 성능 비교 — HOLD

2026-10-16 KST. 원본 A와 정확한 S8T 후보를 동일한 Apple Silicon 러너에서 비교하고, 내려받은 원시 자료와 통계 판정을 독립적으로 재계산했다.

**성능 개선: NOT DEMONSTRATED. 제품 통합 적격: False. 릴리스 승인: false.**

고정 구간 CPU mean의 trial 중앙값은 12경로 모두 S8T가 더 높았다. 그러나 사전 A/A 기준을 넘는 확정 회귀도 없었다. 따라서 **개선 미입증**이 결론이며, 동등성·비열등성까지 증명한 결과는 아니다. 이번 실행의 A/A 최대 변동폭은 CPU 기준 경로별 13.35~66.10%, MAC 기준 8.87~47.86%였다. 다른 러너에서 실행한 PR74의 수치와 직접 비율을 계산해 S8T 개선율을 주장하지 않는다.

## 실행과 소스

- [검증 PR #76](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/76)
- [네이티브 실행](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38775107197)
- 실행 컨트롤러: `025bb927e8e4fc7177b476ccea00dc12a49b0b41`. 이번 근거 보존 커밋은 실행한 도구와 제품 소스를 변경하지 않는다.
- 원본 A: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`. S8T 실험 B: `20c2be7ab2bf4f72683dbf163055b8a7467901e3`.
- PR75 패치 원본: `2df0b1e7c1a1e7022ed77a577bf0effb99a672fa`. 패치 SHA-256: `7aaef8ab7367c44083009121c6b2ac66d5b6378bc25b731b2b3720f5afbfcde0`.
- 생성된 S8T 헤더 SHA-256: `f95f02d204497aa8c1f18bcb7ad66954b2174780db2570d4a47bd7dba3dbeba4`.
- 원시 ZIP SHA-256: `f2916a4438cf4f9c57cf40312acafabe24941cca205e1df105254b6700dbf5cf`.
- A/B의 제품 소스 차이는 `Source/ModeledCabConvolution.h` 하나이며 두 체크아웃은 측정 전후 clean 상태다. 원래 테스트·기준·IR·JUCE·빌드 옵션은 유지했다.
- 환경: Model Name: Apple Virtual Machine 1 /       Model Identifier: VirtualMac2,1 /       Chip: Apple M1 (Virtual) /       Total Number of Cores: 3 /       Memory: 7 GB.
- 컴파일러: Apple clang version 17.0.0 (clang-1700.0.13.5); Xcode 16.4 / Build version 16F6.
- A/B probe 컴파일 명령은 경로를 정규화한 뒤 동일함을 확인했다. 모든 빌드를 마친 후 순차 측정했다.

## 측정 범위와 사전 판정 기준

96 kHz·32/64 samples, mono/stereo, original-v1·expanded-v2·array-v3-6x10의 12경로. 매 경로마다 1,600회(비동기 worker/automation 1,200 + 고정 six-slot publication 400) 콜백을 보존했다. control/profile별 6회 ABBA/BAAB와 교차 배치한 6회 A/A를 수행했다. A/A는 두 논리 위치에 동일 A 바이너리를 사용한다.

총 96 trial, 1,843,200 콜백이다. spectral MAC은 계측을 켠 별도 실행의 wall 구간 시간이며, thread CPU와 wall p99는 계측을 끈 **CAB 처리 전체(3 rigs/6 mics)** 기준이다. 앰프·FX를 포함하는 전체 PluginProcessor, DAW·오디오 드라이버 콜백을 측정한 결과는 아니다. 두 계측 모드를 합치거나 차감하지 않았다.

인과 비교의 주 모집단은 비동기 worker가 중단된 고정 publication 구간이다. 각 quartet의 log-ratio로 개선율을 구하고 6개 quartet 단위로 10,000회 bootstrap한다. 개선율의 95% 하한이 2%와 최대 A/A 변동폭을 모두 넘고, ABBA/BAAB 양쪽 순서에서 개선되어야 한다. 12경로의 callback CPU 및 MAC이 모두 통과하고, control CPU/wall p99의 확정 회귀가 없어야 전체 성능 개선으로 판정한다. 단일 할당 내 결과이며 모든 Apple Silicon 기기의 보장이나 다중비교 보정 결과는 아니다.

- CPU 개선 통과: 0/12.
- spectral MAC 개선 통과: 0/12.
- 고정 구간 MAC 호출 일정 동일: 12/12.
- control CPU/wall-p99 회귀 표시: 0개 경로·구간.

## 고정 구간: CAB thread CPU

시간은 arm당 12개 trial 요약값의 중앙값(µs)이다. **개선율 양수=빨라짐, 음수=느려짐**이며, 표의 시간 중앙값 비율과 동일한 통계량이 아니다. regression은 개선율의 95% 상한이 `-max(2%, A/A 최대 변동폭)`보다 낮은 경우다.

| Engine | Samples | Ch | CPU mean A→S8T µs | 개선율 | 95% 구간 | A/A 최대폭 | 개선 통과 | 회귀 |
|---|---:|---:|---|---:|---|---:|---|---|
| array-v3-6x10 | 32 | 1 | 20.814 → 21.415 | -4.71% | -12.03 ~ +4.54% | 18.23% | False | False |
| array-v3-6x10 | 32 | 2 | 36.325 → 36.894 | -4.03% | -8.72 ~ -0.48% | 19.58% | False | False |
| array-v3-6x10 | 64 | 1 | 21.939 → 22.926 | -8.01% | -21.67 ~ +0.83% | 66.10% | False | False |
| array-v3-6x10 | 64 | 2 | 39.768 → 41.468 | -1.29% | -9.78 ~ +7.36% | 23.07% | False | False |
| expanded-v2 | 32 | 1 | 20.271 → 22.367 | -13.96% | -20.73 ~ -6.58% | 25.99% | False | False |
| expanded-v2 | 32 | 2 | 35.359 → 37.325 | -0.67% | -10.00 ~ +9.36% | 25.26% | False | False |
| expanded-v2 | 64 | 1 | 21.867 → 22.920 | -5.90% | -15.75 ~ +4.32% | 13.35% | False | False |
| expanded-v2 | 64 | 2 | 40.223 → 40.375 | +2.58% | -9.77 ~ +16.40% | 15.14% | False | False |
| original-v1 | 32 | 1 | 20.841 → 22.173 | -11.14% | -32.95 ~ +7.29% | 20.46% | False | False |
| original-v1 | 32 | 2 | 35.732 → 38.705 | -5.49% | -14.46 ~ +1.59% | 28.54% | False | False |
| original-v1 | 64 | 1 | 22.641 → 22.807 | -7.89% | -32.80 ~ +9.64% | 22.23% | False | False |
| original-v1 | 64 | 2 | 42.099 → 45.099 | -0.22% | -12.72 ~ +9.19% | 22.95% | False | False |

## 고정 구간: spectral MAC

| Engine | Samples | Ch | MAC mean A→S8T µs | 개선율 | 95% 구간 | A/A 최대폭 | 개선 통과 | 회귀 |
|---|---:|---:|---|---:|---|---:|---|---|
| array-v3-6x10 | 32 | 1 | 15.624 → 18.043 | -20.59% | -41.42 ~ -5.07% | 47.86% | False | False |
| array-v3-6x10 | 32 | 2 | 29.864 → 31.976 | -7.02% | -18.04 ~ +3.96% | 16.74% | False | False |
| array-v3-6x10 | 64 | 1 | 13.863 → 13.717 | -19.38% | -56.28 ~ +8.98% | 32.69% | False | False |
| array-v3-6x10 | 64 | 2 | 27.314 → 27.578 | -0.11% | -2.28 ~ +2.16% | 8.87% | False | False |
| expanded-v2 | 32 | 1 | 16.650 → 17.624 | -4.87% | -12.97 ~ +3.43% | 10.29% | False | False |
| expanded-v2 | 32 | 2 | 31.056 → 33.301 | -10.33% | -25.39 ~ +1.45% | 33.85% | False | False |
| expanded-v2 | 64 | 1 | 13.658 → 14.743 | -6.57% | -16.73 ~ +3.55% | 19.72% | False | False |
| expanded-v2 | 64 | 2 | 28.434 → 28.439 | -8.78% | -20.41 ~ +4.50% | 40.81% | False | False |
| original-v1 | 32 | 1 | 17.773 → 16.357 | +4.01% | -7.49 ~ +18.01% | 27.36% | False | False |
| original-v1 | 32 | 2 | 30.126 → 31.603 | -11.54% | -24.12 ~ +1.04% | 33.93% | False | False |
| original-v1 | 64 | 1 | 13.649 → 14.005 | -12.65% | -33.54 ~ +6.42% | 16.59% | False | False |
| original-v1 | 64 | 2 | 27.454 → 29.478 | -19.88% | -35.51 ~ -5.82% | 35.39% | False | False |

## 고정 구간: 꼬리 지연과 deadline miss

각 경로 arm당 4,800 콜백. 아래 p99는 trial별 p99의 중앙값이며 여러 실행의 샘플을 합친 p99가 아니다.

| Engine | Samples | Ch | Thread CPU p99 A→S8T µs | Wall p99 A→S8T µs | Miss A/S8T | CPU 초과 A/S8T |
|---|---:|---:|---|---|---|---|
| array-v3-6x10 | 32 | 1 | 57.209 → 31.584 | 55.271 → 32.188 | 1/3 | 0/3 |
| array-v3-6x10 | 32 | 2 | 59.333 → 53.500 | 60.563 → 53.188 | 0/0 | 0/0 |
| array-v3-6x10 | 64 | 1 | 34.042 → 36.583 | 33.730 → 37.709 | 0/0 | 0/0 |
| array-v3-6x10 | 64 | 2 | 62.708 → 76.812 | 63.520 → 81.812 | 0/0 | 0/0 |
| expanded-v2 | 32 | 1 | 36.562 → 45.416 | 36.500 → 44.479 | 1/1 | 0/1 |
| expanded-v2 | 32 | 2 | 51.562 → 52.604 | 50.688 → 51.354 | 0/0 | 0/0 |
| expanded-v2 | 64 | 1 | 45.958 → 49.854 | 48.291 → 50.125 | 0/0 | 0/0 |
| expanded-v2 | 64 | 2 | 82.396 → 78.230 | 80.084 → 76.375 | 0/0 | 0/0 |
| original-v1 | 32 | 1 | 42.646 → 33.230 | 43.104 → 32.938 | 0/0 | 0/0 |
| original-v1 | 32 | 2 | 49.166 → 57.916 | 48.312 → 58.709 | 1/0 | 1/0 |
| original-v1 | 64 | 1 | 46.770 → 48.709 | 49.666 → 49.959 | 0/0 | 0/0 |
| original-v1 | 64 | 2 | 84.667 → 128.938 | 81.145 → 127.875 | 1/1 | 1/1 |

## 전체 콜백: 보조 관찰

비동기 worker/automation 구간을 포함하므로 workload 노출이 일치하는 인과 비교를 대체하지 않는다. deadline miss는 wall 기준이고 CPU over-budget은 thread CPU 기준이다. 희소 miss 수가 줄어도 원인이나 비율 개선이 입증된 것으로 해석하지 않는다. 각 경로 arm당 19,200 콜백.

| Engine | Samples | Ch | CPU mean A→S8T µs | CPU p99 A→S8T µs | Wall p99 A→S8T µs | Miss A/S8T |
|---|---:|---:|---|---|---|---|
| array-v3-6x10 | 32 | 1 | 40.309 → 43.564 | 125.646 → 133.646 | 127.792 → 132.541 | 22/24 |
| array-v3-6x10 | 32 | 2 | 69.332 → 71.657 | 214.709 → 180.459 | 213.000 → 191.104 | 50/40 |
| array-v3-6x10 | 64 | 1 | 47.103 → 44.565 | 125.583 → 119.395 | 122.083 → 118.333 | 4/4 |
| array-v3-6x10 | 64 | 2 | 87.330 → 80.257 | 217.020 → 215.292 | 214.230 → 212.083 | 11/29 |
| expanded-v2 | 32 | 1 | 43.948 → 42.754 | 125.125 → 120.437 | 124.167 → 121.520 | 17/14 |
| expanded-v2 | 32 | 2 | 65.918 → 74.886 | 188.750 → 220.854 | 188.584 → 219.125 | 25/39 |
| expanded-v2 | 64 | 1 | 47.825 → 44.780 | 134.604 → 118.584 | 131.937 → 118.125 | 8/6 |
| expanded-v2 | 64 | 2 | 81.563 → 78.719 | 205.063 → 214.272 | 209.396 → 212.813 | 4/2 |
| original-v1 | 32 | 1 | 44.460 → 45.463 | 122.417 → 127.459 | 120.459 → 125.166 | 19/10 |
| original-v1 | 32 | 2 | 70.402 → 72.428 | 192.166 → 178.084 | 192.605 → 183.208 | 41/35 |
| original-v1 | 64 | 1 | 46.303 → 47.497 | 118.896 → 128.020 | 117.062 → 127.229 | 6/14 |
| original-v1 | 64 | 2 | 77.998 → 88.374 | 207.145 → 229.521 | 207.271 → 229.062 | 13/17 |

| Samples | Deadline µs | 콜백/arm | Wall miss A→S8T | CPU 초과 A→S8T |
|---|---:|---:|---|---|
| 32 | 333.333 | 115,200 | 174 → 162 | 154 → 153 |
| 64 | 666.667 | 115,200 | 46 → 72 | 42 → 68 |

## 정확성·지연 계약과 감사

- A: 원래 CTest 5/5, deadline 66/66, frozen bits/latency=True, 종료 코드=0.
- B: 원래 CTest 5/5, deadline 66/66, frozen bits/latency=True, 종료 코드=0.
- 비트·지연 계약은 144개 realtime 경로, 동결 2,645,088개 float와 fade 388,800개 float, 불변 스펙트럼, 추가 지연 0을 포함한다. A/B 각각 동일 동결 기준으로 검증한다. 일회성 deadline 통과와 반복 모집단의 miss는 별개의 사실로 보존한다.
- 독립 다운로드 감사: manifest 2,176개 파일의 바이트 수·SHA-256, raw 1,843,200개 콜백, 3,456개 경로/구간 요약 검산.
- 추가 감사: 실행 순서 96개, 명령 294개, 모든 원시 요약과 통계 72행 및 최종 판정 재계산. A/A 동일 바이너리와 source/patch/controller 식별 검증.
- 0이 아닌 진단 종료 코드: 4건. 실패 구간과 이상치를 버리지 않았다.

## 결정과 남은 범위

- 자료 수집·무결성 감사: COMPLETE. 성능 개선: NOT DEMONSTRATED. 원래 CAB 계약: PASS.
- PR73 및 S8T의 제품 통합·출시는 보류한다. 이 PR은 검증 전용 Draft이며 `Source/` 변경, 병합, 태그, 패키지 공개를 하지 않았다. `release_approved=false`.
- 전체 PluginProcessor 성능, 전체 제품 회귀 검증, 실제 DAW·물리 인터페이스·청음 수용은 별도 미완료 범위다. 이번 CAB 수치로 대체할 수 없다.
- 재현: `Tools/CabS8TAB/README.md`; 1차 감사: `Tools/CabS8TAB/audit.py`; 추가 감사: 이 폴더의 `audit_statistics.py`.
- `analysis.json.gz`, `report.json.gz`, `native-manifest.json.gz`에 모든 경로·순서·A/A 분포·명령 결과를 보존한다. 원시 CSV는 Actions artifact에 보관되며 보관 만료일은 `provenance.json`을 확인한다.
