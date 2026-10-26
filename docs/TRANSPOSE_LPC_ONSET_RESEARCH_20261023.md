# Transpose 저역 onset 검출 개선: 선형예측 오차 — 2026-10-23 KST

**결론: 에너지 대신 "저역을 얼마나 예측할 수 없는가"를 단서로 쓰자, 강한 음 직후의 약한 음도 저역 재정렬이 걸렸다. 기타 A1 강약교대의 저역 지연이 14.6ms에서 5.7ms로 줄었다.** 지속음 위의 마커에서는 여전히 저역을 재설정하지 않았다(오검출 0). 다만 다음 두 가지가 남았다.

- 새 holdout 입력에서 고역 onset 검출기의 오검출이 새로 드러났다(아래 3번).
- block64 deadline 초과가 기존 후보보다 잦았다.

전체 ≤10ms 판정은 그대로 PASS가 아니다. 합성 입력만 사용했고, 제품 `Source/` DSP는 바꾸지 않았다.

## 아이디어

이전 후보 `corr40-split-up`은 저역 1ms 에너지가 직전 25ms 최댓값의 2배를 넘어야 새 음으로 확인했다. 강한 음 직후 약한 음(−16.5dB)은 앞 음의 저역 꼬리와 에너지가 비슷하다. 그래서 확인에 실패했다(기타 A1 8/16).

앞 음의 꼬리는 감쇠하는 사인파들의 합이라 과거 샘플로 잘 예측된다. 새로 친 음은 위상과 진폭이 새로 시작되므로, 에너지가 같아도 예측되지 않는다.

- **새 후보 `corr40-split-lpc`:** 저역(350Hz 이하)을 약 4kHz로 솎아낸다(decimation). 1ms마다 과거 32ms만으로 10차 선형예측기를 다시 맞춘다(Hann 창, 자기상관법, Levinson). 1ms 예측오차 에너지를 1ms 신호 에너지로 나눈 비율을 계산한다.
- **확인 조건:** 고역 onset이 연 12ms 창 안에서 기존 에너지 조건을 만족하거나, 예측오차 비율이 직전 25ms 최댓값의 3배를 넘고 동시에 0.1을 넘으면 저역을 재설정한다.
- 나머지(업시프트 재정렬, 분모 평활)는 `corr40-split-up`과 같다.

## 파라미터 선택 (Python 시뮬레이션)

평가 대상은 재설정해야 할 사건 216개와 재설정하면 안 되는 마커 144개다. 재설정해야 할 사건은 리프·강약교대·임펄스·seed 8139 랜덤 피킹이다. 마커는 지속음 위의 마커와 seed 8139 랜덤 마커다.

| 방식 | 검출 | 오검출 |
|---|---:|---:|
| 에너지만 (기존) | 208/216 | 0/144 |
| 에너지 OR 예측오차 절대값 (임계 3) | 214/216 | 0/144 |
| 에너지 OR 예측오차 비율 (임계 1.5–5, 하한 0.05–0.2 전 구간) | **216/216** | **0/144** |
| 예측오차 비율, 하한 0.01 | 216/216 | 9–19/144 |

비율 방식은 임계값 1.5–5와 하한 0.05–0.2 전 구간에서 결과가 같았다. 그 가운데 값(3배, 0.1)을 골랐다. 이 선택에는 seed 8139 holdout 입력이 포함됐다. 따라서 **seed 20261024와 새 `random_soft_after_loud` 입력만** C++ 후보를 고정한 뒤 처음 실행한 진짜 holdout이다. 시뮬레이션 스크립트는 증거 폴더에 같이 보관했다.

## 결과 (`corr40-split-up` → `corr40-split-lpc`, C++)

| 항목 | 이전 | 새 후보 |
|---|---:|---:|
| 기타 A1 강약교대: 저역 재정렬 | 8/16 | 16/16 |
| 기타 A1 강약교대: 저역 음별 최대 | 14.61ms | 5.69ms |
| holdout `random_soft_after_loud` 기타 A1 −2, 저역 최대 (seed 8139 / 20261024) | 15.90 / 16.35ms | 6.28 / 6.29ms |
| holdout `random_soft_after_loud` 기타 A1 +2, 저역 최대 (seed 8139 / 20261024) | 10.56 / 10.92ms | 10.48 / 10.48ms |
| 다른 리프 저역 음별 최대 | 4.3–5.7ms | 4.5–5.7ms |
| 마커 위 저역 재설정 (지속음 마커, holdout 마커) | 0 | 0 |
| 행렬 648회 오검출, 고역 어택 최대 | 0, 2.94/5.03ms | 0, 2.94/5.03ms |
| 지속음 워블 최대 | 0.60 cents | 0.60 cents |

- **guards:** 18개 중 15개 출력이 이전 후보와 비트 단위로 같았다(트레몰로·비브라토·밝은 지속음·노이즈·약한 마커). 다른 3개는 riff50이다. 재설정 횟수는 같고 확인 시점만 앞당겨졌다.
- **회귀 검사:** 네이티브 회귀 테스트 4종과 ASan/UBSan 12회 실행이 모두 통과했다.

## 남은 문제

1. **업시프트 저역은 그대로다.** 기타 A1 +2반음에서 10.5ms다. 이 문제는 검출이 아니라 1단계 연구의 하한(최소 지연 + T/2) 문제다.
2. **CPU:** 평균 thread CPU가 11–16% 늘었다(block64 25.8→29.4µs, 제품 엔진은 약 100µs).
   - block64 thread CPU deadline 초과는 4번 측정에서 6–15회였다(기존 후보 1–3회, 콜백 112,500회 중).
   - 초과 위치는 같은 입력을 반복해도 매번 달라서, 입력에 따른 알고리즘 비용으로 보이지는 않는다. 그래도 차이가 반복되므로 PASS로 판정하지 않는다.
   - 감쇠 꼬리의 denormal을 0으로 만드는 처리를 추가했고, 출력은 비트 단위로 그대로다. 초과 횟수는 6회로 여전했다.
3. **새로 드러난 고역 오검출:** seed 8139 `random_soft_after_loud` B0에서, 앞 음 끝의 5ms 페이드아웃 시작점(기울기 꺾임)을 고역 검출기가 onset으로 잡았다. 그 뒤 45ms 대기가 26ms 뒤의 진짜 약한 음을 막았다(누락 1, 오검출 1). 그 음의 저역은 15.6ms, +2반음 어택은 11.3ms다.
   - #117의 4ms 분모 평활이 약한 마커를 잡도록 민감도를 높이면서 생긴 부작용으로 보인다. 이번 LPC 변경과는 무관하다(`corr40-split-up`도 동일).
   - 실제 연주의 뮤트나 노트오프에서도 비슷한 일이 생길 수 있다.
   - 다음 연구 후보는 대기 중이라도 더 강한 onset이면 재검출을 허용하는 방식이다.

## 재현

```sh
R=build/transpose-next/TransposeProbe
python Tools/TransposeNext/low_band_delay.py --renderer $R --out build/lpc/low-band
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite detail --engines corr40-split-up,corr40-split-lpc --out build/lpc/detail
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite guards --engines corr40-split-up,corr40-split-lpc --out build/lpc/guards
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite matrix --engines corr40-split-up,corr40-split-lpc --out build/lpc/matrix
python Tools/TransposeNext/weak_attack_holdout.py --renderer $R --out build/lpc/holdout-8139
python Tools/TransposeNext/weak_attack_holdout.py --renderer $R --out build/lpc/holdout-20261024 --seed 20261024
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite timing --engines corr40-split-lpc,corr40-split-up --out build/lpc/timing
```

원자료: `docs/evidence/transpose-lowband-lpc-onset-20261023/`. 내용은 `summary.json`, suite별 결과, timing 3회(그중 2회는 denormal 처리 전), 시뮬레이션 스크립트, 회귀·sanitizer 로그, `SHA256.json`이다.
