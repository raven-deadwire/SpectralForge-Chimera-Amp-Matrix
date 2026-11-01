# Auto Set Phase 12 — B0·A0 합성 검증 및 수정

기록일: **2026-11-01 KST**

상태: **합성 DSP 검증 완료 / 실제 DI·청취·실제 DAW 검증 미실행**

기준 소스: [`f371f0442312de279e305bacffb1ae4ed5dd1e97`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/f371f0442312de279e305bacffb1ae4ed5dd1e97). PR #242가 포함된 main이며 이후 #243은 OBS macOS 검증 변경이다. 측정은 이 기준 소스의 원래 `AutoSetPower.h`와 본 변경의 실제 C++ 분석 함수를 각각 컴파일해 비교했다.

## 1. 결론과 적용 범위

**현재 명목상 40–100Hz인 베이스 판단은 실제로 50·63·80·100Hz 네 중심 대역만 측정한다.** 공용 `toneBandCentres`가 50Hz부터 시작하므로 40Hz 대역도 없다. 하한 상수만 25로 바꾸어서는 A0·B0 기본음을 추가할 수 없다.

기본음은 강하고 2–8차 배음은 약한 합성 신호에서 **Niflheimr와 Hot Lead가 저역을 불필요하게 증가시키는 사례를 재현했다.** 기본 240조건의 해당 패턴은 기존 11건에서 수정 0건으로 줄었다. 이는 구성한 합성 스트레스 입력과 잠정 목표 곡선에 대한 결과다. 모든 실제 베이스 DI에서 같은 오류가 발생한다는 뜻은 아니다.

수정은 Phase 12의 베이스 경로에 25·31.5·40Hz 중심 대역과 약 1.5Hz FFT 해상도를 추가하고, RESONANCE/DEPTH를 **25–100Hz 대 400Hz–1.6kHz**로 판단한다. 공용 Tone EQ 분석기와 다른 단계의 대역은 변경하지 않았다. PRESENCE/RESONANCE의 가중 오차 합을 악화시키는 후보를 거부하는 조건도 추가했다.

주 측정과 컨트롤러 회귀는 **Bass를 명시적으로 선택**한다. 기존 Auto 악기 분류기는 별도 특성 검사를 했으며 수정하지 않았다. 이 구분은 8절에 기록한다.

## 2. 입력·앰프·측정 방법

- 샘플레이트: **44,100 / 48,000 / 96,000Hz**.
- 기본음: **A0 27.5Hz / B0 30.8677063285Hz**.
- 기본 매트릭스: 네이티브 헤드 6종 × 샘플레이트 3 × 음 2 × 합성 입력 3 × 캐비넷 유무 2 = 216조건. Niflheimr 추가 4채널의 배음이 풍부한 캐비넷 조건 24건을 더해 **240조건**이다.
- 헤드: Tight 515, Cinder 120, Night Harvest, Hot Lead, Náströnd, Niflheimr. 기본 채널에 두 노브가 모두 표시되는 여섯 모델 전부이며, 모든 판매 앰프·채널을 시험했다는 뜻은 아니다.
- 추가 부하: Gain 및 노출된 MASTER를 범위의 85%로 설정한 Metal/harmonic-gap 조건 **72건**. 노브를 더 세게 구동했을 때의 상호작용을 본다.
- 대안: Niflheimr 기본 채널 **36조건**에서 저역 상한만 100→160Hz로 바꾼 별도 분석기를 실행했다.
- 캐비넷: 제품의 `cabLayout::generate`로 생성한 Bass 4×10 / Foundry 10 / Silk 121 모델 커널과 JUCE convolution. 측정된 실물 IR이나 전체 플러그인 호스트 라우팅이 아니다.

판단 대역의 “25–100Hz”는 그 범위에 속하는 1/3옥타브 **중심 대역의 합**이다. 실제 외곽은 약 22.3–112.2Hz이며 A0와 B0를 포함한다. CSV의 별도 `sub25_40_db` 등 고정 구간 전력과 구분한다.

각 신호는 3초이며 0.5초 간격의 피킹 형태다. `harmonic_rich`는 많은 배음을, `fundamental_dominant`는 강한 기본음을 포함한다. `harmonic_gap`은 기본음 진폭 1, h2–8은 0.015/h, h9–128은 0.18/h^0.4로 구성한다. 마지막 입력은 저차 배음이 약한 경우를 분리하는 스트레스 신호이며 특정 실물 베이스의 모델이라고 주장하지 않는다. 생성식과 고정 위상·포락선은 `Tests/AutoSetPowerBassProbe.cpp`에 있다.

결정은 제품의 `analysePower`/`nativePowerRenderer`로 계산한다. 검사용 별도 분석은 최대 0.5Hz 간격 FFT와 알려진 주파수 투영을 사용한다. 25–40 / 40–100 / 100–250 / 400–1600Hz 에너지, coherent 기본음 peak 성분, RMS, peak, crest factor, 20ms RMS 분포의 압축량을 기록한다. 압축량은 내부 파워단의 클리핑 횟수나 왜곡률이 아니다.

같은 입력을 최초 상태, PRESENCE만 적용, RESONANCE만 적용, 두 제안 동시 적용으로 각각 렌더했다. 두 노브를 각각 +0.2 및 동시에 +0.2 바꾼 고정 비교도 포함한다. 반영 후 수치는 예측 스펙트럼만이 아니라 **실제로 다시 렌더한 출력**의 독립 측정값이다.

## 3. 재현 사례: Niflheimr / Hrímfaxi

Rock, harmonic-gap, 앰프 단독, 최초 PRESENCE=DEPTH=0.50이다. 아래 수치는 동일한 독립 25–100Hz/중역 기준에서 목표와의 차이(dB)다. 양수는 목표보다 저역 비율이 큰 상태다.

| kHz | 음 | 최초 | 기존 적용 후 | 25–100Hz 수정 후 | 25–160Hz 대안 적용 후 |
|---:|---|---:|---:|---:|---:|
| 44.1 | A0 | +1.374 | +3.021 | +0.777 | +2.226 |
| 44.1 | B0 | +1.255 | +3.126 | +0.285 | +2.091 |
| 48 | A0 | +1.361 | +3.009 | +0.770 | +2.216 |
| 48 | B0 | +1.255 | +3.128 | +0.287 | +2.093 |
| 96 | A0 | +1.356 | +3.012 | +0.775 | +2.220 |
| 96 | B0 | +1.248 | +3.129 | +0.290 | +2.095 |

기존 알고리즘은 여섯 조합 모두 **PRESENCE 0.50→0.20, DEPTH 0.50→0.80**을 선택했다. 48kHz A0에서 기존 표시 기준은 −18.090→−13.416dB로 개선되지만, 기본음까지 포함한 독립 기준은 +1.361→+3.009dB로 악화된다. 기본음 peak 성분도 A0 약 +0.772dB, B0 약 +1.005dB 증가한다.

수정안은 여섯 조합 모두 **PRESENCE 0.50→0.20, DEPTH 0.50→0.20**을 선택한다. 최초 저역이 허용 범위여도 먼저 PRESENCE를 바꾸면 공유 중역 기준이 변하므로 이후 DEPTH 보정이 필요하다. 수정 적용 후 기본음 자체의 레벨은 A0 약 −1.50dB, B0 약 −1.87dB 변한다. 따라서 이 변경을 기본음 레벨의 원상 보존 기능으로 설명하면 안 된다.

Hot Lead의 같은 입력/앰프 단독 B0에서도 세 샘플레이트 모두 기존 RESONANCE 0.50→0.70과 독립 저역 편차 약 +1.636→+3.02dB를 확인했다. A0의 48·96kHz는 0.50→0.60과 약 +0.98→+1.65dB다. Niflheimr 6건과 Hot Lead 5건을 합친 것이 위 11건이다.

## 4. 전체 비교와 앰프별 조절

기본 240조건에서 70조건의 제안이 달라졌다. 독립 25–100Hz 절대 편차는 66조건에서 기존 대비 0.2dB 초과 개선됐고, 0.2dB 초과 악화는 없었다. 다만 Tight 515 / 48kHz / B0 / fundamental-dominant / 앰프 단독의 편차는 **0.01890dB 증가**했다. 모든 수치가 일률적으로 개선됐다는 판정은 하지 않는다.

아래는 수정안의 기본 매트릭스 전체 범위다. 노브 이동은 0–1 범위에서의 변화량이며, 저역 변화는 실제 출력의 25–40Hz 전력 변화다. 상호작용은 두 노브를 동시에 +0.2 바꾼 결과에서 각 단독 변경량의 합을 뺀 절댓값 최대치다.

| 헤드 | 조건 수 | PRESENCE 이동 | RESONANCE/DEPTH 이동 | 25–40Hz 변화(dB) | 저역 상호작용 최대(dB) |
|---|---:|---:|---:|---:|---:|
| Tight 515 | 36 | -0.30…+0.30 | -0.30…+0.30 | -2.00…+1.49 | 0.466 |
| Cinder 120 | 36 | -0.30…+0.00 | -0.25…+0.30 | -1.48…+0.76 | 0.041 |
| Night Harvest | 36 | -0.30…+0.00 | +0.00…+0.30 | -2.91…+3.21 | 0.609 |
| Hot Lead | 36 | -0.30…+0.00 | -0.30…+0.00 | -1.11…+0.09 | 0.025 |
| Náströnd | 36 | -0.30…+0.00 | -0.30…+0.30 | -2.87…+2.19 | 0.016 |
| Niflheimr | 60 | -0.30…+0.00 | -0.30…+0.30 | -1.91…+1.05 | 0.000 |

고게인 Metal 72조건에서는 25조건의 제안이 달라졌고, 기존 대비 독립 저역 편차가 0.2dB 넘게 개선된 것은 12조건, 더 커진 것은 **8조건**이다. 최대 증가는 약 **1.004dB**다. 내역은 Niflheimr 앰프 단독 6조건, Hot Lead 44.1kHz A0+cab 1조건(+0.832dB), Náströnd 48kHz A0+cab 1조건(+0.252dB)이다. 목표 허용 범위, 1dB 최소 개선량과 PRESENCE까지 함께 고려하는 선택 때문에 기존과 결과가 달라진다. 기본음 포함이 모든 고게인 조건의 저역 편차를 기존보다 더 작게 만든다는 증거는 없다.

기본·고게인 네이티브 매트릭스에서 **각 실행의 변경 전 대비 두 역할의 보고 가중 오차 합이 증가한 경우는 기존과 수정 모두 0건**이다. 다음 절의 총비용 허점은 별도 통제 신호로 재현했으므로 이 둘을 혼동하지 않는다.

## 5. PRESENCE·RESONANCE 상호작용과 남은 제한

### 합계 오차 허점의 회귀

기존 비용식은 다른 대역의 오차가 1.5dB 이내일 때 그 증가를 벌점에 넣지 않는다. 통제 신호에서 (PRESENCE 편차, 저역 편차)가 **(1.6, 0.0)→(0.4, 1.4)dB**로 바뀌면 기존식은 자체 1.2dB 개선으로 채택하지만, 합계는 1.6→1.8dB로 악화된다.

새 조건은 이 후보를 거부하고, (1.6, 0)→(0.2, 0.6)처럼 합계가 개선되는 후보는 허용한다. 이 조건만 제거한 별도 복사본은 실제 회귀 시험에서 `PRESENCE worsened the combined balances within the other band's tolerance`로 실패했다. 제품 소스를 시험 도중 되돌리거나 실패 기준을 완화하지 않았다.

### 비선형 응답

고게인 +0.2/+0.2 비교에서 저역 상호작용은 최대 약 **0.606dB**, coherent 기본음 성분은 약 **0.717dB**였다. 노브별 변화량을 더하는 선형 EQ 가정만으로 결과를 설명할 수 없다. 주파수 균형, crest/RMS와 RMS 분포 변화는 측정했지만 내부 클리핑의 수용 기준을 새로 설정한 것은 아니다.

**Night Harvest의 극단적인 harmonic-gap A0+cab 조건은 별도 제한으로 남는다.** 48kHz의 PRESENCE 0.50→0.20, RESONANCE 0.50→0.80에서 25–40Hz 전체 전력은 +0.357dB지만, A0 ±1Hz의 좁은 전력은 −71.910→−88.006dBFS, 약 **−16.095dB**다. ±3Hz는 −8.240dB, ±5Hz는 −5.512dB다. 원래 좁은 성분은 전체 출력 RMS보다 약 58dB 낮았다. 이 결과를 저역 전체의 16dB 손실로 표현하면 부정확하지만, 넓은 대역 목표가 개별 기본음 성분의 보존을 보장하지 않는 실제 합성 스트레스 사례다. 실제 DI와 청취를 통해 중요도를 판단해야 한다.

## 6. 대안 대역 및 샘플레이트

25–160Hz 대안은 수정 소스에서 베이스 저역 상한 하나만 바꾼 별도 실행이다. Niflheimr 기본 채널 36조건을 계산했다. harmonic-gap 앰프 단독 여섯 조건에서 PRESENCE=0.20, DEPTH=0.50을 선택한다. 자체 25–160Hz 편차는 약 +0.74…+0.83dB지만, 같은 출력의 독립 25–100Hz 편차는 +2.09…+2.23dB다.

25–100Hz를 채택한 이유는 누락된 기본음 대역을 추가하면서 기존 100Hz 상한과 125/160Hz 몸통 대역의 역할을 유지하기 위해서다. 25–160Hz가 청감상 열등하다고 결론 내린 것은 아니다. 두 대안 모두 기존 곡선의 40Hz 아래 상수 연장을 사용하며, 이 목표는 실제 DI·청취 승인을 받지 않은 잠정 기준이다.

수정안도 모든 샘플레이트에서 같은 노브를 선택하지는 않는다. 기본 매트릭스의 동일 조건 80그룹 중 선택값이 다른 그룹은 기존 8개, 수정 5개다. 고게인 24그룹은 기존 5개, 수정 1개다. 대표적인 수정안 경계 사례는 Hot Lead/high-drive/B0/앰프 단독으로, 44.1·48kHz에서는 RESONANCE 0.50→0.20, 96kHz에서는 0.50을 유지한다. 44.1·48kHz에서 측정된 최소 개선량은 각각 **1.000736 / 1.004044dB**로 엄격한 1dB 채택 경계에 매우 가깝다. 이 차이를 없애기 위해 임계값을 낮추지 않았다.

## 7. Strength·Lock·UNDO AUTO

실제 `ChimeraProcessor`/`Controller`의 capture tap에 합성 신호를 넣는 57조건 회귀를 실행했다. Tight 515 및 Niflheimr, A0/B0, 세 샘플레이트에서 25/50/100% Strength를 확인했다. 두 노브 모두 초기 0.90에서 100%는 0.60, 50%는 0.75, 25%는 0.825로 적용되는 실제 양성 사례가 있다.

- Power lock은 두 노브를 유지하고 해당 amp pass를 건너뛴다.
- Amp lock만 설정하면 GAIN은 보존하면서 Phase 12는 실행된다.
- UNDO AUTO는 원래 전체 파라미터 스냅샷을 복원한다.
- capture 중/완료 후 사용자가 값을 바꾸었다가 돌려놓는 편집 이력, 새로 선택한 undo lock을 보존한다.
- 전체 Auto Set, 기존 Phase 12 기타 규칙과 새 베이스 규칙의 실행 결과는 아래 검증 상태에 별도로 기록한다.

기존 결과 행의 dB는 100% 제안값을 설명한다. 부분 Strength나 사용자의 편집으로 적용을 거부한 경우 실제 적용 상태를 다시 렌더한 표시가 아니다. 이번 회귀는 실제 파라미터 보간·편집 소유권·복원을 검증하며, 이 기존 표시 제한을 해결했다고 주장하지 않는다.

## 8. Auto 악기 분류의 별도 특성

공유 `measureSpectrum`의 자동 분류는 그대로다. 여섯 기본 채널 × 세 샘플레이트 × 두 음 × 세 입력 × 캐비넷 유무의 216조건에서 모두 playing으로 감지됐지만, 자동 Bass 분류는 **115/216**이었다.

| 헤드 | 자동 Bass 분류 |
|---|---:|
| Tight 515 | 14/36 |
| Cinder 120 | 17/36 |
| Night Harvest | 0/36 |
| Hot Lead | 33/36 |
| Náströnd | 15/36 |
| Niflheimr | 36/36 |

명시적 Bass는 216/216이었다. 따라서 주 비교 결과를 모든 Auto 모드의 저음 베이스 입력에 자동 적용할 수 없다. Niflheimr 기본 채널은 이번 36조건 모두 Bass로 분류됐다. 기타 계열 헤드를 베이스로 시험할 때는 Bass를 명시적으로 선택해야 이번 베이스 경로의 결과와 일치한다. 공유 악기 분류기의 개편은 이 PR에 포함하지 않았다.

## 9. 검증 상태와 재현

| 항목 | 상태 |
|---|---|
| 기본 네이티브 매트릭스 240조건 × 기존/수정 | 완료, 각 실행 종료 코드 0 |
| 고게인 Metal 72조건 × 기존/수정 | 완료, 각 실행 종료 코드 0 |
| 25–160Hz 대안 36조건 | 완료, 종료 코드 0 |
| 새 베이스 규칙: 해석적 6 + 통제 15 + 네이티브 6 | PASS, 종료 코드 0 |
| 1초 연주+2초 쉼, 무음/짧은/비유한 입력 | PASS |
| 합계 오차 조건 제거 음성 대조 | 예상한 실패 확인, 종료 코드 1 |
| 실제 컨트롤러 57조건 | PASS, 종료 코드 0 |
| 전체 `ChimeraAutoSetTests` | **PASS, 종료 코드 0** |
| Windows/macOS 실행 | 이 로컬 작업에서는 미실행; PR CI와 별도 구분 |
| 실제 기타·베이스 DI | 파일 미확보, 미실행 |
| 청취·실제 DAW·실물 캐비넷 | 미실행 |

로컬 환경은 Linux, GCC 13.3, JUCE 8.0.8, Release `-O2 -DNDEBUG -ffp-contract=off`다. 테스트는 DISPLAY를 사용하지 않는 콘솔 실행이다. 사전 `xvfb-run` 실행은 assertion PASS 후 가상 디스플레이 소켓/정리 문제로 wrapper 코드 1을 반환했으므로 최종 판정에는 사용하지 않았고, 실제 바이너리의 직접 실행 종료 코드를 기록했다. GUI 위젯 시험을 수행한 것으로 표시하지 않는다.

Phase 12의 후보 렌더 상한은 참조를 포함해 **14회 × 3초**로 유지된다. CSV의 `inner_analysis_render_calls`는 외부 played-reference 한 번을 제외하므로 1을 더해야 이 상한과 비교할 수 있다. `inner_analysis_ms`는 동시 빌드/시험 부하가 있는 측정이며 성능 보증이나 실시간 처리 시간으로 해석하지 않는다. 새 FFT 작업량은 늘었고, message-thread 분석·세션 레이트·amp oversampling 미적용이라는 기존 제한은 남는다.

### 기본 실행

```sh
cmake -S . -B build-phase12 -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-phase12 --target ChimeraAutoSetTests ChimeraAutoSetPowerBassProbe --parallel 2
./build-phase12/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --power-bass-rules
./build-phase12/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --power-bass-controller
./build-phase12/ChimeraAutoSetPowerBassProbe_artefacts/Release/ChimeraAutoSetPowerBassProbe --style rock --csv candidate-rock.csv
./build-phase12/ChimeraAutoSetPowerBassProbe_artefacts/Release/ChimeraAutoSetPowerBassProbe --driven --style metal --fixture harmonic_gap --default-channels-only --csv candidate-driven-metal.csv
```

기존 버전은 기준 SHA의 `Source/AutoSetPower.h`를 별도 경로에 저장하고 같은 probe의 첫 include만 그 파일로 지정해 컴파일했다. 다른 네이티브 DSP·분석기·JUCE 코드는 동일하다. 25–160Hz 대안은 `powerBassExcessDb`의 저역 상한만 160으로 바꾸어 같은 방식으로 컴파일한다. 기존 파일이나 제품 파라미터를 실행 도중 변경하지 않는다.

요약 재생성:

```sh
python3 Tools/summarize_auto_set_power_bass.py baseline-rock.csv candidate-rock.csv
python3 Tools/summarize_auto_set_power_bass.py baseline-driven-metal.csv candidate-driven-metal.csv
```

### 보존한 근거

[`docs/evidence/auto-set-phase12-bass-20261101/`](evidence/auto-set-phase12-bass-20261101/)에 전체 CSV(gzip), 기본/고게인 비교 JSON, Auto 분류와 Night Harvest 좁은 대역 CSV, 회귀 로그, 컴파일·소스 해시 provenance를 둔다. 추가 진단용 `diagnostic-runner.cpp`는 동일한 probe 신호를 재사용한다. `low_band_disagreements`는 움직임 방향·독립 목표 편차 증가·1.5dB 범위 이탈을 조합한 관찰 플래그이며 청감 판정이 아니다. 0.2dB는 결과를 분류하는 보고용 기준이고 제품의 1dB 채택 조건을 바꾸지 않는다.
