# Chimera 독립 멀티밴드 / Pro-MB 검증 — 2026-10-20 KST

> **2026-10-26 갱신 — Global ON/OFF 간섭 해결됨.** 아래 §4의 −28.6 dB / −59.4 dB
> dip은 2026-10-20 소스 기준이다. 이후 `63a72f4`(LR4 분석·밴드 gain 법칙·0 latency 유지,
> magnitude가 같은 minimum-phase 합성 출력, OFF 중 고정 합성 state warm-up,
> 10 ms quintic gain 전환)과 `ffab682`(약 6 kHz 계수 갱신 + 샘플 보간)로 수정되었다.
> main `cfeaa53`에서 `Tools/measure_multiband_bypass_fix.py --skip-cpu`를 다시 실행한 결과:
> 4 rate × 4 분할 × 9 tone × gain 3조합 × ON/OFF = 864개 전환 전부 undershoot ≥ −0.0001 dB,
> overshoot ≤ 0.0 dB(고정 baseline `3a942a1`: −59.385 / +3.722 dB). 중립 288개는 envelope 0 dB이며,
> 이전 후보가 실패한 40/56/78.4 Hz 밀집 분할도 포함한다. carrier 제거 step 지표 최대
> −36.02 dBFS(baseline −31.38 dBFS), 정적 임펄스 224쌍 PASS. 실제 DI 청취·DAW·CPU 재측정은
> 이 갱신에 포함되지 않는다. 아래 본문은 당시 기록으로 그대로 둔다.

## 판정과 범위

**정상 상태 합산과 내부 Parallel mix는 수치 검증 통과. Global ON/OFF 전환의 위상 간섭은 상당하며 미해결. Pro-MB 실제품 응답 비교와 음향적 동등성은 검증하지 않았다.**

- 대상: [Draft PR #110](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/110).
- 최종 확인·측정 기준 소스: `9e92124823a34d3432a74bde8cb7d9d5b4fe16d0`.
  작업 중 추가된 native UI test synchronization 수정을 반영했다.
  DSP header는 최초 확인한 `e91709d5`와 byte-for-byte 동일하며 측정을 재실행했다.
  사용자가 전달한 `9d5b7fc...` 이후의 수정과 이미 작성된 시험을 먼저 확인했다.
- 측정은 실제 `Source/MultibandDynamicsCore.h`를 C++로 컴파일해 수행했다.
  DSP를 Python으로 재구현한 결과가 아니다. 별도의 z-domain LR4 수식은 **검산용 oracle**이다.
- 생산 DSP·파라미터 ID/순서·default·state serializer·factory preset·latency reporting은 변경하지 않았다.
  이번 변경은 누락된 복소 응답/전환 회귀, 재현 가능한 측정 도구·결과, 정확한 UI 도움말이다.
- 기존 LR4 보상, phase-aligned dry, 4가지 동작, Peak/RMS, 6개 기능별 화면 및 호환성 검사를 중복 개발하지 않았다.
- Pro-MB 바이너리·버전 식별 정보·렌더된 테스트 오디오가 이 작업에 제공되지 않았다.
  설치된 Pro-MB/DAW를 실행하거나 동일 DI로 비교 청취하지 않았다.

## 1. 공식 기능과 현재 구현의 차이

아래 Pro-MB 열은 **제조사 공개 설명**이다. 내부 계수·회로·검출 구조를 알아냈다는 뜻이 아니다.

| 항목 | Chimera 최신 PR #110 | Pro-MB 공식 문서 | 판정 |
|---|---|---|---|
| 밴드 배치 | 3개 crossover가 전 대역을 4개 연속 밴드로 분할. 최소 주파수 비율 1.4, 위치 smoothing 20 ms | 최대 6개 밴드를 필요한 위치에 자유 배치. 밴드 사이 빈 구간, 인접 밴드의 crossover 공유 가능 [F1,F2] | 구조적 기능 차이. Matrix amp 라우트나 4개 Dynamic EQ 슬롯과 구별 |
| 필터 | 고정 LR4(각 split 24 dB/oct), 이후 allpass 보상 | 모드별 가변 slope 6–48 dB/oct, 양쪽 경계 개별 설정 [F1,F2] | 4→6개만 늘리는 것으로 해결되지 않음 |
| 검출 | 밴드 처리 경로에서 검출. Peak: 양 채널 절댓값 최대 + 30 ms peak decay. RMS: 각 채널 5 ms 지수 평균 energy 중 최대의 제곱근. 전 밴드 공통 Peak/RMS 선택 | Band/Free trigger, 내부/외부 입력, trigger Audition. 공개 설명만으로 Chimera와 같은 Peak/RMS 검출이라고 분류할 수 없음 [F3] | 원본 detector topology/계수/threshold 기준 미측정 |
| 스테레오 | 항상 100% linked, 두 채널에 같은 밴드 gain | 0–100% link, M/S-only 처리와 band output pan [F3,F4] | 외부 SC·free detector band·M/S 미구현 |
| 다이나믹 | 하향/상향 compression 및 expansion. 연속 soft knee, finite Range, Ratio, Makeup | Compress/Expand와 Range 부호 조합, Knee, Ratio, Output [F4] | 기능 종류는 대응. 동일 수치의 응답 일치는 미검증 |
| Attack/Release | ms 단위 gain-dB 1-pole time constant; detector 응답이 추가됨 | % 표시, program- 및 frequency-dependent attack/release [F4] | 숫자를 1:1 대응시킬 수 없음 |
| 위상 | unity gain에서도 정적인 IIR 위상 회전. neutral 합산은 allpass | Dynamic Phase / Linear Phase / Minimum Phase. Dynamic Phase는 무처리 때 flat phase라는 제조사 설명 [F5] | Chimera는 Dynamic Phase 또는 Linear Phase 동등 구현이 아님 |
| 지연 | 이 코어의 명시적 delay-line 지연 0 samples. 주파수별 group delay 존재 | lookahead OFF + oversampling OFF + Dynamic/Minimum Phase면 0 latency. lookahead ON이면 20 ms, Linear Phase/OS는 추가 가능 [F4,F6] | 코어 0 samples를 전체 플러그인·인터페이스·DAW 왕복 지연 0으로 확대 금지 |
| Lookahead/OS | 이 멀티밴드 코어에는 없음. 다른 엔진의 oversampling을 이 기능으로 계산하지 않음 | 밴드별 lookahead 최대 20 ms, 전역 활성화; 최대 4x oversampling [F4,F6] | transient/alias 비교 미실행 |
| Mix | 0–100%, 각 위상 보상된 밴드에서 선형 amplitude blend. 0%도 crossover phase와 solo 유지 | 0–200%, 전체 dynamic/static gain 변화량 조절 [F7] | Pro-MB의 실제 interpolation 법칙을 추정하지 않음 |
| Bypass | 밴드 Bypass는 phase 유지. Global ON/OFF는 원음↔위상 회전 경로 10 ms linear crossfade, OFF 끝은 bit-exact | 전역 soft bypass와 latency compensation 제공 [F7] | 원본 전환 파형/시간은 미측정. Chimera의 dip은 아래 실측 |
| Spectrum | 기능별 실제 IN/OUT stereo-power FFT, 2048/4096/8192, 25 Hz UI timer, smoothing, 마지막 완성 프레임 Freeze. 밴드 선택·crossover drag·선택 밴드 GR | Pre/Post/SC, FFT 1024–8192, Speed/Tilt/Freeze, 수평 zoom, dynamic response curve와 range 직접 조작 [F2,F8] | 동일 UI/응답 아님. Chimera Freeze는 snapshot; Pro-MB 문서는 max-over-time. SC·tilt·수평 zoom·전체 dynamic curve 차이 |

현재 Chimera 화면의 주파수 마커는 crossover 위치다. IN/OUT 스펙트럼과 gain-reduction 수치는 별도 정보이며, 실제 전달함수와 동일한 정적 EQ 곡선이라고 부르지 않는다. 모든 밴드의 GR 동시 표시도 현재 선택 밴드 수치와 구별해야 한다.

## 2. 측정 방법과 재현

```sh
python Tools/measure_multiband_response.py --output /tmp/chimera-mb-audit
# GCC / numpy / matplotlib 필요. 원본 Pro-MB를 실행하지 않는 offline core audit.
g++ -O2 -std=c++20 -ISource Tests/MultibandDynamicsCoreTests.cpp -o /tmp/mb-tests
/tmp/mb-tests
```

- 샘플레이트: 44.1 / 48 / 96 / 192 kHz.
- crossover 4조합: `(120,700,3500)`, `(40,56,78.4)`, `(1000,1400,1960)`, `(7000,10000,16000)` Hz.
- 65,536-sample impulse, 20 Hz–min(20 kHz, 0.45 Fs)의 모든 FFT bin.
- neutral 16회, uniform −6 dB / differentiated `[-6,+3,-9,+6] dB` × Mix `0/25/50/75/100%` × 16 = 160회. 총 176개 impulse render.
- ratio=1로 nonlinear gain 검출을 제거하고 **실제 필터/Makeup/Mix 경로**를 측정.
  dynamic 모드/범위/선택성/스테레오/무할당은 기존 core tests를 함께 실행했다.
- transient 72회: 6개 tone × 4 rate × ON/OFF/all-band bypass.
  0.5 s 사전 입력 후 40 ms 기록. 서로 독립적인 mono sin/cos 측정의 제곱합 제곱근으로 순간 envelope를 복원한다.
  이 방법은 여기서의 neutral linear time-varying 처리에만 적용한다. 압축된 음악의 loudness가 아니다.
- 모든 수치, compiler/platform, source SHA, header SHA-256 및 측정 도구 hash:
  [measurements.json](../Validation/MultibandAudit/measurements.json).
- [응답·군지연·전환·시간 그래프](../Validation/MultibandAudit/response.png).

이는 유한한 rate/crossover/gain 조합의 synthetic measurement다. 모든 자동화 상태의 상한, 청감 무색성, 모든 호스트 동작을 증명하지 않는다.

## 3. 밴드 합산·Dry/Wet·위상

보상된 경로는 `[A3 A2 L1, A3 L2 H1, L3 H2 H1, H3 H2 H1]`이며
`Li + Hi = Ai`다. 따라서 settled unity sum은 `A3 A2 A1`이다.
allpass 합산은 **magnitude만 평탄**하며 원래 impulse와 null 되지 않는다.

| 측정 | 결과 | 의미 |
|---|---:|---|
| neutral magnitude 최대 절대 오차 | 4.965e-7 dB 미만 | 검사한 정적 분할에서 합산 notch 없음 |
| uniform gain 내부 mix의 기대 응답 오차 | 5.854e-7 dB 미만 | 위상이 다른 raw dry를 섞는 결함 재현 안 됨 |
| 서로 다른 4밴드 gain/mix의 기대 응답 오차 | 1.059e-6 dB 미만 | weighted sum도 별도 z-domain 식과 일치 |
| 48 kHz 기본 neutral impulse, 첫 nonzero sample | 0 | 추가 delay line 없음 |
| 같은 impulse의 원음 대비 residual L2 | 1.049007 | 무처리 원음과 동일하지 않음. RMS나 dBFS가 아님 |
| 48 kHz 기본 분할, 20 Hz–20 kHz 최대 group delay | 약 5.305 ms | frequency-dependent IIR delay; PDC에 더할 일정 지연이 아님 |

48 kHz, 기본 crossover, unity gains의 정확한 주파수 DFT 결과:

| Hz | phase (wrapped °) | group delay ms | raw dry와 외부 50:50 합산 시 dB |
|---:|---:|---:|---:|
| 31 | −51.343 | 4.754 | −0.903 |
| 40 | −66.959 | 4.888 | −1.576 |
| 80 | −141.208 | 5.302 | −9.575 |
| 120 | +146.510 | 4.539 | −10.809 |
| 700 | +175.763 | 0.889 | −28.645 |
| 3500 | −142.273 | 0.165 | −9.808 |
| 10000 | +62.024 | 0.024 | −1.340 |

이 표의 마지막 열은 `0.5 + 0.5 H(f)` 검산이며 **Chimera 내부 Parallel mix의 결과가 아니다**.
DAW에서 원음을 따로 병렬 합산하면 cancellation이 생길 수 있다. 일정 sample delay 하나로 위상 회전 전부를 맞출 수 없다.

내부 Mix=0과 모든 밴드 Bypass는 동일한 phase reference를 유지한다(별도 Solo가 없을 때 neutral sum).
Global OFF만 원래 샘플을 그대로 반환한다. 기본 off인 구형 프로젝트의 소리는 이 차이에 영향을 받지 않는다.

## 4. Bypass: 수치상 중요한 미해결 한계

48 kHz에서 Global OFF는 480 samples(10 ms)에 걸쳐
`y=(1-a)x+a Hx`로 전환한다. 두 경로 위상이 반대에 가까우면 중간에
상쇄된다. coefficient smoothing 또는 click-free라는 말로 이 notch를 숨길 수 없다.

| tone Hz | OFF 전환 최소 envelope dB | OFF 전환에서 −3 dB 이하인 시간 | cold ON 전환 최소 dB |
|---:|---:|---:|---:|
| 40 | −1.576 | 0 ms | −3.683 |
| 80 | −9.575 | 6.646 ms | −9.021 |
| 120 | −10.809 | 6.771 ms | −17.042 |
| 700 | **−28.645** | **7.063 ms** | **−59.385** |
| 3500 | −9.808 | 6.646 ms | −9.980 |
| 10000 | −1.340 | 0 ms | −1.367 |

- Global OFF 종료 후 원음은 bit-exact로 복귀한다. 새 regression은 4 rate에서 정확한 종료 sample과 host block partition 독립성, 기대 crossfade 외의 추가 불연속이 없음을 확인한다.
- cold ON은 bypass 동안 splitter를 처리하지 않다가 state를 reset하므로 filter startup까지 포함한다. 10 ms가 지나도 즉시 완전한 정상 상태라고 볼 수 없다(80 Hz 시험의 이후 10–40 ms 구간 최대 편차 약 0.264 dB).
- 모든 밴드에 −6 dB Makeup 후 **밴드 Bypass**하면 48 kHz의 시험 tone 전부가 −6→0 dB 범위로 이동했다. Global OFF의 위상 상쇄 notch와 다르다.
- 회귀 테스트의 `CHARACTERIZATION` 출력은 이 한계가 **통과했다는 음질 판정이 아니다**.

### 이번 개선과 엔진 유지 이유

기존 UI 도움말에 전체 ON/OFF, 밴드 Bypass, Parallel mix=0의 차이를 명확히 넣었다.
다이나믹 적용 전후 비교에는 기존 Parallel mix 또는 밴드 Bypass를 이용할 수 있다.
새로운 중복 bypass 기능이나 host parameter는 추가하지 않았다.

전환 시간만 줄이면 dip이 짧아질 뿐 사라지지 않고, 오히려 더 빠른 변화가 생긴다.
항상 allpass를 켜 두면 exact raw bypass와 idle CPU 동작이 바뀐다.
raw dry 기반의 residual filter로 무단 교체하면 이미 작성한 multiband preset의 처리 응답이 바뀐다.
이를 검증 없는 음질 개선으로 채택하지 않았다. **전체 Bypass 간섭은 이번 수정으로 해결됐다고 주장하지 않는다.**

후속 개선은 기존 엔진을 유지하는 명시적 선택 모드/전환 설계로 검증해야 한다.
필수 기준은 neutral/가변 gain 응답, 전환 dip·overshoot·click, 실제 DI 청취,
latency/PDC·callback budget·state migration이다. 독자 zero-neutral-phase 설계나
linear-phase 모드도 제품 선택지일 수 있지만, 이를 FabFilter Dynamic Phase로 명명하거나 내부 알고리즘을 추정하지 않는다.

## 5. 검출 및 시간 응답

48 kHz, 6 kHz sine, peak −42→−6→−42 dBFS, 변동 시점 0.5/1.5 s.
최고역 밴드만 처리, crossover 80/400/1200 Hz, Threshold −24 dB,
Ratio 4:1, Knee 0, Range 36 dB, Attack 10 ms, Release 120 ms.
기록값은 실제 코어의 적용 GR이다. `63%`는 plateau 변화량의 `1−e^-1` 기준이며 90%와 구별한다.

| detector | plateau GR dB | attack 63% ms | attack 90% ms | release 63% ms | release 90% ms |
|---|---:|---:|---:|---:|---:|
| Peak | 13.487 | 9.979 | 23.042 | 152.354 | 308.667 |
| RMS | 11.239 | 12.792 | 26.292 | 128.750 | 285.063 |

Peak/RMS의 약 2.248 dB GR 차이는 sine의 peak/RMS 레벨 차이와 4:1 slope에 부합한다.
RMS의 5 ms energy averaging, Peak의 30 ms decay와 gain smoothing을 함께 거치므로
Release 120 ms를 '출력 완전 회복까지 120 ms'라고 설명하면 부정확하다.
해당 UI 도움말도 time constant와 실제 관측 응답을 구분하도록 수정했다.
이 값은 특정 carrier/step의 결과다. 모든 주파수나 프로그램에 일반화하지 않으며 Pro-MB % 값과 맞춘 결과도 아니다.

## 6. 회귀·호환성·플랫폼 상태

이번 추가 검사는 기존 `ChimeraMultibandDynamicsCoreTests` 타깃 안에 들어가므로
새 중복 CI workflow가 필요 없다. 기존 Graphical EQ / Signal Path workflow가 실행한다.

- 독립 z-domain 기준에 대한 **복소수 응답** 검사: magnitude뿐 아니라 phase, unequal band gain, mix 0/50/100%, 저역 밀집/고역 분할, 4 rate.
- zero mix = all-band bypass 경로, raw bypass와의 구별.
- Global OFF ramp: 단일/1-sample 블록 결과 동일, 10 ms exact raw endpoint, 정의된 crossfade 외 추가 불연속 검사. dip은 characterization으로 남김.
- 기존 band selectivity, 4-mode curves, signed meter, range, de-esser, malformed parameters, allocation=0 검사 유지.
- Linux GCC 최적화 core suite PASS; ASan/UBSan PASS(`detect_leaks=0`, 따라서 leak 검사 미실행).
- 수정된 UI header를 실제 PluginEditor.cpp / JUCE 8.0.8 compile flags로 syntax compile했다. 이것은 UI 청취·DAW·배포판 검사가 아니다.
- 호스트 파라미터, 저장/복원, factory preset, DSP header는 변경하지 않았다. 기존의 5,026개 prefix + 55개 append/version 13 규약을 유지한다.

확인한 **기준 SHA e91709d5**의 [Graphical EQ CI run 39387327081](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39387327081)은 Windows / macOS / Linux 3개 job 모두 success다.
해당 타깃에는 MultibandDynamics 통합/core 및 CommonDynamics 검사가 포함된다.
이 결과는 **이번 새 회귀를 추가한 커밋의 CI PASS로 승계하지 않는다**.
동일 기준 SHA의 CAB realtime workflow에는 별도 실패가 있어 전체 CI 통과라고 부르지 않는다.
최종 PR HEAD의 CI는 그 커밋의 실제 결과로 별도 확인해야 한다. Draft/미병합/미공개 상태를 유지한다.

## 7. Pro-MB 원본 측정 대기 항목

| 다음 비교 | 필요한 고정 조건 / 결과 |
|---|---|
| neutral / 처리 phase | 설치 Pro-MB 버전·format·host·sample rate, phase mode, slope, inactive-band 조건, unity 및 unequal gain impulse/복소 sweep |
| detector/threshold | RMS로 교정한 sine/다중 tone, peak 및 noise, 위상 반대 stereo, L/R level 차이, Band/Free 및 link 설정별 GR |
| attack/release | 충분한 settle, carrier 주파수별 level step, attack/release % 전 범위, GR 10/63/90% 및 output envelope. Chimera ms와 단순 매칭 금지 |
| lookahead / latency | host reported PDC와 impulse onset/centroid 구분, lookahead/OS/phase mode 별 실제 지연, bypass·automation 중 PDC 변경 |
| sidechain | 동일 외부 입력/내부 free filter의 stimulus, detector audition, process band와 trigger band 분리 |
| mix / bypass | 0/50/100/200% 및 static/dynamic gain, global/band/host bypass, 전환 dip·overshoot·회복시간 |
| spectrum | 동일 입력·FFT/window/tilt 조건, pre/post/SC, phase-opposed stereo, freeze 의미, 저역 분해능·UI update/capture latency |
| 실제 품질 | 동일 기타/베이스 DI, blind level-matched 비교, 저역 note/transient/연속음, DAW 저장/자동화/재실행, callback cost |

지금 확보한 결과는 **Chimera의 실제 독립 코어 측정 + Pro-MB 공식 사양 대조**다.
FabFilter 바이너리 응답·독자 알고리즘·음향적 동등성을 확인한 결과가 아니다.

## 공식 출처 (2026-10-20 열람)

- [F1 — Pro-MB 소개와 주요 기능](https://www.fabfilter.com/help/pro-mb)
- [F2 — Display and workflow](https://www.fabfilter.com/help/pro-mb/using/display)
- [F3 — Expert band controls](https://www.fabfilter.com/help/pro-mb/using/expertbandcontrols)
- [F4 — Basic band controls](https://www.fabfilter.com/help/pro-mb/using/basicbandcontrols)
- [F5 — Processing mode](https://www.fabfilter.com/help/pro-mb/using/processingmode)
- [F6 — Oversampling](https://www.fabfilter.com/help/pro-mb/using/oversampling)
- [F7 — Input and output options](https://www.fabfilter.com/help/pro-mb/using/outputoptions)
- [F8 — Spectrum analyzer](https://www.fabfilter.com/help/pro-mb/using/analyzer)
