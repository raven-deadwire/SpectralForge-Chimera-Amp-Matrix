# Transpose T6 청취·DAW 수용 준비 — 2026-10-24 KST

T6(외부 수용) 중 **블라인드 청취**와 **DAW 수용**을 준비한 문서다. 실제 DI 측정은 `TRANSPOSE_T6_DI_PROTOCOL.md`가 다룬다. 녹음·청취자·DAW 시험은 아직 없다. 이 문서는 그 전에 절차와 판정 규칙을 고정한다.

비교 대상은 두 가지다.

- **product:** 제품의 현재 Transpose 엔진(`Source/PolyPitch.h`, 바꾸지 않음)
- **candidate:** 연구 후보 `corr40-split-v2`

고정 PDC 계약과 제품 엔진 통합은 이 문서 범위가 아니다. 여기 결과로 제품 DSP를 바꾸지 않는다.

## 1. 블라인드 청취

도구는 `Tools/TransposeNext/t6_listening.py`다. 입력은 실제 DI 검증과 같은 녹음 목록이며, 라벨은 필요 없다.

```sh
python Tools/TransposeNext/t6_listening.py prepare --manifest ~/t6/takes.json --renderer build/transpose-next/TransposeProbe --out ~/t6/listening --seed <정수>
# 청취자마다 listener/ 폴더 사본을 주고, 채운 두 CSV를 돌려받는다
python Tools/TransposeNext/t6_listening.py score --key ~/t6/listening/blind_key.json --listener ~/t6/answers/A --listener ~/t6/answers/B --listener ~/t6/answers/C --out ~/t6/listening-result.json
```

### 시험 세트

**클립.** 테이크마다 `riff`·`dynamics`·`long` 구간의 앞 10초를 쓴다. 조건은 −2반음과 +2반음이고, product와 candidate를 각각 렌더한다.

**맞춤 처리:**

- **음량:** 두 조건을 모두 −23 LUFS(BS.1770)로 맞춘다. 피크가 −1dBFS를 넘으면 두 조건을 같은 양만큼 줄인다.
- **지연:** product가 보고하는 고정 지연(FFT 창)은 앞에서 잘라낸다. candidate의 가변 지연(2–15ms)은 그대로 둔다.
- 오프라인 클립은 음질만 판단한다. 지연 체감은 DAW 실시간 시험(§2의 D06)에서 본다.

**시험 구성:**

| 시험 | 구성 | 목적 |
|---|---|---|
| ABX | `riff` 구간, 테이크 × 반음마다 12회. 테이크 세 개면 72회, 순서는 섞음 | 두 엔진을 들어서 구별할 수 있는지(보고용) |
| 쌍 비교 | 세 구간 × 테이크 × 반음, 각 2회. 테이크 세 개면 36회. P/Q 배정은 무작위 | 선호도와 속성 점수(판정용) |

**쌍 비교 기록 항목:**

- 선호(P/Q/none)
- 클립별 0–5 점수 세 가지
  - `attack`: 어택 선명도, 높을수록 좋음
  - `low_end`: 저역 몸통, 높을수록 좋음
  - `artifacts`: 워블·이중 어택·클릭, 0 = 없음, 5 = 심함
- 메모: 들린 문제를 적는다

**익명화.** 청취자에게는 중립 이름의 파일(`abx001-A.wav` 등)과 시트만 준다. 정답 키(`blind_key.json`)는 진행자만 가진다. 같은 파일을 복사하므로 파일 내용을 비교하면 정답을 알 수 있다. 청취자에게 귀로만 판단하도록 안내한다.

### 청취 규칙

- 청취자는 3명 이상이다. 그중 기타 연주자 1명, 베이스 연주자 1명을 포함한다.
- 헤드폰 또는 모니터 스피커 중 하나로 고정하고, 볼륨은 시작 때 정한 뒤 바꾸지 않는다.
- 시험당 반복 재생은 자유다. 휴식은 20분마다 둔다.
- 청취자는 product/candidate의 의미와 기대 결과를 모른다.

### 판정 규칙 (사전 고정)

| 항목 | 기준 |
|---|---|
| PASS | 청취자 3명 이상의 쌍 비교를 합쳐서 볼 때 두 조건을 모두 만족 |
| ① 선호 | 선호를 표시한 시험 중 candidate 비율 ≥ 50% |
| ② 속성 | `attack`·`low_end` 평균은 product보다 0.5점 넘게 낮지 않고, `artifacts` 평균은 product보다 0.5점 넘게 높지 않음 |
| ABX | 청취자별 정답 수와 단측 이항 p값을 보고한다. 구별 가능해도 그 자체로 실패는 아니다 |
| INSUFFICIENT | 청취자가 3명 미만이거나 선호·속성 응답이 비어 있음 |

메모에 적힌 문제는 위치(테이크, 구간, 반음)와 함께 모두 보고한다. 결과를 본 뒤 기준·클립·가중치를 바꾸지 않는다.

### 자체 점검 (합성 WAV, T6 증거 아님)

`t6_listening.py selftest`로 실행했다. 실제 DI 검증 자체 점검의 합성 WAV 세 테이크를 사용했고, 결과는 다음과 같다.

- ABX 72회, 쌍 비교 36회가 만들어졌다.
- 청취자용 WAV 수가 계획과 일치했다(3 × 72 + 2 × 36).
- 음량 맞춤 이득은 +6.3–+18.8dB였다. 합성 입력이 작아서다.
- 가상 청취자 세 명(완벽, 무작위, candidate 선호 80%)의 결과가 예상대로 채점됐다.
  - 완벽 청취자: ABX 72/72
  - 무작위 청취자: 39/72(p = 0.28)
  - 합산 판정: PASS

## 2. DAW 수용

### 연구용 플러그인

`Tools/TransposeNext/plugin/`에 연구 전용 VST3(macOS에서는 AU도)를 추가했다. 이름은 **Chimera Transpose Research**이고, 제품 빌드·설치·배포와는 별개다. 제품과 같은 엔진 두 개를 TransposeProbe와 똑같이 설정해 감쌌다.

- **파라미터:**
  - `Shift`: −2…+2반음
  - `Engine`: Product baseline / corr40-split-v2
  - 엔진을 바꾸면 새 엔진을 초기화한다. 클릭이 날 수 있다.
- **지연 보고:**
  - product는 FFT 창 길이를 보고한다(48kHz에서 2048샘플).
  - candidate는 **0**을 보고한다. 고정 PDC 계약이 아직 없기 때문이다. DAW에서 candidate 트랙은 원음보다 2–15ms 늦게 들린다. 이것은 D05 항목에서 확인한다.
- **편집기:** JUCE 기본 편집기만 있다.

```sh
cmake -S Tools/TransposeNext/plugin -B build/transpose-plugin -DJUCE_DIR=<JUCE 8.0.8> -DCMAKE_BUILD_TYPE=Release
cmake --build build/transpose-plugin --parallel 4
# 헤드리스 호스트 점검 (Linux는 xvfb-run을 자동으로 쓴다)
cd Tools/TransposeNext && python t6_host_check.py \
  --plugin "../../build/transpose-plugin/ChimeraTransposeResearch_artefacts/Release/VST3/Chimera Transpose Research.vst3" \
  --host-check ../../build/transpose-plugin/TransposeResearchHostCheck_artefacts/Release/TransposeResearchHostCheck --out ../../build/t6-host-check
```

### 헤드리스 호스트 점검 (자동)

`TransposeResearchHostCheck`는 빌드한 `.vst3`를 DAW처럼 스캔·생성해 처리한다. 그 출력을 엔진을 직접 호출한 결과(TransposeProbe 설정, block 128)와 비교한다.

| 점검 | 내용 | 판정 |
|---|---|---|
| 고정 블록 128 | 플러그인 래퍼가 출력을 바꾸지 않는가 | 비트 동일 |
| 지연 보고 | product = FFT 창, candidate = 0 | 일치 |
| 가변 블록 1–512 | DAW처럼 블록 크기가 매번 다를 때(시드 고정 무작위) | 비트 동일 |
| 큰 블록 1024/2048/4096 | 큰 버퍼 | 차이를 보고만 함 |
| 재준비 | 다른 표본률로 준비한 뒤 원래 표본률로 되돌림 | 비트 동일 |
| 상태 저장·복원 | 새 인스턴스로 상태를 옮김 | 비트 동일 |
| 자동화 | 50ms마다 반음 변경, 엔진 1회 전환, 무작위 블록 | 유한값, peak ≤ 입력의 4배 |

결과는 §3에 있다.

### 실제 DAW 점검표 (수동)

`Validation/transpose-daw-acceptance-template.csv`에 12개 항목(D01–D12)을 정리했다.

| 항목 | 내용 |
|---|---|
| D01 | 스캔·로드 |
| D02–D03 | 오프라인 재현성, 기준 렌더와의 일치 |
| D04–D05 | PDC 동작 |
| D06 | 실시간 연주 체감 |
| D07 | 자동화 |
| D08 | 표본률 변경 |
| D09 | 버퍼 크기 |
| D10 | 저장·복원 |
| D11 | freeze·render |
| D12 | CPU |

**시험 환경:**

| OS | DAW |
|---|---|
| Windows | Reaper, Cubase, Ableton Live, Studio One |
| macOS | Reaper, Logic(AU), Ableton Live |

DAW마다 CSV 한 장을 채운다. D06은 실제 기타·베이스로 연주한다. 64샘플 버퍼에서 candidate와 product를 번갈아 써 보고, 어택 타이밍 체감을 1–5점으로 적는다. 오프라인 청취로는 판단할 수 없는 항목이다.

## 3. 자동 점검 결과와 발견 사항

헤드리스 호스트 점검을 144회 실행했다. 조합은 표본률 3(44.1/48/96kHz) × 입력 6 × 엔진 2 × 반음 4(±1/±2)다. 입력은 B0·A0·A1 각각의 holdout 랜덤 피킹(시드 8139)과 긴 음이다.

| 점검 | product | candidate (v2) |
|---|---|---|
| 고정 블록 128 비트 동일 | 72/72 | 72/72 |
| 지연 보고 | 72/72 (2048·4096샘플) | 72/72 (0) |
| 가변 블록 1–512 비트 동일 | 72/72 | **68/72** (최대 차이 8.6e-4) |
| 큰 블록 1024/2048/4096 차이 | 0/216 | **24/216** (최대 1.7e-3) |
| 재준비 비트 동일 | 72/72 | 72/72 |
| 상태 저장·복원 비트 동일 | 72/72 | 72/72 |
| 자동화 (유한값, peak ≤ 입력의 4배) | 72/72 | 72/72 |

오류나 충돌은 0건이었다. AddressSanitizer 빌드에서도 메모리 오류가 없었다(B0 긴 음 48kHz −2).

### 발견 1: candidate 출력이 블록 분할에 따라 달라진다

**나타나는 조건.** 차이는 모두 **긴 음, −2반음**에서만 나왔다.

- 가변 블록: 48kHz 세 악기, 44.1kHz 기타
- 큰 블록: 44.1·48kHz는 1024 이상, 96kHz는 2048 이상, 세 악기 모두

차이가 처음 나는 곳은 마지막 음의 감쇠 구간이다. 저역 무게중심 지연 차이는 0.02ms 이하로, T1–T4 판정에는 영향이 없다. 다만 같은 입력이라도 DAW 버퍼 설정에 따라 출력이 비트 단위로 달라질 수 있다.

**원인.** 연구 엔진의 단계별로 무작위 블록(1–512)을 넣어 비교했다(`blockdep.cpp`, `blockdep-B0-long-notes-48k.txt`).

- 차이가 없는 단계: split·low·up·lpc, 그리고 기존 corr40-novelty
- 차이가 처음 생기는 단계: rt

rt에서 추가된 기능은 '고역 경로가 이번 처리 구간에서 접합했으면 저역 접합 검색을 보류'하는 판정이다(`TransientSplitPitch::process`의 `setSplicesHeld`). 이 판정이 호스트 블록 단위로 내려져서 블록 분할이 결과에 들어간다.

**처리.** 이 PR에서는 고치지 않는다. 판정을 블록과 무관하게 바꾸면(예: 고역 접합 뒤 고정 샘플 수 동안 보류) v2의 고정 블록 출력도 달라진다. 그러면 지금까지의 합성·새 시드 검증을 다시 해야 한다. 별도 후보로 만들어 새 시드로 다시 검증하는 것이 맞다.

DAW 점검표 D09는 이 현상을 실제 DAW에서 확인하는 항목이다.

### 발견 2: denormal 처리

플러그인과 DAW는 denormal을 0으로 처리한다(`ScopedNoDenormals`). TransposeProbe와 비교 기준의 첫 버전은 그렇게 하지 않았다. 그 결과 v2는 72회 중 36회에서 **1.5e-36 크기의 차이**가 났다(`host_check-reference-without-ftz.json.gz`). 기준도 denormal을 0으로 처리하도록 고친 뒤에는 고정 블록이 72/72 비트 동일했다.

따라서 DAW 바운스와 TransposeProbe 렌더를 비교할 때(D03)는 denormal 크기 차이를 허용한다. 오디오에는 영향이 없다.
