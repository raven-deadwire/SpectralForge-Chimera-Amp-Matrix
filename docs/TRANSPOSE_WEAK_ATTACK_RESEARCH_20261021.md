# Transpose 약한 어택 독립 연구 — 2026-10-21 KST

**결론: 기본 합성 약한 어택의 고역 반응은 개선했으나 전체 추가 지연 ≤10ms 목표는 FAIL이다. 제품 DSP 교체·main 병합·공개 배포를 승인하지 않는다.**

PR #100 `6ec78c475311827ccfd1949c9ae9ce364f79c0b4`에서 분기했다. 구현 커밋은 `979c13d65e1cf42fb21ecbca6b8c39b434da0111`이다. 로컬 측정 소스 커밋 `b0d9f29`과 Git tree `cae306afa1d77eb431f11d7550153e6b7bbe0ff5`가 동일하다. 게시 방식에 따른 commit ID 차이이며 소스 차이는 없다. 바이너리·파일별 SHA-256은 `provenance.json`에 기록했다.

## 원인과 독립 후보

- 기존 `corr40-onset`: 800Hz 고역 에너지의 0.3ms fast envelope와 1ms 지연된 60ms 지수 피크를 비교한다. 임계값 2.5배·45ms refractory·fade 중 요청 거부 때문에 강한 피킹 뒤 약한 피킹을 놓친다. 상관 접합은 800Hz 저역 파형의 12ms 구간을 정규화 제곱 오차로 비교하고, 2~40ms read age 안에서 coarse/fine 검색 후 2ms crossfade한다. 10ms history로 축소하면 저음 한 주기를 충분히 찾지 못하는 기존 실패를 그대로 존중했다.
- `corr40-novelty`: 2.5kHz 고역 필터 4단, 전체 에너지에 대한 고역 비율, 1ms 지연 및 41개 1ms 버킷의 유한 최댓값, 4배 threshold를 사용한다. 오래된 강한 피킹은 기준에서 만료되고 느린 전체 음량 변화는 정규화로 억제한다. 원래 45ms refractory를 유지하며 fade 중 요청은 끝날 때까지 보류한다. 검출 뒤 추가 fade 대기는 최대 2ms지만 **검출 성공·전체 음향 지연의 10ms 보장은 아니다.** 절대 energy floor가 있어 모든 입력 레벨에서 동일 동작한다고 주장하지 않는다.
- `corr40-split`: 원래 입력으로 검출하되 350Hz Linkwitz–Riley 4차 분리의 고역 재생 위치만 재설정한다. 저역은 onset reset 없는 기존 상관 경로로 변환한다. 두 대역 모두 실제로 pitch shift하며 dry tonal attack을 섞지 않는다. 상관 cost/search/fade 알고리즘은 변경하지 않았다. 저역 지속음 위상 점프는 줄지만 두 대역의 지연이 달라 크로스오버 위상·착색과 저역 시간 정렬은 미해결이다.

## 동일 조건 비교
48kHz·128 samples·−2반음, 동일 float32 stereo 합성 입력. 아래 값은 기존 PR100의 1–8kHz/2ms RMS 어택 peak에서 입력 자체 bias를 뺀 추가 지연이다. 고정 알고리즘 지연, 저역 전체 반응, 호스트 PDC 또는 왕복 지연과 다르다.

| 입력 | 조건 | 공개 baseline | corr40-onset | corr40-split |
|---|---|---:|---:|---:|
| bass_B0 | 강약 교대 75ms | 45.438ms | 12.104ms | 2.750ms |
| bass_B0 | 지속음 + 작은 어택 | 47.708ms | 34.604ms | 2.750ms |
| bass_dropA_A0 | 강약 교대 75ms | 47.542ms | 12.104ms | 2.750ms |
| bass_dropA_A0 | 지속음 + 작은 어택 | 51.708ms | 39.354ms | 2.688ms |
| guitar_dropA_A1 | 강약 교대 75ms | 44.729ms | 12.104ms | 2.938ms |
| guitar_dropA_A1 | 지속음 + 작은 어택 | 49.583ms | 39.375ms | 2.667ms |

B0 강약 교대·작은 어택 입력 SHA-256은 10월 18일 `stress.json`과 일치한다. B0 8배음의 공개 baseline 기음 오차 약 +67.55 cents도 재현했다. 기존 scorer를 수정하여 결과를 좋게 만들지 않았다.

| B0 −2 지속음+작은 어택 | 기음 오차 | 목표 기음 gain | local 기음 gain | 워블 proxy p95–p5 |
|---|---:|---:|---:|---:|
| baseline | 67.5508 cents | -47.5165 dB | -5.2528 dB | 107.962 cents |
| corr40-onset | 0.0002 cents | -0.0001 dB | -0.0001 dB | 0.297 cents |
| corr40-novelty | 2.4074 cents | -3.2898 dB | -0.2002 dB | 224.700 cents |
| corr40-split | -0.0000 cents | -0.0005 dB | -0.0007 dB | 0.304 cents |

검출만 바꾼 whole-waveform 후보는 작은 어택마다 지속음 위상을 재설정해 워블을 만들었다. 이를 최종 채택 후보로 승격하지 않았다. 12ms peak decay를 사용한 최초 후보도 지속음에서 약 +200 cents 오류가 발생하여 `rejected-decay/`에 실패 소스·원결과를 보존했다.

## 범위 확장과 검출 한계

- B0 30.867706Hz, 5현 베이스 drop A0 27.5Hz, 7현 기타 drop A1 55Hz × 44.1/48/96kHz × 64/128/256 samples × −2/−1/+1/+2반음. steady/강약 교대/작은 어택과 공개/이전/분리 후보를 972회 렌더했다. 블록 크기를 비교하는 **324/324 그룹 output SHA 일치**. 이상적인 단음 scorer controls도 통과했다.
- 총 품질·진단 렌더 1119회, 별도 timing 45회. 재튜닝에 사용한 development 합성 입력과 최종 선택 뒤 seed 8139의 별도 합성 holdout을 구분한다. holdout은 무작위 간격·세기·위상·감쇠의 96개 이벤트이며 고역 추가 peak 지연 최대 4.771ms였다. 다만 검출기 자체는 10ms 이내 95/96 이벤트만 연결했다. 출력 peak가 빠르다는 이유로 누락된 검출을 PASS로 바꾸지 않는다.
- 일정/밝은 64배음/트레몰로/비브라토/노이즈 바닥 지속음의 시작 10ms를 제외한 false reset은 새 후보 0회. 별도 guard suite의 이전 onset은 60회(트레몰로)였다. 이 조건 밖 모든 실제 악기의 오검출률을 보증하지 않는다.
- 더 작은 0.001 amplitude marker의 A1 조건은 **6/16 검출 누락, 최대 30.188ms**. 기본 0.004 marker의 성공으로 이 실패를 덮지 않는다.
- 50ms 피킹 검출은 3음역에서 16/16이지만 기존 scorer의 70ms 검색창이 다음 이벤트와 겹치므로 해당 peak 지연은 `attack_metric_valid=false`인 진단값이다. 이를 지연 통과 근거로 사용하지 않는다.
- 트레몰로·비브라토에는 의도한 변조가 있으므로 stationary warble threshold를 그대로 적용하지 않는다. 실제 악기 저역 안정성이나 polyphonic 코드 분리는 이번 합성 단음 검사로 인증하지 않았다.

| shift | 기본 입력 어택 최대 | 기음 오차 절댓값 최대 | 워블 proxy 최대 |
|---|---:|---:|---:|
| -2 | 2.938ms | 0.0076 cents | 0.595 cents |
| -1 | 2.521ms | 0.0028 cents | 0.420 cents |
| 1 | 37.687ms | 0.0009 cents | 0.210 cents |
| 2 | 33.750ms | 0.0007 cents | 0.155 cents |

## 전체 10ms 판정이 여전히 실패인 이유

고역에 에너지가 많은 impulse의 95% 시점은 새 후보에서 2.771ms지만, 이것만 보고 저음 전체 지연이 10ms 이하라고 판단하면 안 된다. 같은 입출력에 causal 4차 120Hz lowpass를 적용하고 기존 impulse energy scorer로 별도 측정했다. 입력의 band-limited centroid bias를 뺀 저역 추가 에너지 중심 지연은 **10.686~16.877ms**다. 절대 centroid와 energy95·각 이벤트 원자료도 `low-band.json`에 보존했다. 이 검사는 대역 제한 합성 진단이며 저음 연주 청감 또는 fixed latency 검사가 아니다.

상향 변환은 onset reanchor 대상이 아니며 기본 입력에서도 최대 37.687ms가 남는다. `latency()=-1`을 유지했고 2ms·10ms로 host 보고값을 바꾸지 않았다. 고정 PDC, 양 대역 정렬, bypass/automation/recall의 실제 호스트 검증과 DI/Neural DSP 비교가 완료되기 전에는 제품 통합할 수 없다.

## CPU·회귀 검사
공유 Linux/KVM AMD EPYC 9V74 노출 환경, GCC 13 계열 O2, JUCE 8.0.8 pinned SHA. 30초 B0 강약 교대+작은 어택 혼합을 3엔진·3블록·5회 순환 순서로 측정했다. 품질·빌드·sanitizer 작업 종료 뒤 별도로 실행했으나 전용 실물 오디오 장비나 드라이버 callback 측정은 아니다. 현재 probe의 공통 진단 계측을 포함한 비교이며 이전 머신의 246.69µs 등과 직접 비율 비교하지 않는다.

| block | engine | 5회 평균 CPU의 중앙값 | p99 CPU 중앙값 | CPU peak | CPU / wall deadline miss |
|---|---|---:|---:|---:|---:|
| 64 | baseline | 69.878µs | 401.600µs | 3966.290µs | 40 / 40 |
| 64 | corr40-onset | 2.436µs | 2.534µs | 1160.530µs | 0 / 0 |
| 64 | corr40-split | 6.992µs | 49.084µs | 1347.620µs | 1 / 1 |
| 128 | baseline | 138.670µs | 444.831µs | 5159.530µs | 6 / 7 |
| 128 | corr40-onset | 4.432µs | 18.698µs | 618.830µs | 0 / 0 |
| 128 | corr40-split | 13.577µs | 289.946µs | 851.436µs | 0 / 0 |
| 256 | baseline | 279.794µs | 531.369µs | 3366.190µs | 0 / 0 |
| 256 | corr40-onset | 8.876µs | 285.059µs | 861.360µs | 0 / 0 |
| 256 | corr40-split | 30.758µs | 340.972µs | 1455.400µs | 0 / 0 |

128 samples 평균 CPU는 이전 onset 4.432µs → 분리 후보 13.577µs로 **약 3.06배 증가**했다. 공개 baseline 138.670µs보다 작지만 낮은 평균만으로 실시간 PASS를 선언하지 않는다. 새 후보도 64 samples에서 CPU와 wall이 함께 예산을 넘긴 **1회**가 있다(peak 1347.620µs, budget 1333.333µs).

연구 후보의 270개 rate/bound/layout/shift 그룹에서 partition 일치, 정확한 bypass/unity, silence, mono/stereo polarity, reset, automation finite, process 중 C++ new/new[] 0을 검사했다. ASan/UBSan도 candidate translation unit 범위에서 통과했다. JUCE 링크 객체는 일반 O2이며 leak 검사와 malloc 계열 전체 계측으로 확대하지 않는다. 기존 공개 코어 PitchRegression/Stability/Gate 실행 로그도 보존했다. 제품 통합 CI·Windows/macOS·실제 DAW 결과로 대신하지 않는다.

## 산출물·재현·다음 판정

- `Tools/TransposeNext/README.md`의 build/각 suite/holdout 명령을 사용한다. 저역 보완 검사는 `low_band_delay.py --renderer <probe> --out <dir>`이다.
- [summary.json](evidence/transpose-weak-attack-20261021/summary.json), detail/guards/holdout/low-band/timing JSON, 압축 matrix JSON, callback 원자료 archive, B0 합성 비교 WAV archive, 소스·바이너리 SHA-256, 테스트 로그를 보존했다. 파일 무결성 목록은 `SHA256.json`이다.
- 실제 DI 및 Neural DSP 원본 출력은 이번 작업에서 확보하지 못했다. WAV archive는 전부 합성 입력/출력이며 실제 연주 비교 또는 Neural DSP 동등성 증거가 아니다.
- Source/제품 Tests/Assets/root CMake/Installer/버전/파라미터/정책/workflow/PDC는 PR #100 기준 그대로다. 새 후보는 Tools의 독립 실행 파일에만 있다.
- 후속 우선순위: 0.001 A1·고역 변화가 거의 없는 어택의 검출 누락, 저역 시간 정렬·크로스오버 착색, 상향 변환, 고정 latency/PDC 계약, 전용 하드웨어 callback tails, 동일 DI/Neural DSP 및 Windows/macOS 실제 DAW 수용. **Issue #97 P0 완료·10월 23일 공개 승인으로 처리하지 않는다.**
