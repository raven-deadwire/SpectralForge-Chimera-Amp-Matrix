#!/usr/bin/env python3
"""Write the limited Korean research verdict directly from measured JSON."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('results',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
d=json.loads(a.results.read_text());rows=d['rows']
def row(fixture,shift,engine):return next(r for r in rows if r['fixture']==fixture and r['shift']==shift and r['engine']==engine)
def fmt(x):return '미검출/무효' if x is None else f'{x:.3f}'
lines=['# Transpose 예측 합성 연구 — 2026-10-18 KST','',
'**판정: 연구 후보 유지. 제품 반영·배포 제외. 10ms 이하 지연과 저역 품질의 동시 달성은 미입증.**','',
'## 범위와 근거','',
'- 통합 요구: [#97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97), Transpose [#8](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/8).','- 공개 기준: v1.3.2-beta.1 제품 소스 `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`. 작업 기준 main `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`; `PolyPitch.h`와 `GlobalDSP.h`가 공개 기준과 동일함을 git diff로 확인했다.','- 원본 Neural DSP 측정·구현 결과는 이번 검색에서 특정하지 못했다. 사용자 소유 GitHub의 관련 검색/PR #45, 과거 기록, 저장된 비교 패키지 두 개, Google Drive의 Transpose/Azhi 검색을 확인했다. 검색 범위 밖에 자료가 없다는 단정은 하지 않는다.','- `03_Azhi Dahaka.flac`라는 전체 믹스 자료의 메타데이터는 찾았으나, 이를 베이스 단독 DI로 사용하지 않았다. 실제 isolated DI·Neural 처리 WAV·원본 플러그인 실행 환경은 확보되지 않았다.','- 이 PR은 새 독자 알고리즘 실험이다. Neural DSP 원본 역분석 완료나 동등성을 주장하지 않는다. 독점 코드를 복제하지 않았다.','',
'## 기존 연구와 비교','',
'| 자료 | 확인된 내용 | 상태 |','|---|---|---|',
'| PR #45, `ff9ca84c1cb53eaf7a054965ec4eee5e084aa60f` | 자체 엔진·상대 위상·Signalsmith·8ms 가변 딜레이 비교. 상대 위상 후보도 42.667ms, 짧은 딜레이는 B0 음정 실패 | Neural 원본 측정 아님 |',
'| 10월 4일 P0 설계 패키지 | 창 축소 시 저역 손실과 전환/지연 검증 설계 | 제품 변경 아님 |',
'| 10월 13일 비교 패키지 | 비대칭 4096/384 후보의 −2반음 impulse centroid 8.688ms, 흔들림 94.861 cents; 다른 반음·코드·DI 미통과 | 역사적 자체 실험. 이번 신규 수치와 구분 |',
'| 이번 예측 spectral 후보 | 과거 Hann 분석의 부분음을 추적하고 현재 시점 oscillator 합성. N=1024/2048/4096, H=128, 최대 64 tracks | 연구 실행 파일 전용 |','',
'기존 ZIP SHA-256:','',
'- `Chimera_Transpose_P0_Design_Evidence.zip`: `207c25d509f36f2506c67035cb6c92655c17d68f13c0f2d795671e0306a888b2`','- `Chimera_Transpose_Comparison_2026-10-13.zip`: `96e609f22feba0b54a43f80d97483628db5bcec0558c9ed655a85ebf00c27a3e`','',
'## 이번 실측','',
f'48kHz·128 samples·−2/−1/+1/+2반음, {len(rows)}회 native render. 입력은 모두 결정적 **합성 신호**다. 실제 Azhi Dahaka 연주에 대한 결과가 아니다. Gate/앰프/CAB을 제외한 피치 코어 비교이며, 입력 노트 정보는 측정기에만 있고 엔진에는 전달하지 않았다.','',
'배음 8개를 포함한 합성 B0 → A0(−2반음), 정상상태 2초:','',
'| 엔진 | 중심 음정 오차 cents | 정확한 목표 기음 gain dB | 주변 기음 대역 gain dB | 흔들림 폭 cents |','|---|---:|---:|---:|---:|']
for en in d['summary']:
 z=row('harmonics',-2,en)['tone'];lines.append(f"| {en} | {fmt(z['dominant_error_cents'])} | {fmt(z['target_gain_db'])} | {fmt(z['local_fundamental_band_gain_db'])} | {fmt(z['warble_p95_p5_cents'])} |")
lines+=['','출력 FIFO를 없애도 과거 분석으로 인한 진폭 응답 지연은 남는다. 예측 후보의 고정 PDC는 정의하지 않았으며 `-1`로 표시했다. 이를 0ms 또는 10ms 이내 지연으로 해석하면 안 된다.','',
'−2반음 impulse 8회 및 75ms 반복 피킹(기존 fixture의 고유 hop 위상은 baseline 4개, 후보 2개):','',
'| 엔진 | impulse 중심 중앙값 ms | impulse 폭 중앙값 ms | impulse 출력/입력 에너지 dB | 어택 peak 지연 중앙값 ms | 어택 peak gain 중앙값 dB |','|---|---:|---:|---:|---:|---:|']
for en in d['summary']:
 q=row('impulses',-2,en);z=q['impulse'];r=row('riff75',-2,en)['attack'];lines.append(f"| {en} | {fmt(z.get('centroid_ms_median'))} | {fmt(z.get('smear_ms_median'))} | {fmt(q['impulse_output_input_energy_db'])} | {fmt(r['peak_offset_median_ms'])} | {fmt(r['peak_gain_median_db'])} |")
lines+=['','어택은 입력 대비 1–8kHz/2ms RMS envelope peak 차이다. 20dB 넘게 소실된 어택의 지연은 무효로 처리한다. 임펄스·어택·정상상태 음정은 다른 측정이며, 작은 잔여 출력만으로 저지연 통과를 주장하지 않는다. 수치는 청감 명료도 점수가 아니다.','',
'## 판정과 평가기 제한','',
'제안 연구 선별값은 음정 ±10 cents, 흔들림 ≤20 cents, 기음 gain ±1dB, impulse 중심 최대 10ms와 폭 최대 2ms다. 기존 제품 릴리스 정책을 변경하는 값이 아니다.','',
'| 엔진 | tone 선별 통과 | latency 선별 통과 | 제품 반영 |','|---|---:|---:|---|']
for en,s in d['summary'].items():lines.append(f"| {en} | {s['tone_pass']}/{s['tone_total']} | {s['latency_pass']}/{s['latency_total']} | {'현행 비교 기준' if en=='baseline' else '제외'} |")
invalid=[c for c in d['controls'] if not c['pass']]
lines += ['',f"이상적인 정답 신호에 대한 평가기 제어 {len(d['controls'])}개 중 {len(d['controls'])-len(invalid)}개 통과, {len(invalid)}개 실패. 실패한 조건의 tone 선별 PASS는 허용하지 않았다.",'']
for c in invalid:lines.append(f"- {c['fixture']} {c['shift']:+}반음: 이상적 입력에서도 warble {c['metrics']['warble_p95_p5_cents']:.3f} cents. 해당 평가군은 무효다.")
lines += ['','가까운 두 음은 단일-tone warble 대신 각각의 목표 성분 투영으로 기록했다. `out_of_target_power_fraction`은 목표 부분음 ±2Hz 밖 에너지의 비율로, 음정 이탈·변조·잡음이 함께 들어가는 진단값이다. 실제 악기 음색/명료도와 등치하지 않는다.','',
'새 후보는 residual/noise 모델이 없어 피킹과 비정상 신호 손실 위험이 크다. 긴 창의 기음 보존 장점을 짧은 어택 경로와 결합하는 후속 설계가 필요하지만, 원음 어택을 그냥 섞어 이중 음정을 만들지 않아야 한다. 현재 결과로 배포하지 않는다.','',
'## 빌드와 회귀','',
'- Linux GCC 13.3 / JUCE 8.0.8 pin에서 native probe와 신규 연구 테스트, 기존 생산 회귀 실행 파일을 새로 빌드했다. `build-manifest.json`에 명령·소스 hash·실행 파일 hash를 기록했다.','- `PredictiveTests`: 90개 rate/window/channel/shift 경로에서 finite/unity, 63/128/257 block partition, reset/reprepare, 역위상 스테레오 및 process 내 C++ new 관측 검사 통과. 별도 transition/crosstalk 검사 통과. 음질·실제 실시간 콜백·호스트 수용 검사와 구분한다.','- 기존 `PitchRegressionTests`, `StabilityTests`, `GateTests` 3/3 통과. 기존 검사나 assertion을 완화하지 않았다.','- 초기 개발 빌드와 새 의존성 빌드의 B0 −2 출력은 네 엔진 모두 bit-identical. 최종 144회 측정은 새 빌드로 실행했다.','- callback wall time은 공유 환경의 진단값만 기록했다. 다른 작업 부하가 있었으며 controlled realtime PASS가 아니다.','- Windows/macOS/VST3/Standalone/실제 DAW/PDC/loopback 검사는 이번 로컬 실행에 포함되지 않았다. 새 GitHub CI는 별도 실행 상태로 확인해야 하며 이 로컬 로그로 대신하지 않는다.','',
'## 다음 실행에 필요한 입력','',
'Azhi Dahaka의 **48kHz 베이스 단독 pre-effects DI**와, 가능하면 동일 DI의 Neural DSP bypass/0/−2/−1/+1/+2 출력 및 정확한 제품·버전·설정이 필요하다. `Tools/TransposePredictive/render_wav.py`로 같은 입력을 곧바로 렌더할 수 있다. 실제 DI·청감·Neural 외부 측정은 모두 미완료로 유지한다.','',
'제품 Source, 파라미터/상태/프리셋, 배포 버전·업데이트 정보는 변경하지 않았다. 이 문서는 연구 근거이며 공개 패키지 포함 승인이나 역분석 완료 선언이 아니다.','']
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(lines))
