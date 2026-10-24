# Transpose T6 실제 DI 검증 프로토콜 — 2026-10-24 KST

T6(외부 수용) 중 **실제 DI 녹음으로 T1–T4를 다시 재는 부분**의 준비 문서다. 녹음은 아직 없다. 이 문서와 도구는 녹음이 들어왔을 때 결과를 보기 전에 절차와 판정 방식을 고정해 두려는 것이다.

T6의 나머지(청취, Windows/macOS DAW 수용, 고정 PDC, 제품 엔진 통합)는 여기서 다루지 않는다. 이 검증을 통과해도 제품 승인은 아니다.

- 도구: `Tools/TransposeNext/di_validation.py`
- 녹음 목록 템플릿: `Validation/transpose-di-take-template.json`
- 기준: `Validation/transpose-latency-targets.json`(합성 기준과 같은 파일, 같은 한계값)

## 1. 녹음

**악기와 테이크.** 합성 기준과 같은 세 저음을 녹음한다. 테이크 하나에 아래 일곱 구간을 순서대로 담는다.

| 테이크 | 최저음 |
|---|---|
| 기타 drop A | A1 55.0Hz |
| 5현 베이스 | B0 30.87Hz |
| 5현 베이스 drop A | A0 27.5Hz |

| 구간 | 종류(`kind`) | 연주 | 최소 개수 |
|---|---|---|---|
| riff | `riff` | 저음현 리프, 8분·16분음표, 120–180 BPM. 기타는 팜뮤트 | 24음 |
| dynamics | `dynamics` | 저음현에서 세게·약하게 번갈아 피킹 | 16음 |
| long | `long` | 저음 단음을 250–400ms 울린 뒤 뮤트, 반복 | 10음 |
| ghost | `ghost` | 저음 개방현을 한 번 치고 울리는 동안, 뮤트한 고음현을 가볍게 긁기 | 8회 |
| steady-open | `steady` | 저음 개방현 한 번, 비브라토 없이 5초 이상 | 1음 |
| steady-fret5 | `steady` | 저음현 5프렛 한 번, 비브라토 없이 5초 이상 | 1음 |
| vibrato | `guard` | 한 번 친 뒤 손가락 비브라토 4초 이상, 추가 피킹 없음 | 1음 |

**녹음 조건:**

- 모노, 드라이 DI. 앰프·캐비닛·게이트·컴프레서·EQ·피치 효과·노멀라이즈를 모두 끈다.
- 48kHz는 필수다. 인터페이스가 지원하면 96kHz 테이크를 별도로 녹음한다(재표본화로 만들지 않는다). 44.1kHz는 선택이다.
- PCM16/24/32 또는 float32 WAV. 클리핑이 없어야 한다(peak −1dBFS 이하 권장, −40dBFS보다는 커야 함).
- 테이크 중간에 인터페이스 게인을 바꾸지 않는다.
- 구간 앞뒤에 **최소 0.5초 무음**(현을 뮤트)을 둔다. 도구는 구간 처음·끝 0.2초가 구간 peak보다 40dB 이상 작은지 확인한다.
- `riff` 구간은 20초 이내로 한다. 엔진 onset 기록이 렌더당 256개까지이고, 넘으면 도구가 실행을 거부한다.
- 악기, 픽업, 볼륨·톤, 줄 상태, 피크/손가락, 인터페이스, 게인, 연주자, 날짜를 `take_note`에 적는다.

**파일 관리.** 녹음 파일은 기본적으로 저장소에 넣지 않는다. 결과 공개가 필요하면 권리자 동의를 받고 SHA-256과 함께 증거 폴더에 넣는다.

## 2. 절차

```sh
python Tools/TransposeNext/build.py --juce <JUCE 8.0.8> --out build/transpose-next
cp Validation/transpose-di-take-template.json ~/t6/takes.json   # wav 경로와 구간 시간을 실제 테이크에 맞춘다

# 1) 입력 점검: 빠진 것과 문제(클리핑, 표본률, 무음 경계 등)를 나열한다
python Tools/TransposeNext/di_validation.py plan --manifest ~/t6/takes.json

# 2) 기준 onset 후보 생성 → 사람이 검토
python Tools/TransposeNext/di_validation.py label --manifest ~/t6/takes.json --take guitar-A1-48k --out ~/t6/guitar-A1-48k-labels.csv

# 3) 사전 등록: 결과를 보기 전에 커밋·push한다
python Tools/TransposeNext/di_validation.py freeze --manifest ~/t6/takes.json --renderer build/transpose-next/TransposeProbe --out docs/evidence/<폴더>/preregistration.json

# 4) 실행: 사전 등록과 하나라도 다르면 거부한다
python Tools/TransposeNext/di_validation.py run --manifest ~/t6/takes.json --renderer build/transpose-next/TransposeProbe --prereg docs/evidence/<폴더>/preregistration.json --out ~/t6/result
```

### 기준 onset 라벨

실제 녹음에는 정답 onset이 없다. 그래서 사람이 확정한 라벨을 정답으로 쓴다.

- **후보 생성.** `label`이 오프라인 검출기로 후보를 만든다. 이 검출기는 엔진 검출기와 독립이다.
  - 검출은 1–8kHz 대역에서 한다. 앞 5ms 평균 에너지가 직전 40ms의 최대 5ms 평균보다 9dB 이상 커야 한다. 40ms는 25Hz 현의 한 주기라서, 밝은 지속음이나 비브라토의 주기적 에너지에서는 재검출되지 않는다.
  - 위치는 인과 필터 포락선으로 되짚어 정한다. 0이 아닌 위상 필터의 선행 울림이 위치를 앞당기지 않게 하기 위해서다.
- **검토.** 검토자는 모든 행을 듣고 파형을 보며 확인한다.
  - 공격이 아닌 행은 `keep=no`로 바꾼다. 예: 뮤트 소리, 줄 끝 처리 잡음.
  - 빠진 공격은 행을 추가한다. 의도한 모든 피킹·뜯기가 라벨이어야 한다.
  - 위치는 공격 시작 쪽에 둔다.
  - 끝나면 `labels_reviewed: true`로 바꾼다.
- **검토 규칙:**
  - 검토 중에는 엔진 출력을 보지 않는다.
  - `freeze` 뒤에는 라벨을 바꾸지 않는다.
- **도구 검사.** `steady`·`guard` 구간은 라벨이 정확히 1개여야 한다. 다른 구간은 위 최소 개수를 넘어야 한다. 둘 다 도구가 확인한다.

### 사전 등록이 고정하는 것

`freeze`는 다음의 SHA-256을 기록한다.

- 녹음 목록
- 각 WAV와 라벨
- renderer
- 기준 JSON
- 이 문서
- 도구 스크립트

여기에 엔진 이름과 git 커밋도 기록한다. 적용 규칙은 새 시드 검증과 같다.

- 엔진마다 한 번만 실행한다.
- 결과를 본 뒤 라벨·구간·지표·기준·파라미터를 바꾸지 않는다.
- 실패도 모두 보고한다.

## 3. 판정 (실제 DI에 맞춘 부분)

조건은 녹음 표본률, block 128, −2/−1/+1/+2반음이다. 구간은 따로 렌더한다. 한계값은 합성 기준과 같다. 바꾼 점은 정답 onset이 없거나 실제 현이 합성 음과 달라서 불가피한 것만이며, 아래에 모두 적었다.

| 기준 | 지표 | 합성 기준과 다른 점 |
|---|---|---|
| T1 어택 ≤10ms | 같은 1–8kHz 2ms RMS peak 지연(출력 − 입력). 입력 자체 peak가 라벨 뒤 10ms 안에 있는 이벤트만 센다 | 반음당 유효 이벤트가 20개 미만이면 `INSUFFICIENT` |
| T2 다운시프트 저역 ≤10ms | 같은 120Hz 저역 무게중심 지연. `riff`·`dynamics`는 짧은 음, `long`은 긴 음 | 짧은 음·긴 음 중 하나라도 없으면 `INSUFFICIENT` |
| T3 업시프트 저역 | 같은 지표, 악기별 보고 | 합성 비회귀 상한은 **참고로만** 함께 적는다. 실제 음 길이·감쇠가 달라서 상한 판정을 하지 않는다 |
| T4 검출 | 라벨마다 엔진 onset이 [라벨 −2ms, 라벨 +10ms] 안에 있어야 한다. 짝이 없는 엔진 onset은 오검출이다(렌더 시작 10ms 제외) | 라벨 오차를 감안해 앞쪽 허용 2ms를 더했다. 합성은 [0, +10ms] |
| T4 정상음 | `steady` 구간에서 라벨 0.5초 뒤부터 2초 분석. 출력 음정 오차는 **입력 실측 음정 × 2^(반음/12)** 기준 ≤1 cent. 워블은 **출력 − 입력** ≤1 cent | 실제 현은 음정이 조금씩 흔들리고 조율도 정확하지 않아서 상대값으로 바꿨다 |
| T4 저역 재설정 | `ghost`·`steady`·`guard` 구간에서 저역 재설정이 첫 번째 이후 0회 | 합성은 시작부를 빼고 셌다. 실제 DI는 첫 음을 직접 치므로 1회를 허용한다 |

오검출은 위치(테이크, 구간, 반음, 시각)를 함께 출력한다. 청취로 원인을 확인할 수 있게 하기 위해서다. 원인을 확인한 뒤에도 판정은 바꾸지 않는다.

## 4. 도구 자체 점검 (T6 증거 아님)

`selftest`는 정답 onset을 아는 합성 WAV로 파이프라인을 점검한다. 합성 WAV는 세 저음 각각에 대해 riff·dynamics·long·ghost·steady·guard 구간을 담는다.

```sh
python Tools/TransposeNext/di_validation.py selftest --renderer build/transpose-next/TransposeProbe --out build/t6-selftest
```

2026-10-24 결과(`corr40-split-v2`, 48kHz, ±2반음):

- **라벨러:** 세 테이크 모두 정답 61개 전부를 ±0.56ms 안에서 찾았다.
  - 추가 후보는 테이크당 17개였다.
  - 16개는 합성 리프 음이 61.5ms에서 잘리는 지점이고, 1개는 비브라토 구간 끝이다.
  - 실제 녹음에서는 이런 후보를 검토자가 지운다.
- **판정 경로:** T1 5.58ms, T2 짧은 음 6.28ms·긴 음 9.34ms, T4 오검출 0·누락 0·워블 증가 0.30 cents. 합성 기준 결과와 같은 범위다.
- **알려진 위험:** 점검 중 B0·A0 정상음 끝에 50ms 페이드를 두 번 겹쳐 적용했을 때, 페이드 시작점에서 엔진이 onset을 잘못 잡았다(각 1건). 이전에 96kHz 음 끝 페이드에서 본 약점과 같은 종류다. 실제 연주의 뮤트·음 끝에서도 같은 오검출이 나올 수 있어서, T6에서 가장 먼저 확인할 항목이다.
