"""Build the review report from committed measurements, without rescoring gates."""
import json
from pathlib import Path

P=Path(__file__).resolve().parent
ROOT=P.parents[2]
load=lambda n:json.loads((P/n).read_text())
sweep=load('sweep-summary.json');targets=load('target-summary.json');cpu=load('cpu-summary.json')
parity=load('parity.json');blocks=load('blocks.json');onset=load('onset-diagnostic.json')
rows=[]
for r in sweep['groups']:
    b=r['corr40-split-v3-sinc'];c=r['corr40-split-v3-sinc-edge']
    rows.append(f"| {r['rate']/1000:g} | {r['multiple']}fc | {b['worst_db']:.2f} | {c['worst_db']:.2f} | {c['over_minus50']} / {r['cases']} |")
cpu_rows=[]
for e,r in cpu['summary'].items():
    cpu_rows.append(f"| {e.removeprefix('corr40-split-')} | {r['relative_change_median']:+.2f}% | {r['relative_change_min']:+.2f}~{r['relative_change_max']:+.2f}% | {r['cpu_max_us_off']:.1f} → {r['cpu_max_us_on']:.1f} | {r['cpu_overruns_off']} → {r['cpu_overruns_on']} | {r['wall_overruns_off']} → {r['wall_overruns_on']} |")
low=[]
for name,v in targets['comparisons']['corr40-split-v3-sinc']['by_instrument'].items():
    low.append(f"| {name} | {v['max_abs_ms']:.6f} | {v['max_increase_ms']:+.6f} | {v['max_decrease_ms']:+.6f} |")
reg=sweep['regressions_over_1db']['corr40-split-v3-sinc']
worst=max(reg,key=lambda x:x['after']-x['before']);loudest=max(reg,key=lambda x:x['after'])
text='''# PR #234 독립 검증 — 고역 접합 경계 옵션

검증 기준일: 2026-10-31 KST. 상태: **연구 엔진의 기본값 OFF 옵션 / 제품 채택 보류 / 합성 신호만 검증**.

107Hz 부근과 2배·3배 구간의 경계 접합 결함은 독립 재현됐다. v3-sinc와 결합한 고역 전용 옵션은 새 시드의 모든 집중 스윕 조건에서 사전 잔여 기준을 만족하고, 기존 T1~T4 판정을 보존했다. 따라서 별도 검토 가능한 연구 옵션으로 유지할 가치가 있다. CPU 및 개별 시점·잔여 회귀를 함께 검토해야 하며, 실제 DI·청취·호스트 검증 전 제품 기본값으로 채택하는 근거는 아니다.

## 범위와 고정 근거

- 원 연구: [PR #234](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/234), head `3f28c3e09ec437a898868e5db0784acb30e33cdc`.
- 독립 기준 main: `472b93e68d20bcad07a028cc67a17b50f8171838`.
- 새 입력 렌더 전 원격 등록: `340c78cd508261e7c86cb5bcfa2f72ceda2b7251`. 등록 파일 SHA-256 `e5afc5bb3cc07c502cc6a010a6bbe414505498ed16330ebd5f99674e5602a193`.
- GCC 13.3.0, `-O2`, JUCE 8.0.8 `d6181bde38d858c283c3b7bf699ce6340c050b5d`, 공유 Linux 환경. 원본 probe를 패치 전에 빌드하고 같은 JUCE 객체·컴파일러로 후보를 빌드했다. 바이너리 및 소스 SHA는 등록 파일에 고정했다.
- 새 holdout 시드 `2026103101`, `2026103107`; 긴 음 시드 `2026103113`; 스윕 시드 `2026103129`. 44.1/48/96kHz, −2/−1/+1/+2반음.
- 변경은 `Tools/TransposeNext`의 연구 엔진·측정 도구에 한정된다. `Source/`, 제품 파라미터·프리셋·PDC·연구 플러그인 UI는 변경하지 않는다. `corr40-split-v3-edge`와 `corr40-split-v3-sinc-edge`를 명시적으로 선택할 때만 ON이다.

## 기존 근거 재검토

원 근거의 20개 SHA-256이 모두 일치했다. 저장된 fc/2fc/3fc 스윕을 재집계한 최악값·0.09샘플 주장은 PR 표와 일치한다. 기존 2fc/3fc 병렬 실행의 임시 파일 충돌 정정 이력도 확인했다. 이번 harness는 작업마다 독립 `TemporaryDirectory`를 사용한다.

각 구간·샘플레이트의 원래 최악 OFF 사례 총 9개를 시드 11의 정확한 난수열로 재생성했다. OFF/ON 잔여가 원 결과와 최대 `6.54e-13 dB` 차이로 재현됐다. 한 샘플씩 처리한 접합 추적과 128샘플 probe 출력은 18/18 비트 동일하다. 이 선택된 9개 사례의 최대 접합 오차는 1.651224 → 0.032870샘플이다. 새 스윕 전체의 접합 오차 상한을 재측정했다는 뜻은 아니다.

`integerCost()`의 경계 바로 밖 비용과 포물선 보정은 기존 ring buffer의 가드 안에서 평가된다. 추가 검색은 최대 3패스이며 새로운 메모리 할당·락·I/O는 DSP 경로에 넣지 않는다. 저역 `body`에 옵션을 켜는 폐기안은 적용하지 않았다.

## 새 시드의 고역 잔여

fc±1Hz, 2fc±2Hz, 3fc±3Hz 각각 51점 × 4반음 × 3표본률 = 1,836입력 조건, 네 엔진 총 7,344렌더. 무작위 고조파 위상, 1/h^0.8, 2초 합성음, 0.5~1.8초 측정. 새 스윕은 원 스윕과 달리 주파수를 0.001Hz로 반올림하지 않았다.

지표는 1~8kHz 고조파 사이 에너지에서 이상적 합성음의 측정 바닥을 뺀 값이며, 출력 고조파 에너지 대비 dB다. ±f0/4 고조파 주변은 제외한다. 따라서 전체 잡음·THD·청감 평가가 아니며, 바닥 차감 전 값과 고조파 gain도 원시 결과에 보존한다.

| kHz | 구간 | sinc OFF 최악 dB | sinc ON 최악 dB | ON −50dB 초과 |
|---:|---|---:|---:|---:|
'''+ '\n'.join(rows)+f'''

사전 기준 `sinc-edge 전 조건 ≤ −50dB`: **PASS**.

단, “모든 개별 사례가 개선됐다”는 주장은 성립하지 않는다. sinc 비교에서 {len(reg)}/1,836조건은 1dB 넘게 악화됐다. 가장 큰 증가는 {worst['after']-worst['before']:.2f}dB ({worst['before']:.2f} → {worst['after']:.2f}dB); 악화 사례 중 가장 큰 최종 잔여는 {loudest['before']:.2f} → {loudest['after']:.2f}dB다. 전부 −50dB 아래지만 청감상 무해하다고 판정하지 않는다. 개별 1dB 무회귀를 별도 필수 조건으로 삼으면 FAIL이며, 이번 등록에서는 이를 보고 항목으로 지정했다.

선형 보간 v3에도 결함 수정 효과는 있으나 44.1/48kHz에서는 ON 최악이 약 −38.49/−40.21dB에 머문다. 이 옵션은 보간 잔여까지 제거하지 않는다. 우선 검토 대상은 **v3-sinc + edge** 조합이다.

## T1~T4와 B0·A0 보호

네 엔진 각각 612조건(그중 새 holdout/긴 음 252조건), 총 2,448렌더. 전체와 새 입력+고정 steady 대조군 모두 같은 기존 spec v2로 평가했다. 임계값 변경 없음. 아래 수치는 sinc 비교이며 OFF/ON의 집계 최악값은 동일하다.

| 기준 | OFF → ON | 판정 |
|---|---|---|
| T1, SNR≥10dB 어택 최악 | 5.562 → 5.562ms | PASS, 한도 10ms |
| 희미한 어택 보고 | 412이벤트, 최악 5.146ms, 10ms 초과 0 | 보고 항목 |
| T2 다운시프트 저역, 짧은/긴 음 | 6.333/9.375 → 6.333/9.375ms | PASS, 한도 10ms |
| T3 업시프트 B0 긴 음 | 14.864 → 14.864ms | 15.5ms 회귀 한도 이내 |
| T3 업시프트 A0 긴 음 | 17.571 → 17.571ms | 19ms 회귀 한도 이내 |
| T3 업시프트 A1 긴 음 / 짧은 음 | 9.543 / 10.528ms, 동일 | 11/12ms 한도 이내 |
| T4 | warble 0.593cent, 오차 0.001cent; 누락·허위 reset·지속음 저역 reset 모두 0 | PASS |

T3는 10ms 달성 판정이 아니다. 제품의 고정 PDC를 검증·변경한 것도 아니다.

개별 저역 지연 차이(ON−OFF):

| 악기 | 최대 절댓값 ms | 최대 증가 ms | 최대 감소 ms |
|---|---:|---:|---:|
'''+ '\n'.join(low)+'''

모든 악기에서 사전 기준 절댓값 0.05ms 이내다. 고역 접합 시점이 저역 splice hold에 영향을 줄 수 있으므로 “저역을 수정하지 않았으니 차이가 없다”는 추론 대신 실제 렌더를 비교했다.

**onset의 중요한 차이:** 두 보간 방식 모두 612조건 중 13조건, 15이벤트의 고역 재설정 시점이 달라졌다. reset 수·저역 reset 수·짝 맞춤·누락·허위 검출은 같다. 재설정 지연 변화는 −3.53125~+1.473923ms이며, 실제 T1 어택 지표의 최대 증가는 v3 +0.340136ms, sinc +0.158730ms다.

`onset_samples`는 최초 novelty 검출 시각이 아니라 crossfade가 끝나 재설정이 실행된 시각을 기록한다. 변경된 13조건을 사후 추적한 26 OFF/ON 비교에서 최초 `detected` 시각과 `low_confirm` 시각은 모두 동일했다. 이는 해당 13조건의 추적 근거이며 모든 가능 입력의 검출 동일성 보증은 아니다. 개별 어택 시점의 소규모 악화는 청취 검증 대상으로 남긴다.

## OFF 동작·블록·안전성

- OFF vs 수정 전 main: **216/216 출력·결정 비트 동일**.
- 네 모드의 **432조건 × 14패턴 = 6,048비교 전부 비트 동일**. 기준 128, 고정 1/37/64/100/256/512/1024/2048/4096, 다섯 무작위 분할 패턴(새 시드 포함).
- ON 상태의 별도 C++ 계약 검사 **180그룹 PASS**: 8/44.1/48/96/192kHz, mono/stereo, ±12반음 포함, 무음·정확한 bypass/unity·reset·자동화·0샘플 처리·처리 중 C++ allocation 0. ASan/UBSan 적용 대상은 후보 header와 harness이며 재사용 JUCE 객체는 일반 빌드다.
- 첫 LeakSanitizer 실행은 `/proc/.../task` 접근 오류로 실패했다. 실패 로그를 보존했다. `detect_leaks=0` 재실행에서 ASan/UBSan 및 계약 검사는 통과했다. leak 검증 완료로 표기하지 않는다.

## CPU 비용

다른 검증 phase 종료 후 단독 CPU phase. 공유 Linux이므로 시스템 전체의 배타적 실행이나 실제 오디오 호스트를 보장하지 않는다. 세 표본률 × B0/A0 pluck 및 fc/2fc/3fc tone × block64/128/256 × −2/+1반음 × 5회 × 네 모드 = **1,800실행**. 매 반복 실행 순서를 반대로 번갈아 사용했다. edge tone의 중심은 +1반음 fc 기준이므로 모든 반음의 최악 접합 비용을 포괄하는 상한은 아니다.

thread CPU와 wall time을 분리한다. 아래 변화율은 조건별 5회 OFF/ON 쌍의 상대 변화 중앙값을 구하고, 그 90조건을 다시 요약한 값이다. CPU%는 컴퓨터 전체 점유율이 아니라 오디오 길이 대비 해당 처리 스레드의 CPU 시간이다. p99·최대·모든 deadline miss와 callback 식별자는 근거 파일에 있다. outlier 제거 없음.

| 기준 | 평균 CPU 상대 변화의 조건 중앙값 | 조건별 범위 | 최대 thread CPU callback µs OFF→ON | CPU deadline 초과 OFF→ON | wall deadline 초과 OFF→ON |
|---|---:|---:|---:|---:|---:|
'''+ '\n'.join(cpu_rows)+'''

**CPU 판정: 비용 차이의 신뢰할 만한 정량 추정 불가 / T5 실시간 채택 근거 부족.** 중앙값의 약 +0.93%/−0.83%를 '비용 없음' 또는 '속도 개선'으로 읽으면 안 된다. 조건별 변화 중앙값조차 약 −47~+89%로 흩어졌고, thread CPU 최대는 OFF 약 107~121ms, ON 약 173~176ms였다. 양쪽 모두 마감 초과가 있으며 ON의 최악값도 증가했다. 이 원인을 단순 OS 스케줄링 탓으로 확정하지 않는다.

기존 `TransposeProbe`에는 `ScopedNoDenormals`가 없고, 제품 `Source/PluginProcessor.cpp`에는 있다. 측정 전 동일하게 고정한 probe로 비교했지만 부동소수점 환경이 실제 호스트와 일치한다고 보장하지 못한다. 이 차이가 이번 spike의 원인인지는 입증하지 않았다. 이번 결과를 그대로 보존하고, 차후 전용 머신에서 호스트와 동일한 FTZ/DAZ·스레드 조건을 명시한 별도 CPU 측정이 필요하다.

평균 비용이 작아도 최악 callback과 deadline 초과를 지우지 않으며 Windows/macOS DAW·오디오 드라이버의 실시간 안전성으로 확대하지 않는다.

## 제품 채택 전 남은 항목

1. 실제 DI: A1 기타·B0 베이스·A0 베이스, 44.1/48/96kHz, 기존 9테이크 계획의 riff/dynamics/long/ghost/steady/vibrato. 이번 실행에는 실제 DI가 없다.
2. 레벨을 맞춘 청취: 107Hz 및 2fc/3fc를 지나는 bending/vibrato, A=432의 A2, 부드러운/강한 피킹, 새 결과의 잔여 악화 및 onset 재설정 지연 사례.
3. Windows/macOS 실제 호스트에서 block64부터 CPU peak·underrun, 자동화·bypass·project restore·고정 PDC를 별도로 검증.
4. 위 근거를 확보한 후 제품 통합·기본값 변경을 별도 판단. 이 PR은 main 병합·배포·청감 승인 요청이 아니다.

## 근거와 재현

근거 폴더: [`docs/evidence/transpose-edge-independent-20261031`](evidence/transpose-edge-independent-20261031/README.md). 원시 측정 JSON은 gzip으로 보존했다. CPU의 큰 callback 중간 파일은 재현 harness가 생성하며, 저장소에는 모든 run 요약·모든 overrun·원 callback gzip의 SHA-256을 보존한다. 원시 CSV 전체가 저장소에 있다고 주장하지 않는다.
'''
(ROOT/'docs/TRANSPOSE_EDGE_INDEPENDENT_20261031.ko.md').write_text(text)
