# Transpose 저역 재정렬·업시프트 후속 연구 — 2026-10-22 KST

**결론: #112의 남은 차단 요인 다섯 가지 중 저역 지연, 0.001 마커 누락, 업시프트 어택, CPU 증가 네 가지를 합성 측정에서 해소했다. 그래도 전체 ≤10ms 판정은 아직 PASS가 아니다.** 다음 세 가지가 남아 있다: 강한 피킹 직후 약한 피킹의 저역(기타 A1, 최대 14.6ms), 업시프트 저역(기타 A1, 최대 10.6ms), 이 VM에서의 block64 deadline 판정. 실 DI 입력, Neural DSP 비교, 고정 PDC, 실제 DAW 검증은 이번에도 하지 않았다. 제품 `Source/` DSP는 교체하지 않는다.

#112(`research/transpose-weak-attack-20261021`)를 main `23e710e`(#114·#115 포함) 위에 병합한 뒤 연구 도구(`Tools/TransposeNext`)만 수정했다. 새 후보 엔진 이름은 `corr40-split-up`이다. 기존 엔진(`corr40-split` 등)의 출력은 바뀌지 않으며, 새 동작은 모두 opt-in 설정으로만 켜진다.

## 원인 분석

1. **저역 지연(10.7–16.9ms, 리프에서는 최대 36ms).** `corr40-split`은 350Hz 아래 대역의 재생 위치를 어택에서 재설정하지 않는다. 주기 정렬 접합만으로 다운시프트하면 평균 추가 지연이 대략 T/2(B0는 약 16ms)보다 작아질 수 없다. 기존 임펄스 진단만으로는 이 문제가 과소평가됐다. 이번에 추가한 리프별 측정(120Hz 저역 에너지 무게중심)에서는 `corr40-split`이 23.8–36.1ms까지 늘어났다.
2. **0.001 마커 6/16 누락(기타 A1).** novelty의 분모는 광대역 에너지인데, 이를 0.3ms로 평활해서 55Hz 지속음의 파형 자체를 따라갔다. 파형이 0을 지날 때 생기는 novelty 스파이크가 41ms 최댓값 기준을 끌어올렸고, 파형 정점 부근에 떨어진 약한 마커가 묻혔다. Python으로 재현한 시뮬레이션이 같은 6개 누락을 그대로 보였다.
3. **업시프트 어택 37.7ms.** 업시프트에서는 onset 재설정이 꺼져 있었다. 단순히 켜기만 하면 어택 자체는 5.8ms에 나온다. 그러나 다음 역방향 접합이 어택을 다시 읽어 이중 어택(flam)이 생겼고, 그 사본이 측정 peak로 잡혀 이벤트 절반이 약 38ms로 남았다.

## 후보 `corr40-split-up` 변경점

- **저역 재정렬(onset 게이트 + 저역 확인).** 고역 경로가 onset을 검출하면 12ms 창을 연다. 그 창 안에서 저역(350Hz 이하) 1ms 에너지가 직전 25ms 최댓값의 2배를 넘으면 저역 재생 위치도 재설정한다. 지속음 위의 작은 마커는 이 확인을 통과하지 못하므로 저역 궤적이 유지되어 워블이 생기지 않는다. 새로 친 저음은 확인을 통과한다. 트레몰로·비브라토처럼 고역 onset 자체가 없는 입력에서는 저역도 재설정하지 않는다.
  - 처음 시도한 방식인 저역 에너지 상승률만 보는 독립 검출기는 실패해 폐기했다. 기존 3배·60ms 피크 방식은 리프에서 16개 중 1개만 검출했다. 1–15ms 평활과 비율 3–8을 탐색해도 지속음 배음 맥놀이·리플 때문에 오검출이 15–1096회 나왔다.
- **업시프트 재정렬.** 재생 헤드를 하한 경계에서 25ms 이동 거리만큼 위에 놓는다. +2반음 기준 약 3ms다. 그 뒤 25ms 동안은 어택을 끊김 없이 재생한다. 그다음 역방향 접합은 "onset 경과 시간 − 8ms"보다 과거로 가지 못하게 제한했다. 따라서 어택이나 onset 이전 음을 다시 읽지 않는다.
- **novelty 분모 평활 4ms**(고역 경로만). 시뮬레이션에서 2–7ms 범위 전부가 누락 0·오검출 0이었다. 10ms부터는 트레몰로 오검출이 생겼고(24회), 4ms는 그 범위의 중간값이다.

## 결과 (합성 입력, 48kHz·128 samples 기준; 행렬은 44.1/48/96kHz × 64/128/256)

| 항목 | #112 `corr40-split` | `corr40-split-up` |
|---|---:|---:|
| 저역 임펄스 추가 무게중심 | 10.686–16.877ms | 4.063ms |
| 저역 리프(B0/A0/A1 × riff75/riff50/강약교대) 음별 최대 | 23.8–36.1ms | 4.3–5.7ms, 단 A1 강약교대 **14.6ms** |
| 다운시프트 고역 어택 추가 최대(행렬 −2/−1) | 2.94ms | 2.94ms |
| 업시프트 고역 어택 추가 최대(행렬 +1/+2) | 37.69ms (검출 꺼짐, 1728 이벤트 누락) | 5.03ms (누락 0) |
| 0.001 마커, 기타 A1(guards) | 6/16 누락, 30.19ms | 0 누락, 2.73ms |
| 지속음 워블 최대(행렬) | 0.59 cents | 0.60 cents (+1/+2 마커 0.41) |
| 오검출(unmatched reset, 행렬 648회 렌더) | 0 | 0 |
| block64 thread CPU 평균(중앙값) | 27.6µs | 26.2µs (현재 제품 baseline 104.0µs) |

- **Holdout (seed 8139, 조정 후 1회 실행, 임계값 재조정 없음).** 랜덤 피킹에서 저역 음별 최대는 −2반음 5.9–6.3ms(`corr40-split` 36.2–36.3ms), +2반음 7.3–10.6ms(같은 조건 20.6–27.3ms)였다. 고역 어택은 −2에서 2.67ms, +2에서 4.83–4.96ms다. 검출은 96/96 이벤트를 10ms 안에 연결했다. 기타 A1 `random_markers`는 입력 자체의 1–8kHz peak가 이벤트 위치에 있지 않다(입력 lag 최대 66ms, valley-to-peak −7.2dB). 따라서 어택 지연 수치는 모든 엔진에서 무효이며 `attack_metric_valid=false`로 표시했다. 이 경우에도 검출 연결은 17/17이다(`corr40-split`은 −2에서 1개, +2에서 16개 누락).
- **CPU.** 이전 "약 3배 증가"는 #115 이전 baseline과 비교한 값이었다. 현재 main의 제품 엔진(포먼트 보존 포함)과 비교하면 후보가 블록 크기별로 약 1/4의 평균 thread CPU를 쓴다. 이 비교만으로 후보가 더 싸다고 단정하지는 않는다. 두 엔진은 기능과 품질이 다르다.
- **Guards.** 밝은 지속음·노이즈 바닥·비브라토에서 시작 구간 외 재설정은 0회, 품질 차이는 0.1 cents 미만이다. 트레몰로는 시작 시 1회 재설정한 뒤 접합 위상이 달라졌다. 그 결과 워블 proxy가 B0 48.6→113.4, A0 68.6→173.1, A1 10.8→9.5 cents로 변했다. 같은 입력에서 baseline은 50.9/135.6/5.3 cents로, 이 지표가 엔진마다 크게 흔들린다(트레몰로는 원래 정상음 품질 기준 대상 외). 그래도 개선이라고 주장하지 않는다.

## 아직 FAIL 또는 미판정인 항목

1. **강한 음 직후 약한 음의 저역(기타 A1 강약교대, −16.5dB, 75ms 간격).** 저역 확인 단계가 약한 음 8/16을 놓친다. 앞 음의 저역 꼬리 에너지가 새 음과 같은 크기라서 에너지 비교로는 구분되지 않는다. 그런 음의 저역은 최대 14.6ms다. B0/A0에서는 16/16이다.
2. **업시프트 저역.** 25ms 유지 뒤 접합은 저음 한 주기 이상 뒤로 뛰어야 한다. 그래서 +2반음 기타 A1 리프·랜덤 피킹은 10.0–10.6ms다. 유지 시간 25/35/45/60ms를 탐색했지만 모든 악기에서 동시에 10ms 이하가 되는 값은 없었다(45ms는 기타 9.5ms·B0 14.3ms).
3. **block64 deadline.** 한 번에 112,500 콜백 중 thread CPU 기준 3–7회 초과가 있었다. 같은 실행에서 기존 `corr40-split`은 1–4회, 제품 baseline은 0–3회였다. 공유 VM 잡음과 접합 검색 burst를 구분하지 못했으므로 PASS로 판정하지 않는다.
4. 저역·고역 두 경로의 지연이 서로 다르고 시변이라 크로스오버 착색이 남는다. 고정 PDC도 없다. 저역 지표는 120Hz 저역 에너지 무게중심이며, 지각된 베이스 onset이나 호스트 지연과 같지 않다.

## 재현과 산출물

```sh
python Tools/TransposeNext/build.py --juce <JUCE 8.0.8> --out build/transpose-next
R=build/transpose-next/TransposeProbe
python Tools/TransposeNext/low_band_delay.py --renderer $R --out build/weak/low-band
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite detail --engines corr40-split,corr40-split-up --out build/weak/detail
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite guards --engines corr40-split,corr40-split-up --out build/weak/guards
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite matrix --engines corr40-split,corr40-split-up --out build/weak/matrix
python Tools/TransposeNext/weak_attack_holdout.py --renderer $R --out build/weak/holdout
# 다른 작업이 끝난 뒤 단독 실행:
python Tools/TransposeNext/weak_attack_study.py --renderer $R --suite timing --engines baseline,corr40-split,corr40-split-up --out build/weak/timing
```

산출물은 `docs/evidence/transpose-lowband-upshift-20261022/`에 있다. 내용은 `summary.json`(집계), suite별 원결과, ASan/UBSan 실행 로그 8회, 네이티브 회귀 테스트 4종 로그(모두 exit 0), `SHA256.json`이다. `guards-reference-baseline-onset.json`은 upshift 변경 전에 빌드한 renderer로 만들었다. baseline·`corr40-onset` 코드 경로는 그 뒤 바뀌지 않았다(새 설정의 기본값은 기존과 같다).
