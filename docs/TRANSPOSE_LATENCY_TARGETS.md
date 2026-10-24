# Transpose 지연 목표 기준 (분리 정의) — 2026-10-23 KST

#97 P0의 "Transpose 전체 추가 지연 ≤10ms" 하나짜리 목표를 아래 여섯 기준으로 나눈다. 바꾸는 이유는 다음과 같다.

- 1단계 연구(`TRANSPOSE_NEW_ALGORITHM_FEASIBILITY_20261023.md`)에서, 저음 지속음의 저역 지연에는 알고리즘 계열마다 하한이 있음을 확인했다.
  - 시간영역 접합: 최소 지연 + 주기의 절반(T/2)
  - 분석·재합성: 음정 추정에만 24–48ms
- 업시프트 연구(`TRANSPOSE_UPSHIFT_LOWBAND_RESEARCH_20261023.md`)에서, 업시프트 저역은 한 주기 이상 뒤로 점프해야 하므로 10ms를 보장할 수 없음을 확인했다.
- 그래서 귀에 가장 먼저 들리는 **어택**과 **다운시프트 저역 onset**은 10ms 관문으로 유지한다. **업시프트 저역**은 하한을 명시한 보고 항목으로 분리한다.

기계가 읽는 정의는 `Validation/transpose-latency-targets.json`이고, 평가 스크립트는 `Tools/TransposeNext/latency_targets.py`다. 이 기준은 연구 후보 비교용이며 합성 입력만 사용한다. 제품 승인 기준은 T6까지 통과해야 한다.

## 기준

| ID | 종류 | 내용 | 기준 |
|---|---|---|---|
| **T1** 고역 어택 | 관문 | 1–8kHz 2ms RMS 어택 peak의 추가 지연(출력 − 입력). 입력 자체 peak가 이벤트에 있는 경우만 센다 | 최대 ≤10ms |
| **T2** 다운시프트 저역 onset | 관문 | 음마다 120Hz 저역 에너지 무게중심의 추가 지연. −1/−2반음, 짧은 음과 긴 음(감쇠 80ms) | 최대 ≤10ms |
| **T3** 업시프트 저역 onset | 보고 + 비회귀 상한 | T2와 같은 지표, +1/+2반음. 주기 정렬 접합의 하한(B0/A0 긴 음 19–21ms)을 함께 기록 | 아래 상한 이하 (품질 목표 아님) |
| **T4** 품질·검출 | 관문 | 정상음 워블 p95–p5와 음정 오차. 모든 입력에서 onset 10ms 안 연결(누락·오검출). 마커·지속음 위 저역 재설정 | 워블 ≤1 cent, 오차 ≤1 cent, 누락 0, 오검출 0, 저역 재설정 0 |
| **T5** 실시간 | 보고 | 결정적 최악 콜백 비용과 deadline 초과 | 별도 증거(이 스크립트에서 측정하지 않음) |
| **T6** 외부 수용 | 제품화 전 필수 | 실제 DI 녹음과 청취, Windows/macOS DAW 수용, 고정 PDC 계약, 제품 엔진 통합 | 미실시 |

**T3 비회귀 상한:**

| 대상 | 상한 |
|---|---:|
| 짧은 음 전체 | 12.0ms |
| B0 긴 음 | 15.5ms |
| A0 긴 음 | 19.0ms |
| A1 긴 음 | 11.0ms |

- 정하는 규칙: 이 정의의 첫 전체 측정값(`corr40-split-rt`)에 1.0ms를 더하고 0.5ms 단위로 올림했다.
- 의미: 지금보다 나빠지는지만 잡는 선이다. 이 값이 좋은 값이라는 뜻은 아니다.

**측정 조건:**

- **표본률·블록·반음:** 44.1/48/96kHz, block 128, −2/−1/+1/+2반음
  - 블록 크기는 출력에 영향이 없다. 이전 suite에서 블록 비교 그룹이 324/324, 648/648 동일했다.
- **입력:** B0, A0, A1마다 다음을 넣었다.
  - 리프: riff75, riff50
  - 강약 교대
  - 지속음 위 마커: 0.004, 0.001
  - 정상음 4종
  - 트레몰로, 비브라토
  - holdout 두 시드(8139, 20261024): 랜덤 피킹, 강약 랜덤, 랜덤 마커
  - 긴 음
- **렌더 수:** 1,116회(표본률 3 × 반음 4 × 입력 93)

## 현재 상태

| 기준 | `corr40-split-v2` (3 표본률) | `corr40-split-rt` (3 표본률) | 제품 엔진 (48kHz) |
|---|---|---|---|
| T1 | **PASS** 최대 5.58ms | FAIL 25.37ms (원인은 아래 1) / 그 외 입력 최대 5.14ms | FAIL 69.0ms |
| T2 | **PASS** 짧은 음 6.38ms, 긴 음 9.34ms | PASS 6.37 / 9.34ms | FAIL 48.1 / 51.5ms |
| T3 | 상한 이내 — 짧은 음 10.54, B0 14.43, A0 17.86, A1 10.01ms | 상한 이내 — 10.53 / 14.42 / 17.85 / 9.98ms | 45.9–54.0ms |
| T4 | **PASS** 워블 0.59 cents, 오차 0.001 cents, 누락 0, 오검출 0, 저역 오재설정 0 | FAIL 오검출 188 (원인은 아래 2) | FAIL 워블 570 cents, 오차 356 cents (onset 검출 없음) |
| T5 | 보고: 평균 thread CPU 29.3µs (block64), 초과 0 (공유 VM) | 결정적 최악 콜백 402–447µs(48kHz), VM 잡음으로 10회 중 0–3회 초과 | — |
| T6 | 미실시 (실제 DI·청취·DAW 절차와 도구 준비됨. 자동 호스트 점검에서 블록 분할 의존성 발견) | 미실시 | 미실시 |

`corr40-split-v2`의 수정 내용과 근거는 `TRANSPOSE_T1_T4_FIXES_20261023.md`에 있다. 이후 사전 등록한 새 시드(holdout 31415/27182/16180, 긴 음 2718)로 다시 검증했다. T1·T2·T4 PASS, T3 상한 이내였다(`TRANSPOSE_V2_FRESH_SEED_20261023.md`). 수정 파라미터는 holdout 두 시드를 모두 본 뒤 정했으므로, 다음 검증은 새 시드나 실제 녹음으로 해야 한다.

실제 DI 검증(T6의 일부) 절차와 도구는 `TRANSPOSE_T6_DI_PROTOCOL.md`에 있다. 녹음이 들어오면 사전 등록 뒤 같은 한계값으로 T1–T4를 판정한다. 청취·DAW 수용 준비와 연구용 플러그인 점검 결과는 `TRANSPOSE_T6_LISTENING_DAW.md`에 있다.

제품 엔진은 onset 재설정 기능이 없다. 그래서 T4의 누락 수(2,232)는 검출 기준으로 의미가 없어 표에서 뺐다.

### 이 정의로 처음 드러난 실패 (`corr40-split-rt`, v2에서 수정)

1. **T1: 업시프트 고역 접합 잡음.**
   - 기타 A1의 0.001(−60dB) 마커 위, +1/+2반음에서 발생했다.
   - 어택 자체는 약 5ms에 나온다. 그러나 onset 뒤 25–28ms에 고역 경로의 접합(유지 시간 종료 시점)이 약 1e-3 크기의 고역 성분을 만든다. 이 성분이 마커보다 커서 어택 peak로 잡힌다.
   - v2에서 접합 상한을 4ms로 줄이고 일반 접합 페이드를 4ms로 늘려 해결했다.
2. **T4: 음 끝 페이드에서의 오검출.**
   - holdout 합성 음은 끝 240샘플을 페이드한다. 이 길이는 48kHz에서 5ms지만 96kHz에서는 2.5ms라서 꺾임이 더 가파르다.
   - 96kHz에서 이 꺾임 시작점마다 고역 검출기가 onset을 잡았다(184건).
   - v2에서 상대 고역 하한(직전 100ms 최댓값의 2e-5배)을 추가해 해결했다.

## 운영 규칙

- 연구 후보를 바꾸면 `latency_targets.py`를 전체 조건으로 다시 실행한다. 결과(`results.json`)와 판정을 증거 폴더에 보관한다.
- T1·T2·T4 관문이 하나라도 FAIL이면 제품 통합 후보가 아니다. T3가 상한을 넘으면 회귀로 보고 원인을 기록한다.
- 상한·관문 값은 이 문서와 JSON을 함께 바꾸고, 바꾼 이유와 측정 근거를 남긴다. 판정이 통과로 바뀌도록 지표나 입력을 바꾸지 않는다. 지표가 틀렸다고 판단되면 그 근거를 먼저 문서화한다.
- T5는 공유 VM에서 판정할 수 없다. 전용 하드웨어나 DAW에서 다시 측정해야 한다. T6이 끝나기 전에는 제품 DSP를 교체하거나 릴리스를 승인하지 않는다.

## 재현

```sh
python Tools/TransposeNext/build.py --juce <JUCE 8.0.8> --out build/transpose-next
python Tools/TransposeNext/latency_targets.py --renderer build/transpose-next/TransposeProbe --out build/latency-targets
# 제품 엔진 비교 (48 kHz만):
python Tools/TransposeNext/latency_targets.py --renderer build/transpose-next/TransposeProbe --out build/latency-targets-baseline --engine baseline --rates 48000
# 기준 JSON만 바꾼 뒤 기존 렌더 결과를 다시 판정:
python Tools/TransposeNext/latency_targets.py --renderer build/transpose-next/TransposeProbe --out build/latency-targets --evaluate-only
```

원자료: `docs/evidence/transpose-latency-targets-20261023/`(렌더별 결과와 판정, 실행 로그, `SHA256.json`).

holdout 입력 생성은 `weak_attack_holdout.holdout_input()`로 분리했다. 기존 holdout 입력과 두 시드 모두 9/9 비트 동일함을 확인했다.
