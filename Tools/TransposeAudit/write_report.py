#!/usr/bin/env python3
import json,tarfile,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];base=ROOT/'build/transpose-audit';ev=ROOT/'docs/evidence/transpose-pr125-pr128-audit-20261023'
s=json.loads((ev/'summary.json').read_text());t=json.loads((ev/'timing.json').read_text());q=json.loads((ev/'quality.json').read_text());p=json.loads((ev/'partition.json').read_text())
labels={'baseline':'공개판 Classic','tight125':'#125 Tight','corr40-split-rt':'#128 RT'}
fmt=lambda x: '<0.001' if abs(x)<.001 else f'{x:.2f}'
table='\n'.join(f"| {r['note']} | {labels[r['engine']]} | {r['attack_extra_max_ms']:.2f} | {r['lowband_extra_max_ms']:.2f} | {fmt(r['harmonic_pitch_abs_max_cents'])} | {r['harmonic_warble_max_cents']:.2f} |" for r in s['summary'])
cpu='\n'.join(f"| {labels[r['engine']]} | {r['mean_us_range'][0]:.1f}–{r['mean_us_range'][1]:.1f} | {r['budget_pct_range'][0]:.2f}–{r['budget_pct_range'][1]:.2f}% | {r['p99_us_range'][0]:.1f}–{r['p99_us_range'][1]:.1f} | {r['cpu_misses']:,} / {r['wall_misses']:,} |" for r in s['cpu128'])
fresh='\n'.join(f"| {r['note']} | {r['fresh_attack_max_ms']:.3f} | {r['fresh_lowband_max_ms']:.3f} |" for r in s['summary'] if r['engine']=='corr40-split-rt')
block32=[]
for eng in labels:
 rr=[r for r in t['results'] if r['block']==32 and r['engine']==eng];block32.append(f"| {labels[eng]} | {min(r['timing']['cpu_p99_us'] for r in rr):.1f}–{max(r['timing']['cpu_p99_us'] for r in rr):.1f} | {sum(r['timing']['cpu_misses'] for r in rr)} / {sum(r['timing']['wall_misses'] for r in rr)} |")
peaks='\n'.join(f"- {labels[r['engine']]}: CPU peak {r['peak_cpu_us']/1000:.3f}ms, wall peak {r['peak_wall_us']/1000:.3f}ms, 총 {r['callbacks']:,} callbacks." for r in s['cpu128'])
text=f'''# Transpose PR #125 / #128 동일 조건 검증 — 2026-10-23 KST

**판정: 음질을 포함한 전체 추가 지연 ≤10ms 달성은 입증되지 않았다. beta.2 공개 승인 근거로 사용할 수 없다.**

#125의 짧은 창은 실제 구현됐고 보고 지연도 줄었다. 그러나 B0/A0/A1에서 음정·저역 품질이 무너지는 조건이 있다. #128은 합성 단음의 기음과 어택에서 훨씬 유망하지만 업시프트 저역, 가변 지연/PDC, 블록 의존성과 실제 연주·호스트 검증이 남아 있다. 두 결과는 측정 대상이 달라 직접 모순되지 않는다. 연구 단계의 유망함을 제품 통합·릴리스 PASS로 승격하지 않는다.

## 1. 소스·실행 범위

| 대상 | 고정 커밋 / 구현 |
|---|---|
| 공개 v1.3.2-beta.1 | `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, 원본 `Source/PolyPitch.h` |
| PR #125 | `1ef032c441b06cac5c44076e8c231323f5e63749`, Tight analysis 480 / FFT 512 / hop 60 @48kHz |
| PR #128 | `9d28b06c3cb6498724d6618d08bebac1771f63eb`, `corr40-split-rt`, LPC·upshift·retrigger·splice coordination 모두 활성 |
| JUCE | 8.0.8, `d6181bde38d858c283c3b7bf699ce6340c050b5d` |

두 PR은 조회 시 open/미병합(#128 Draft)이었다. 최신 공장 프리셋 유지보수 소스 `50e78360`의 PolyPitch blob도 공개 기준과 동일한 `8149d7201376f8b4e6cdf89308be9e03e8fb40fe`임을 확인했다.

Linux x86_64 공유 KVM, Xeon Platinum 8370C, GCC 13.3 `-O2`. 제품 DSP를 Python으로 흉내 내지 않고 원본 C++ 헤더를 함께 컴파일했다. 동시 이름 충돌을 피하기 위한 namespace 변경만 수행했다. 후보 DSP 상수나 알고리즘은 수정하지 않았다.

- 품질/지연 219 renders, 별도 블록 진단 120 renders.
- CPU는 원래 연구 프로브 117회, 제품의 `ScopedNoDenormals`를 반영하고 vCPU 하나에 고정한 후 117회. 각 조건 18초 × 3반복, 엔진 실행 순서를 회전했다. 두 CPU 실행 모두 빌드·기능·품질 시험과 분리했다.
- 기본 조건: 48kHz / block128 / stereo R=−0.65L / B0 30.867706Hz, A0 27.5Hz, A1 55Hz / −2·−1·+1·+2반음.
- 같은 float32 입력 파일을 세 엔진에 전달했다. 음정·onset 정답은 오프라인 채점기에만 제공했다. 입력·출력·실행파일·콜백 파일 SHA-256은 원자료에 기록했다.
- 기존 `weak_attack_study.generate`, `legacy_metrics.tone_metrics/impulse_metrics`, `evaluate.attacks`, `weak_attack_holdout.lowband_extra`를 재사용했다. 각 입력의 원신호 어택 편향을 뺐다. 시간 정렬·출력 gain 보정으로 실패를 숨기지 않았다.

## 2. 동일 입력 비교

아래는 **±1/±2 네 설정의 최악값**이다. 어택/저역은 75ms 반복·강약 교대 피킹, 음정/워블은 8배음 지속음이다. 서로 다른 입력 계열의 수치를 한 사건의 측정값으로 해석하지 않는다.

| 음 | 엔진 | 고역 어택 추가 지연 ms | 저역 에너지 추가 지연 ms | 기음 오차 절댓값 cents | 워블 폭 cents |
|---|---|---:|---:|---:|---:|
{table}

![동일 조건 지연 비교](evidence/transpose-pr125-pr128-audit-20261023/delay-comparison.png)

**지표 해석:** 고역은 1–8kHz의 2ms RMS peak, 저역은 인과적 4차 120Hz 저역통과 뒤 음별 에너지 무게중심의 input-relative 차이다. 저역 값은 출력 반응의 진단 지표이며 물리적 transport delay나 청감 onset의 정확한 측정값은 아니다. 주파수 변경·이벤트 창 경계의 영향도 받는다. 알려진 10ms 순수 지연 대조군에서 고역은 정확히 10ms, 저역은 최저 약 9.44ms로 측정돼 창 편향이 확인됐다. 이 한계를 포함해 해석해도 전체 통과를 입증할 수 없다.

기음은 시작 후 0.75–2.75초에서 목표 주변의 지배 성분을 찾았다. 따라서 이 수치는 **정착 후 정확도**이며 새 음의 기음 획득 시간을 뜻하지 않는다. 독립 ideal sine 대조군은 모두 통과했다. 워블은 6Hz 복조 기반 p95−p5 proxy다. 목표 성분이 약하거나 크게 이탈한 출력에서는 독립적인 청감 점수로 해석하면 안 된다. A1 Tight처럼 낮은 워블과 잘못된 음정이 동시에 나타날 수 있다.

### #125의 중요한 실패

B0 −2반음의 목표는 27.5Hz인데 주요 저역 성분은 **31.3592Hz, +227.35 cents**였다. 목표 주파수의 coherent 성분은 **−47.43dB**, 목표 주변 저역 band power는 **−8.64dB**였다. detuning 때문에 coherent 성분이 작아지는 것과 전체 저역 에너지 손실은 구분해야 한다.

#125 테스트의 레벨 비교는 110/220/440Hz 사인 및 이전 짧은 창과의 비교다. B0/A0/A1의 품질 보증이 아니다. 제로 패딩은 FFT grid를 제공하지만 10ms 창에 실제 저음 관측 정보가 추가되는 것은 아니다. 이것이 모든 10ms pitch shifter가 불가능하다는 뜻은 아니다.

약한 기음 입력에서 최대 오차는 공개판 550.47 cents, Tight 393.96 cents, RT <0.001 cents였다. RT도 이 입력의 워블 proxy는 최대 3.09 cents로, 일반 배음 지속음의 0.59 cents와 같지 않다. 모든 세부 설정은 `per-condition.csv`에 있다.

### 새 holdout

후보를 고정한 다음 seed **2026102307**로 간격·위상·감쇠와 강약을 바꿨다. 후보를 이 결과로 다시 튜닝하지 않았다.

| #128 RT 음 | 고역 추가 지연 최대 ms | 저역 추가 지연 최대 ms |
|---|---:|---:|
{fresh}

A1 +1은 10.235ms, +2는 **10.684ms**다. 고역 onset 192/192개는 10ms 안에 대응됐지만 B0의 한 위치에서 각 shift마다 추가 reset이 1개씩, 총 **4개 unmatched reset**이 있었다. 누락 0만으로 오검출 0을 주장하면 안 된다. Tight의 새 holdout 저역 최대는 **13.221ms**였다.

Tremolo는 의도적인 진폭 변조이므로 정상 지속음의 워블 한계로 채점하지 않았다. RT의 B0/A0/A1 proxy는 각각 80.17/208.60/15.65 cents로 재현됐다. 이 값을 개선 또는 청감 PASS로 포장하지 않는다.

## 3. 보고 지연·관측 지연·PDC·왕복 지연

| 구분 | 공개 Classic | #125 Tight | #128 RT |
|---|---|---|---|
| 코어 `latency()` @48kHz | 2048 samples = 42.667ms | 480 samples = 10.000ms | −1 = 가변 지연, 고정값 미정 |
| 임펄스 에너지 중심 범위, ±1/±2 | 42.60–42.78ms | 9.925–10.096ms | 2.365–4.836ms |
| 임펄스 95% 에너지 도달, 최악 | 58.17ms | 13.67ms | 5.125ms |
| 임펄스 5–95% 퍼짐, 최악 | 31.33ms | 6.98ms | 0.688ms |
| 호스트 보고/PDC | 기존 제품 경로 존재 | 제품 소스가 선택 모드 지연을 체인 합계에 반영 | 제품 경로 미통합, 고정 PDC 계약 없음 |
| 실제 DAW 보상/자동화/재호출 | 이번 시험 미실행 | 이번 시험 미실행 | 이번 시험 미실행 |
| 장치 포함 전체 왕복 지연 | 미측정 | 미측정 | 미측정 |

#125는 단지 표시값만 바꾼 변경이 아니다. 실제 input/OLA/analysis 길이를 줄였고 임펄스 중심도 약 10ms로 이동했다. 그러나 480샘플이라는 보고값이 어택·저역·95% 에너지 도달을 전부 10ms 안에 보장하지 않는다.

`PluginProcessor.cpp`는 transpose 모드를 설정한 뒤 `postFX + engine + PRE/board` 합계를 `setLatencySamples()`에 보고한다. 따라서 480샘플을 **전체 플러그인 보고 지연**이나 **전체 왕복 지연**으로 쓰면 틀린다. 상태가 고정된 한 모드의 nominal delay와, bypass/모드 변경 시 호스트가 실제로 보상을 갱신하는지는 별도 검증이다. PDC는 트랙 정렬을 위한 보상이며 라이브 입력의 처리 대기를 없애지 않는다.

#125의 Transposer wrapper를 발췌 실행한 B0 −2 피킹 출력은 raw Tight와 bit-identical했다(첫 onset 400ms로 warmup 뒤). Classic125와 PR128의 기본 PolyPitch는 비교한 B0 지속음·피킹에서 공개 원본과 bit-identical했다. 이 국소 확인을 전체 plugin/DAW 회귀로 확대하지 않는다.

## 4. CPU 결과와 한계

제품과 같은 denormal 설정을 적용한 후의 **Transpose 코어만의** 측정이다. block128 기한은 2666.67µs다. 범위는 12개 음×shift 조건 각각의 3반복 중앙값 범위이며, 초과 횟수는 반복 전체를 합산했다. `%`는 해당 오디오 처리 기한 대비 thread CPU 비율이며 OS 전체 점유율이 아니다.

| 엔진 | 평균 thread CPU µs | 기한 대비 평균 | thread CPU p99 µs | thread CPU / wall 초과 횟수 |
|---|---:|---:|---:|---:|
{cpu}

{peaks}

추가 block32, A1 +2, 기한 666.67µs, 각 3반복:

| 엔진 | thread CPU p99 범위 µs | thread CPU / wall 초과 합계 |
|---|---:|---:|
{chr(10).join(block32)}

RT의 평균 처리비용 이점은 이 조건에서 확인된다. 다만 모든 초과를 scheduler 탓으로 지우지 않았다. thread CPU 초과도 별도로 남아 있다. 공유 VM, CPU 이동/경합·가상화 계측·전원 상태 등 영향의 정확한 원인은 분리하지 못했다. **실제 타깃 Windows/macOS에서의 실시간 안전, full plugin/Matrix CPU 또는 최대 인스턴스 수는 미검증**이다.

원래 연구 프로브의 denormal 설정 없는 첫 117회는 `timing-original-probe.json`에 보존했다. 제품은 `processBlock()`에서 `ScopedNoDenormals`를 사용하므로 최종 비교에는 두 번째 실행을 우선했다. 두 실행은 시간·CPU affinity도 다르므로 차이를 FTZ 하나의 인과효과로 계산하지 않는다.

## 5. 회귀 및 새로 확인한 통합 차단 요인

- 기존 `CorrelationTests` 270그룹, 공개 경로 `PitchRegressionTests`, `StabilityTests`, `GateTests` **PASS**.
- #125의 정확한 `PitchRegressionTests.cpp` **PASS**. 44.1/48/96/192kHz nominal ≤10ms와 동길이 arithmetic/레벨 계약의 국소 통과다.
- 기존 CorrelationTests는 새 LPC/retrigger/coordination 옵션을 켜지 않는다. 테스트 어댑터만 확장해 최신 RT 옵션을 켠 `CorrelationTestsRT`는 **FAIL: partition-dependent output**.
- 별도 44.1/48/96kHz, ±1/±2, block32/64/128/256/257 지속음 비교: block32 기준과의 96개 비자명 비교 중 **RT 다운시프트 24개가 불일치**. LPC control은 48/48 동일, RT 업시프트는 24/24 동일했다.
- 최대 표본 차이는 3.3503e−5, 약 **−89.50dBFS**로 이 지속음 사례에서는 작다. 청감 결함으로 과장하지 않되 bit-identical 블록 불변성 계약은 실패다.
- 원인 위치: `TransientSplitPitch::process()`가 high 경로를 chunk 전체 처리한 뒤 `transient.splices()!=transientSplices`를 body 전체 chunk의 hold 조건으로 사용한다. chunk 크기가 호스트 block/capacity에 따라 달라져 body splice 시점이 달라진다. 최신 옵션의 sample 단위 일관성 또는 고정 내부 chunk 경계를 검증해야 한다.
- 이번 작업에서 새 sanitizer/실제 plugin host 시험은 수행하지 않았다. PR에 있는 이전 sanitizer PASS를 이번 확장 조건의 PASS로 자동 승계하지 않는다.

## 6. 달성 가능성과 제품 반영 판단

**목표 자체가 보편적으로 불가능하다는 결론은 내릴 수 없다.** #128의 T/2 논의는 해당 주기 정렬·read-head 모델의 조건부 설명이고, 24–48ms pitch-estimation 결과는 선택한 추정기·잡음·신호 모델의 실험이다. 모든 causal pitch shifter가 새 f0를 먼저 확정해야 한다는 보편적 증명은 아니다. 그 문서의 목표 재정의 제안을 이번 #97 승인 조건으로 채택하지 않는다.

실용적으로는 #128이 B0/A0 단음의 기음·피킹을 개선한 유망한 출발점이다. 다만 A1 업시프트 저역 초과와 가변 지연, onset extra reset, 다성부/레가토/실 DI의 미검증 때문에 **모든 입력에서 음질을 유지하는 ≤10ms 제품 기준의 실현 가능성은 아직 입증되지 않았다.**

| 범위 | 판단 / 필요한 작업 |
|---|---|
| #125 analysis/FFT 길이 분리 | 옵션형 실험 인프라로 검토 가능. 현재 Tight를 저음용 완료품이나 #97 해결로 홍보/기본 승격하지 않음. B0/A0/A1 정확도·에너지 기준을 회귀에 추가해야 함. |
| #128 splice-search 최적화 / LPC onset | 연구 경로에서 유지할 가치 있음. 현재 제품 PolyPitch에 그대로 적용할 수 있는 동일 엔진 변경은 아님. |
| #128 제품 DSP 교체 | 현 상태 통합 보류. block 불변성, 업시프트 저역, 낮은 주파수의 시간 변화, crossover 정렬, interpolation aliasing, 채널/mono·stereo 동작 검증 필요. |
| PDC 및 전환 | 한 값으로 보상 가능한 신호 경로 계약을 먼저 정의. 단순히 보고값을 10ms로 설정하거나 40ms padding으로 목표 달성 처리하지 않음. wet/dry·bypass·모드/shift 자동화·session recall 검증 필요. |
| beta.2 | #97 P0 완료/공개 승인 불가. 다른 P0, 실제 패키지/호스트 검증을 대체하지 않음. |

다음 우선순위는 **RT의 블록 독립성 보정 → 업시프트·약한 새 음의 저역 시간 반응/추가 reset 개선 → 고정 PDC 및 전환 계약 → 동일 DI/Neural DSP 원본·실제 DAW 검증**이다. 개선 과정에서도 현재 실패를 재현하는 입력과 새 holdout을 보존하고 목표를 완화하지 않는다.

## 7. 확보하지 못한 조건

- 식별 가능한 isolated bass/guitar DI, Azhi Dahaka Low-B −2용 원본 DI 및 레벨/체인 정보 미확보.
- 제품명·버전·preset·sample rate·buffer와 SHA가 연결된 Neural DSP 원본 입출력 캡처 미확보. 이 환경에서 원본 플러그인도 실행하지 않았다.
- 저장소의 Transpose 측정 도구/문서는 합성 자료다. 파일 제목 검색에서는 기존 두 Transpose 연구 ZIP과 Azhi FLAC 항목을 찾았으나, 해당 FLAC이 isolated DI로 확인된 자료는 아니어서 사용하지 않았다. 자료가 어디에도 없다는 단정은 아니다.
- 청취 평가, 실제 인터페이스/드라이버 loopback, DAW PDC on/off null/정렬, 자동화·저장/재열기, 타깃 하드웨어 장시간 재생은 미실행.

## 재현 자료

[재현 절차](../Tools/TransposeAudit/README.md) · [요약 JSON](evidence/transpose-pr125-pr128-audit-20261023/summary.json) · [전체 조건 CSV](evidence/transpose-pr125-pr128-audit-20261023/per-condition.csv) · [품질/이벤트 원자료](evidence/transpose-pr125-pr128-audit-20261023/quality.json) · [CPU 원자료](evidence/transpose-pr125-pr128-audit-20261023/timing.json) · [블록 진단](evidence/transpose-pr125-pr128-audit-20261023/partition.json) · [전체 콜백 로그](evidence/transpose-pr125-pr128-audit-20261023/callbacks.tar.gz) · [선별 합성 WAV](evidence/transpose-pr125-pr128-audit-20261023/synthetic-audio.tar.gz).

선별 WAV는 B0 −2/A1 +2 강약 교대 입력과 세 엔진 출력이다. 실제 DI 또는 Neural DSP 출력이 아니다. gain/시간을 보정하지 않은 수치 재현용 파일이며 level-matched listening acceptance는 수행하지 않았다.

원본: [PR #125](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/125), [PR #128](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/128), [요구사항 #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97). 제품 코드·버전·배포 게이트·공개 상태는 이번 검증으로 변경하지 않았다.
'''
(ROOT/'docs/TRANSPOSE_PR125_PR128_AUDIT_20261023_KO.md').write_text(text)
with tarfile.open(ev/'synthetic-audio.tar.gz','w:gz') as tar:tar.add(base/'quality/audio',arcname='synthetic-audio')
# Hash every stored evidence file after the report assets are complete.
checks={str(f.relative_to(ev)):hashlib.sha256(f.read_bytes()).hexdigest() for f in ev.rglob('*') if f.is_file() and f.name!='SHA256.json'}
(ev/'SHA256.json').write_text(json.dumps(checks,indent=2)+'\n')
print('report written; evidence files',len(checks))
