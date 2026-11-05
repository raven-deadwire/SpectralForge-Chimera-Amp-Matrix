# PR #266 — Drop A LOW 컷 검증 및 승인된 수정안

대상: `raven-deadwire/SpectralForge-Chimera-Amp-Matrix` PR #266  
검증 기준 HEAD: `d994a12dacc177af78b5ef0b0ec59225d8ed07e1`  
상태: 사용자 승인 완료. 아래 수치는 반영 전 원본·수정안의 검증 기록이다. 반영 후 CI 결과는 해당 커밋의 Actions 실행에서 별도로 확인한다.

## 판정

**기본음 손실 위험이 실제 선택 알고리즘에서 재현됐다.** 기타를 명시적으로 선택한 Metal 조건에서 55 Hz 기본음과 배음만 있는 합성 신호도 LOW가 30 → 60 → 100 Hz로 상승했다. 저역 잡음을 추가하지 않았는데도 목표 스펙트럼에 맞추려 기본음을 줄였다. 베이스 A0도 20 → 40 Hz로 상승했다.

목표 오차 개선은 측정됐지만, 그것만으로 의도에 맞는 톤 개선이라고 판단할 수 없다. 원본의 목표곡선은 실제 참조 녹음에 아직 튜닝되지 않았다고 소스에 명시되어 있다. 이번 결과는 합성 신호와 컷 분석 경로의 검증이며, 실제 기타·베이스 DI, 앰프/캐비넷 전체 렌더, DAW, 청취 검증을 대체하지 않는다.

## 최신 코드와 CI

- [PR #266](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/266): Open, non-Draft. 마지막 재조회에서도 HEAD는 `d994a12`.
- 기반 브랜치: `claude/sharp-johnson-mr3amz-phase14`, SHA `ca38c170e0c962803dead5e128885334c7926916`. 아직 main 대상의 단독 PR이 아니다.
- 해당 HEAD의 check-run 56개: **성공 54 / skipped 2 / 진행·실패 0**.
- [Open Beta Build and Test](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41972471442): success.
- [Auto Set CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41972471424): Linux·Windows·macOS 성공. Linux 로그에서 `d994a12` 직접 checkout과 `ChimeraAutoSetTests` PASS, 해당 작업의 CTest 4/4 PASS를 확인했다.
- 필터 계수 중복 계산은 수정 완료다. `cutBandChangeDb`가 from/to 계수를 각각 한 번 만들고 `cutPower`에 전달한다. 미해결 결함으로 재등록하지 않는다.
- 이 CI 결과는 **원본 HEAD의 결과**다. 아래 로컬 수정안의 원격 CI는 아직 실행하지 않았다.

## 시험 방법과 재현 범위

Linux x86_64, GCC 13.3, C++20/O2, JUCE 8.0.8(`d6181bde38d858c283c3b7bf699ce6340c050b5d`)을 사용했다.

전체 플러그인을 빌드하지 않고 실제 `analyseCuts`, `cutBandChangeDb`, `cutPower`와 `AutoSet.h`의 `measureSpectrum`, `alignedTarget`, `toneTargetDb`를 추출해 독립 실행했다. 제품 알고리즘을 Python으로 재구현한 결과가 아니다. GUI/앰프 의존 include만 분리했다. 조회한 원본 파일 13개는 Git blob SHA를 HEAD의 tree와 대조했다. FFT와 필터 계수/샘플 처리는 실제 JUCE 구현을 사용했다. 기존 `cutsRules`도 실제 테스트 본문과 원래 noise/EQ/shelf helper로 실행했다. Controller 본문은 실행하지 않았다.

| 구분 | 설계 |
|---|---|
| 샘플레이트 | 44.1 / 48 / 96 kHz |
| 기본음 | 기타 A1 = 55 Hz, 베이스 A0 = 27.5 Hz |
| 주 시험 | 3 레이트 × 2 악기 × 3 스타일 × 10 신호 = 버전별 180 조건 |
| 10개 신호 | 목표곡선 잡음, +12 dB 저역 shelf 잡음, 순음, 32개 1/h 배음, 기본음 -20 dB, 파워코드, 감쇠 포락선, 작은 목표곡선 잡음을 더한 배음, +12 dB shelf 배음, 15 Hz를 더한 배음 |
| 길이 | 6초. 진폭은 처음 2초를 버리고 뒤 4초를 해당 주파수에 동기 검파 |
| 초기 컷 | 주 시험: 기타 LOW 30 Hz, 베이스 LOW 20 Hz, HIGH 9 kHz |
| 반복 | 매번 실제 변경된 LOW 필터로 원 신호를 다시 렌더하고 스펙트럼을 재측정. 최대 6회 |
| 추가 시험 | 버전별 36 조건: 명시적 악기/Auto, LOW 20/70 Hz, 48 kHz 저역 shelf 0/3/6/9/12/18 dB sweep |
| 새 회귀 게이트 | 72 조건: 두 악기 × 세 레이트 × 세 스타일 × sustain/weak-f0/pluck/power-chord |

기본음은 0.1 peak amplitude, 배음은 0.1/h, 위상은 0.37h rad다. weak-f0는 첫 성분만 0.1배로 낮췄다. pluck는 0.5초마다 `exp(-3t)` 포락선을 반복했다. 파워코드는 1.5f0의 배음군을 0.6배로 더했다. 15 Hz 오염 신호의 peak amplitude도 0.1이다. FFT 및 playing 판정은 원본 코드 그대로다.

회귀 게이트는 컷 적용 전 원 신호 대비 첫 4개 배음의 절대 감쇠가 3.1 dB 이내인지 검사한다. 3.1 dB는 제안한 약 3 dB 보존 정책에 JUCE float 계산 여유를 둔 시험 기준이다. 기존보다 완화한 합격 기준이 아니라, 기존에 없던 기본음 보존 요구다.

주 비교는 LOW만 바꾼 효과를 분리하기 위해 HIGH를 9 kHz로 유지하고 마이크 이동 offset을 0으로 두었다. `analyseCuts` 자체는 원래 LOW/HIGH 양쪽을 계산한다. Strength 100%의 LOW 결정을 비교했다. 추가 CSV의 `applied50`은 Controller에 있는 Hz 보간식의 계산값이며 실제 Controller/호스트 시험 결과가 아니다.

## 주요 실측 — 48 kHz, 명시적 악기, Metal, 32개 배음

감쇠는 **컷 필터 전 합성 원 신호 대비**이며, 앰프 전체 음량 손실이 아니다. 오차는 LOW 평가 대역의 목표 대비 RMS 거리다. dBFS 또는 LUFS가 아니다.

| 악기 / 버전 | LOW 경로 | f0 감쇠 | 2f0 | 3f0 | 4f0 | 재측정 목표 오차 |
|---|---|---:|---:|---:|---:|---:|
| 기타 원본 | 30 → 60 → 100 Hz | -10.765 dB | -2.261 | -0.550 | -0.182 | 19.136 → 16.072 dB |
| 기타 제안 | 30 → 55 Hz | -3.010 dB | -0.264 | -0.053 | -0.017 | 19.136 → 18.222 dB |
| 베이스 원본 | 20 → 40 Hz | -7.382 dB | -1.072 | -0.234 | -0.076 | 10.390 → 9.754 dB |
| 베이스 제안 | 20 Hz 유지 | -1.068 dB | -0.073 | -0.014 | -0.004 | 10.390 → 10.390 dB |

초기 필터 자체의 기본음 감쇠는 기타 30 Hz에서 약 -0.356 dB, 베이스 20 Hz에서 -1.068 dB다. 따라서 원본 Auto Set의 **추가** 기본음 손실은 각각 약 10.41 dB, 6.31 dB다.

목표곡선은 실제 알고리즘처럼 매 pass의 250 Hz–1 kHz 측정으로 다시 정렬된다. 표의 마지막 오차는 수렴 후 재측정한 값이다. 중간 pass의 예측값과 실제 렌더를 별도로 기록했다. 예를 들어 기타 60 → 100 Hz의 예측 오차는 15.668 dB, 그 pass의 고정 목표에 대한 실제 렌더 오차는 16.067 dB였다. 정확한 주파수 응답과 1/3옥타브 평균 근사를 혼동하지 않았다.

| 레이트 | 원본 기타 f0 / 최종 컷 | 제안 기타 f0 / 컷 | 원본 베이스 f0 / 컷 | 제안 베이스 f0 / 컷 |
|---|---:|---:|---:|---:|
| 44.1 kHz | -10.763 dB / 100 Hz | -3.010 / 55 Hz | -7.380 / 40 Hz | -1.083 / 20 Hz |
| 48 kHz | -10.765 dB / 100 Hz | -3.010 / 55 Hz | -7.382 / 40 Hz | -1.068 / 20 Hz |
| 96 kHz | -10.786 dB / 100 Hz | -3.010 / 55 Hz | -7.392 / 40 Hz | -0.998 / 20 Hz |

레이트별 작은 차이는 실제 float 계수/상태 및 FFT 대역 분할을 포함한다. 표는 아날로그 Butterworth 공식만 계산한 값이 아니다.

## 어떤 조건에서 높은 LOW를 고르는가

1. **목표보다 낮은 대역이 강하면 올라간다.** 각 후보의 RMS 목표 오차 개선이 0.5 dB를 넘어야 한다. 목적 함수에는 기본음 보존 항목이 없다.
2. **Metal 목표는 50–80 Hz를 더 줄인다.** 일반 기타의 50 Hz 목표 -16 dB에 Metal -3 dB가 더해진다. 연주 음정과 관계없는 고정 목표다.
3. **베이스 기본음 27.5 Hz는 평가에서 빠진다.** 공유 spectrum의 첫 band가 50 Hz이며 베이스 LOW 평가는 50–100 Hz다. A0 손실은 그 목적 함수에 직접 반영되지 않는다.
4. **초기 컷과 1회 이동 범위가 결과를 바꾼다.** 기타 30 Hz에서는 60 Hz까지만 탐색하고, 다음 실행에서 100 Hz에 도달한다. 70 Hz에서는 한 번에 100 Hz를 선택했다. 같은 신호라도 20 Hz에서는 40 Hz까지의 후보 중 0.5 dB 개선이 없어 20 Hz에 머무는 경우가 있었다.
5. **악기 Auto는 별도 경로다.** 35–71 Hz 대 body 에너지 비로 판별한다. 강한 A1 신호는 베이스로 분류되기도 한다. 위 100 Hz 재현은 Guitar를 명시한 경우다. Auto에서 항상 100 Hz가 선택된다고 일반화할 수 없다.

48 kHz 목표곡선 잡음에 저역 shelf를 더했을 때의 원본 선택은 다음과 같다. 수치는 해당 고정 fixture의 결과이며 모든 연주에 통용되는 문턱은 아니다.

| 저역 shelf | 기타: 초기 70 Hz, 100 Hz shelf | 베이스: 초기 20 Hz, 60 Hz shelf |
|---:|---:|---:|
| 0 dB | 35 Hz | 20 Hz |
| +3 dB | 60 Hz | 20 Hz |
| +6 dB | 70 Hz 유지 | 40 Hz |
| +9 dB | 90 Hz | 40 Hz |
| +12 dB | 100 Hz | 40 Hz |
| +18 dB | 100 Hz | 40 Hz |

## 의도적인 정리와 기본음 손실의 구분

- **의도적인 저역 정리의 합성 대조군:** 목표곡선에 +12 dB shelf를 가한 잡음. 48 kHz 기타 원본의 오차는 7.132 → 2.113 dB로 줄었고, 제안은 7.132 → 5.806 dB로 줄었다. 이런 과도한 저역을 제거할 때 원본의 강한 컷은 더 큰 목표 개선을 낸다.
- **음악 성분 손실 재현:** 잡음이 없는 A1/A0 배음 신호에서도 컷이 올라갔다. 특히 A0는 목표 오차 약 0.64 dB 개선을 위해 기본음에 6.31 dB의 추가 손실을 발생시켰다. 기본음 보존이 요구되는 사용에서 불필요한 손실 위험으로 판정한다. 소리가 실제로 더 나쁜지는 청취로 확인하지 않았다.
- **15 Hz 오염 대조군:** 기타 배음에 15 Hz를 더했을 때, 원본의 추가 15 Hz 감소는 누적 약 20.61 dB였고 제안은 약 10.24 dB였다. 제안도 서브소닉을 줄이지만 덜 공격적이다. 베이스 제안은 이 fixture에서 20 Hz를 유지해 추가 정리를 하지 않았다.
- **약한 기본음:** 기타 f0를 -20 dB 낮춘 fixture에서는 원본도 30 Hz를 유지했다. 따라서 모든 Drop A 신호가 상한까지 올라간다는 주장은 성립하지 않는다. 베이스는 같은 기본음 약화에도 40 Hz를 선택했다.

합성 잡음은 연주자의 의도를 갖지 않는다. 여기서 '의도적인 정리'는 알려진 저역 오염을 더한 통제 대조군이라는 뜻이다. 실제 연주에서 기본음·공진·험을 자동 구별하려면 시간적 음정/배음 정보와 청감 평가가 필요하다.

## 최소 수정안과 한계

음정 검출기를 새로 넣지 않고, 모든 스타일에서 Drop A를 보호하는 고정 상한을 둔다. 2차 Butterworth에서 cutoff가 기본음 이하이면 기본음 손실은 약 3 dB 이하이므로, 5 Hz grid에 맞춰 **기타 55 Hz / 베이스 25 Hz**를 제안한다.

```diff
-inline float cutLowLimit(bool bass,Style style) noexcept {return bass?40.f:style==Style::metal?100.f:80.f;}
+inline float cutLowLimit(bool bass,Style) noexcept {return bass?25.f:55.f;}
```

이것은 **보수적인 자동 상승 제한 정책**이다. 음정 감지나 최적 cutoff 계산이 아니다. 기존 목표/0.5 dB 개선 조건, 계수 캐시 수정, HIGH 검색, Strength·lock·UNDO·Controller는 그대로다. 기존 thin/metal 시험은 바뀐 상한과 '기존 높은 컷을 강제로 내리지 않음'에 맞춰 기대값을 정정한다.

**중요한 잔여 위험:** 현재 파라미터 기본 LOW는 70 Hz다(`PluginProcessor.cpp` layout). 이미 70 Hz인 배음 fixture는 제안에서도 유지됐다. 이때 컷만으로 A1은 약 -5.59 dB, A0는 -16.34 dB다. 따라서 이 패치를 '모든 현재 설정의 기본음을 3 dB 이내로 복구하는 수정'으로 승인해서는 안 된다. 보존 보장은 처음부터 상한 이하인 설정에서의 자동 증가에 대한 것이다. 높은 기존 컷은 목표 오차도 개선될 때만 낮아진다.

기본값 변경 또는 음정 근거가 있을 때 높은 기존 컷을 강제로 내리는 정책은 별도 결정이 필요하다. 이번 패치에는 포함하지 않았다. 또한 더 낮은 튜닝, 잘못된 악기 선택, 다른 EQ/마이크/앰프 단계의 감쇠까지 보장하지 않는다. 베이스 20→25 Hz의 개선이 0.5 dB에 못 미치는 경우가 많아 베이스 자동 저역 정리 기능이 제한되는 대가가 있다.

향후 적응형 방식이 필요하면 CAB 출력만 보고 음정을 추정하기보다, 덜 처리된 입력의 안정적인 기본음/배음 추적과 신뢰도를 사용하고 확신이 낮을 때 이 고정 상한으로 되돌아가는 방식을 검토할 수 있다. 빠른 리프, 파워코드, 약한/없는 기본음, 벤딩, 오염음에 대한 별도 검증이 필요하며 이번 결과로 검증된 기능이 아니다.

## 시험 결과

| 항목 | 결과 |
|---|---|
| 원본 + 새 Drop A 회귀 | 예상대로 FAIL. 첫 fixture에서 f0 -3.831787 dB로 3.1 dB 기준 초과 |
| 제안 + 새 Drop A 회귀 | 72/72 PASS. 3 레이트, 3 스타일, 4 연주 형태, 첫 4개 배음, 반복 실행, 기존 높은 컷 추가 상승 방지 |
| 제안 + 기존 `cutsRules` | PASS. 필터 대역 모델, boom/thin, fizz/dull, 마이크 offset, silence |
| 조사용 주 행렬 | 버전별 180 조건. 원본 408 pass, 제안 252 pass |
| 추가 행렬 | 버전별 36 조건. 원본 48 pass, 제안 38 pass |
| 배음 신호의 예측 RMS 오차 vs 실제 렌더 오차 차이 | 최대 원본 0.4175 dB / 제안 0.3294 dB |
| f0의 필터 주파수 응답 vs 실제 렌더 감쇠 차이 | 최대 원본 0.0050 dB / 제안 0.0011 dB |
| 전체 제품/Controller·호스트·Windows/macOS 로컬 시험 | 미실행. 원본 CI 성공을 제안에 승계하지 않음 |
| 실제 기타·베이스 DI / 청취 | 미검증 |

추가 관찰: 44.1 kHz Guitar/Metal의 순음 한 조건은 원본에서 75↔100 Hz를 왕복해 6회 안에 수렴하지 않았다. 단일 순음에는 목표 정렬용 중역 내용이 거의 없어 유의미한 음색 적합도 지표가 되지 못한다. 별도의 sparse-spectrum 한계로 기록하며, 이번 위험 판정은 안정적으로 수렴한 배음 신호 결과에 근거한다. 제안의 180조건은 모두 6회 안에 수렴했다.

## 승인 대상 파일

| 파일 | 변경 |
|---|---|
| `Source/AutoSetCabCuts.h` | LOW 상한 55/25 Hz와 정책 주석 |
| `Tests/AutoSetCutsTests.h` | 72건 회귀 게이트, 실제 float 필터 진폭 측정, 상한 관련 기존 기대값 정정 |
| `docs/AUTO_SET_PHASE15.md` | 범위·상한·오차 근사·검증 한계 설명 |
| `docs/AUTO_SET_PHASE15_DROP_A_VALIDATION.md` | 이 검증 보고서 추가 |

승인 전 원격 변경은 수행하지 않았다. 사용자 승인 후 반영 직전에 HEAD가 여전히 `d994a12`임을 확인했다. 위 4개 파일을 PR 브랜치에 반영하며, 전체 제품/Controller CI 결과는 반영 커밋에 대해 별도로 확인한다. 병합·릴리스는 이번 승인 범위에 포함하지 않는다.

## 재현 자료

검토 패키지에는 전체 diff, 원본과 제안의 CSV, CI snapshot, SHA 목록, 사용한 소스 범위와 독립 실행기가 포함된다. 실행기는 저장소나 GitHub를 변경하지 않는다.

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE
python3 run_validation.py
```

`run_validation.py`는 원본의 새 회귀 시험 exit 1을 예상 실패로 처리하고, 제안의 시험 exit 0을 요구한다. 조사용 CSV는 모든 시도를 보존한다. 잡음-only fixture의 f0/배음 진폭 열과 순음의 2~4배음 열은 음악 성분의 감쇠량으로 해석하면 안 된다. 보고서 표는 실제 해당 성분을 포함한 배음 fixture만 사용했다. `sub_delta_db`도 15 Hz를 명시적으로 넣은 `harmonics_sub`에서만 유효한 오염 성분 지표다.

원본 근거: [컷 선택](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d994a12dacc177af78b5ef0b0ec59225d8ed07e1/Source/AutoSetCabCuts.h), [스펙트럼/목표/악기 판별](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d994a12dacc177af78b5ef0b0ec59225d8ed07e1/Source/AutoSet.h), [Controller 적용](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d994a12dacc177af78b5ef0b0ec59225d8ed07e1/Source/AutoSetController.h), [실제 컷 필터](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d994a12dacc177af78b5ef0b0ec59225d8ed07e1/Source/Cabinet.h), [파라미터 기본값](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d994a12dacc177af78b5ef0b0ec59225d8ed07e1/Source/PluginProcessor.cpp).
