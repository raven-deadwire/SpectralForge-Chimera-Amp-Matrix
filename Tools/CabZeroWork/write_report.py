"""Render the source-bound Korean result from audited evidence (after collection)."""
import argparse,json,statistics,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--extra',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
e=a.evidence;d=json.loads((e/'analysis.json').read_text());audit=json.loads((e/'audit.json').read_text());s=json.loads((e/'summaries.json').read_text());prov=json.loads((e/'provenance.json').read_text())
assert audit['audit_pass'] and audit['callbacks']==2304000
# This report is a negative-result record, not a generic promotion controller.
assert not any(x['cpu_improvement_demonstrated'] for x in d['decisions'])
focus=lambda phase:next(x for x in d['decisions'] if x['candidate']=='zero_history' and x['route']=='1-32-2' and x['phase']==phase)
zf,zc=focus('fade_active'),focus('cold')
failures={c:sum(not x['deadline_nonincrease'] for x in d['decisions'] if x['candidate']==c) for c in ['empty_partition','zero_history']}
out=a.output;out.mkdir(parents=True,exist_ok=True)
for name in ['analysis.json','audit.json','build.json','gates.json','provenance.json']:
    shutil.copy2(e/name,out/name)
for name in ['checks.json','checks-no-leaks.json']:
    shutil.copy2(a.extra/name,out/name)
labels={'cold':'reset 직후 400 callbacks','steady_before':'전환 전 안정 구간','publication':'교체 후 800 callbacks','fade_active':'fade 실제 활성 구간','steady_after':'전환 후 안정 구간'}
report=f'''# CAB 계산 비용 후보 검증 — 2026-10-20 KST

## 결론과 적용 범위

**이번 측정에서 제품 적용 후보로 승격할 변경은 없다.** 두 후보 모두 12 routes × 5 phase의
CPU screening을 통과한 항목이 0개다. zero_history는 소스상 영 기여 MAC을 줄이지만,
측정 변동을 넘는 CPU 개선은 입증하지 못했다. 따라서 성능 검증 완료나 실패 해결로 표시하지 않는다.

기준은 PR #105 `50ed1fd50ffb735d8f6581197116ac4184187d91`이다. 측정 controller는
`{prov['source_sha']}`이며, 결과 보고/독립 감사 파일의 후속 commit과 구분한다.
제품 `Source/`, 기존 tests, AAX, 버전 및 공개 승인 정책은 수정하지 않았다.
생성한 후보 overlay의 실제 헤더·바이너리 SHA256과 compiler 명령은 `build.json`에 있다.

Linux x86_64 / AMD EPYC 9V74 KVM / Ubuntu 24.04 / GCC 13.3 / JUCE 8.0.8
`d6181bde…` Release 수동 CAB-only 빌드다. Apple vDSP/macOS·Windows·전체
PluginProcessor·DAW·오디오 인터페이스·Pro Tools·청음은 이번 실험에서 실행하지 않았다.
공식 CMake/CTest가 아닌 기존 test entry의 직접 실행이며, 다른 플랫폼의 PASS로 승계하지 않는다.

## 후보별 판정

| 후보 | 소스에서 확인한 효과 범위 | 판정 |
|---|---|---|
| 빈 마지막 partition 생략 | 실제 IR 8,161샘플에서는 생략할 partition이 없음 | 현행 모델의 계산 감소안으로 제외 |
| reset 후 미기록 입력 이력 MAC 생략 | 새 kernel/reset 직후 최대85ms. FFT·IR·tail·50ms fade 그대로 | 출력 동등성 검증 / 성능 개선 미입증 / 제품 승격 보류 |
| Apple complex forward의 unity scale 호출 생략 | JUCE가 vDSP FFT 후 1.0배 pass 호출 | macOS 미실행·미검증. 제품 적용 후보로 승격하지 않음 |

8,160샘플이라는 초기 가설은 `ceil(96000*.085)=8161`을 실제 확인하여 정정했다.
8,160으로 줄이거나 마지막 유효 샘플을 제거하지 않는다. 기존 prefetch, NEON/S8T,
scalar memory/tiling, forward FFT reuse 후보를 반복하거나 결합하지 않았다.

## 집중 경로: expanded-v2 / 96kHz / 32 samples / stereo

예산 **333.333333µs** 유지. 아래 CPU 값은 각 trial의 phase p50을 먼저 구한 뒤
A/B 12회씩의 중앙값이다. 개선율은 각 ABBA/BAAB quartet의 A/B 평균 p50을 짝지은
6개 개선율의 중앙값이다. 집계 방식이 다르므로 표의 A/B 중앙값으로 계산한 단순 비율과 일치하지 않는다.
miss는 같은 phase의 모든 callback을 합한 횟수이며 CPU 동반과 wall-only는 raw 자료에 분리했다.

| 후보/구간 | A CPU µs | B CPU µs | paired CPU 개선 | A/A 최대 변동 | wall miss A→B | CPU 초과 A→B | local screening |
|---|---:|---:|---:|---:|---:|---:|---|
'''
trials=json.loads((e/'trials.json').read_text())
for candidate in ['empty_partition','zero_history']:
    for phase in ['cold','steady_before','publication','fade_active','steady_after']:
        r=next(x for x in d['decisions'] if x['candidate']==candidate and x['route']=='1-32-2' and x['phase']==phase)
        means={}
        for arm in 'AB':
            names={t['name'] for t in trials if t['candidate']==candidate and t['kind']=='AB' and t['arm']==arm}
            means[arm]=statistics.median(v['cpu_p50'] for v in s if v['trial'] in names and v['route']=='1-32-2' and v['phase']==phase)
        counts=r['deadline_counts'];name='빈 partition' if candidate=='empty_partition' else '영 이력'
        report+=f"| {name} / {labels[phase]} | {means['A']:.3f} | {means['B']:.3f} | {r['median_cpu_improvement_pct']:+.2f}% | {r['aa_envelope_pct']:.2f}% | {counts['A']['wall_miss']}→{counts['B']['wall_miss']} | {counts['A']['cpu_over']}→{counts['B']['cpu_over']} | {'PASS' if r['local_screen_pass'] else '보류'} |\n"
report+=f'''
`fade_active`는 publication 구간 안의 하위 집합이다. 두 구간을 더해 총 표본 수로 쓰지 않는다.
주 후보의 집중 경로 fade CPU p50 개선 중앙값은 {zf['median_cpu_improvement_pct']:.3f}%지만 A/A 최대 변동 {zf['aa_envelope_pct']:.3f}%보다 작고,
6개 quartet 중 최저값은 {min(zf['paired_effects_pct']):.3f}%였다. cold 개선 중앙값 {zc['median_cpu_improvement_pct']:.3f}%도 A/A 최대 변동 {zc['aa_envelope_pct']:.3f}%에 못 미쳤다.
일부 유리한 수치나 전체 miss 감소만으로 채택하지 않는다. 후보 전체에서 route/phase별
deadline 비증가 실패는 empty_partition {failures['empty_partition']}/60, zero_history {failures['zero_history']}/60이었다. 겹치는 phase를 합산하지 않는다.
관측된 CPU 변화는 thread CPU accounting과 동일한 clock envelope를 포함한다.
wall-CPU 차이나 독립 p99의 차이를 OS scheduler 시간으로 계산하지 않는다.

## 전체 경로와 안전성

'''
for candidate in ['empty_partition','zero_history']:
    rs=[r for r in d['decisions'] if r['candidate']==candidate]
    report+=f"- `{candidate}`: 12 routes × 5 phase 중 CPU screening 통과 {sum(r['cpu_improvement_demonstrated'] for r in rs)}/60, deadline 비증가까지 통과 {sum(r['local_screen_pass'] for r in rs)}/60. phase는 겹치므로 독립 제품 경로 60개로 해석하지 않는다.\n"
report+='''
- 96 trials / **2,304,000 callbacks**, 5,760개 route-phase summary를 독립 감사로 재계산했다.
- 모든 trial의 전체 float32 출력 SHA256이 canonical과 일치. 모든 callback의 IR/slot/fade/output identity도 일치했다.
- baseline 및 두 후보 각각 기존 integration 네 entry **66 timing routes**, realtime **144 routes** PASS. 새 경계 parity **196 cases / 1,000,818 samples** bit exact.
- 각 구현의 frozen **2,645,088 floats**, fade **388,800 floats** bit exact. immutable spectra, zero added latency, partial/zero block, reset/reprepare, stereo isolation, six-mic direct FIR 회귀 PASS.
- callback new/delete 관측 0. 이는 thread-local C++ 연산자 검사이며 모든 malloc/lock을 감시하는 범용 trace가 아니다. 후보의 제품 변경 자체는 추가 allocation/destruction/lock/I/O가 없는 정수 bound/counter 변경이다.
- 개인 IR 파일이 없는 상태의 embedded WAV bytes·bass metadata·오디오 복원 PASS. modeled worker cancellation, latest six slots, clear/reprepare PASS.
- candidate header와 harness translation unit의 ASan/UBSan PASS. JUCE dependency objects는 Release이며 전체 JUCE 계측을 주장하지 않는다.
- 최초 LeakSanitizer는 이 환경의 `/proc/.../task` 접근 불가로 fatal 종료했다. 원본 네 실패 로그를 보존했다. 별도 `detect_leaks=0` 실행은 주소/UB 검사만 통과했으며 **누수 검사 미검증**이다.

## 승인 및 남은 검증

CPU screening은 사전 정의한 6개 quartet 모두 개선 방향, 중앙값 >5% 및 A/A noise envelope 초과 조건이다.
제품 후보 승격에는 이득이 있는 phase뿐 아니라 **모든 route의 안정 구간 회귀와 deadline 비증가**도 검토해야 한다.
Linux 일부 phase의 개선을 96k/32 macOS 실패 해결, 순수 DSP 비용 측정, OS 원인 확정,
물리 인터페이스 dropout 해소로 확대하지 않는다. 기존 PR #101/#105의 실패 기록은 유지한다.

`integration_eligible=false`, `release_approved=false`, `aax_approved=false`.
기존 AAX 개발 빌드 성공 및 과거 공개 승인을 이 연구에 승계하지 않는다.
실제 Mac에서는 동일 IR·state·fade의 exact-source A/B와 원래 333.333µs gate,
실제 DAW/interface, 별도 scheduler runnable/running trace가 필요하다.

재현: [Tools/CabZeroWork/README_KO.md](../../../Tools/CabZeroWork/README_KO.md).
원시 자료에는 실패·지연 초과·측정 duration과 해당 로그·hash를 관측값 그대로 보존한다.
이 probe는 clock endpoints 자체 대신 원래 endpoint 차이 duration을 기록하며,
원래 integration CSV에는 원시 CPU endpoints도 포함한다. 어느 쪽도 scheduler trace가 아니다.
'''
(out/'RESULTS_KO.md').write_text(report)
print(out/'RESULTS_KO.md')
