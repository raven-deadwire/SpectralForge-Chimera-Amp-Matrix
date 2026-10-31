# Náströnd TONE3000 A2 NAM 개발 마무리

[English](NASTROND_NAM_A2_CLOSEOUT.md)

PR #216을 머지하면서 2026-10-29에 개발을 마무리했습니다. 이 문서는 다음 네 가지를 정리합니다.

- 무엇을 만들었는가
- 출시된 모델의 측정값
- 개발하면서 알게 된 것
- 남은 과제

실험별 근거는 [`Tools/Nam/reports`](../Tools/Nam/reports)에 있습니다.

## 결과

Náströnd 제품 채널 5개의 NAM A2-Full 캡처와 TONE3000 프리셋이 **출시 기준 v3로 출시 승인**됐습니다([release-approval-20261029](../Tools/Nam/reports/release-approval-20261029.md)). 승인 근거는 두 가지입니다.

- 모든 채널이 검증 신호와 학습에서 뺀 테스트 신호 양쪽에서 v3 수치 기준을 통과했습니다.
- 오너가 다섯 채널 모두 청음 확인을 완료했다고 알렸습니다.

원본 충실도 기준인 v2는 **통과하지 못했습니다.** 하이게인 채널의 full ESR은 0.017~0.022로, v2 한도 0.01을 넘습니다. TONE3000이 받는 모델 크기로는 이 한도에 닿을 수 없습니다([알게 된 것](#알게-된-것) 참고).

**배포 파일**
- NAM 가중치, 프리셋, 음원은 레포에 넣지 않았습니다.
- 릴리즈 zip: `Nastrond-A2-TONE3000-Release.zip`
- SHA-256: `ea96c4aec88baa0b23f6a7af00c14d1efe46bbb79e5a3fcef621c7f713afd957`
- 모델·프리셋별 해시는 승인 보고서에 있습니다.
- zip은 임시 세션에서 만들었습니다. zip 파일은 레포 밖에 따로 보관해야 하고, 해시는 이 문서가 영구 기록입니다.

## 출시된 모델

**공통 조건:** 모두 제품 기본값의 앰프 헤드 캡처입니다(캐비닛 없음, 48 kHz, A2-Full 8채널). 수치는 합성 검증·테스트 신호를 `quality_profile.fidelity`로 측정한 값입니다.

| 채널 | 캐릭터 | 검증 full ESR | 테스트 full ESR | 테스트 연주 구간 중앙값 / p95 / 최악값 | 테스트 조용한 구간 RMS / 피크 dBFS | 테스트 레벨 RMS / 피크 dB |
|---|---|---:|---:|---|---|---|
| Fenrir | 빠른 어택, 탄탄한 저역 | 0.0184 | 0.0198 | 0.0113 / 0.093 / 0.198 | −88.8 / −81.9 | −0.12 / −0.16 |
| Surtr | 고르게 퍼지는 컴프레션 | 0.0166 | 0.0190 | 0.0109 / 0.071 / 0.304 | −76.8 / −69.6 | −0.02 / +0.21 |
| Nidhoggr | 고르지 않은 드라이브 | 0.0202 | 0.0224 | 0.0141 / 0.080 / 0.376 | −77.7 / −71.0 | −0.07 / +0.09 |
| Fimbulvetr | 넓은 바디, 블룸 | 0.0068 | 0.0082 | 0.0062 / 0.026 / 0.088 | −81.9 / −75.4 | −0.04 / +0.90 |
| Ragnarok | 늦은 포화, 깊은 타격 | 0.0213 | 0.0186 | 0.0153 / 0.069 / 0.212 | −70.3 / −63.9 | −0.10 / +0.01 |
| **평균** | | **0.0167** | **0.0176** | | | |

**플레이어 검증:** TONE3000 `ef6f178`을 `ChimeraTone3000Tool`로 오프라인 빌드해서 확인했습니다.
- **A2 로드:** 모든 파일이 플레이어의 A2 판별을 A2-Full로 통과합니다.
- **출력 일치:** 플레이어 엔진 출력과 Python 모델 출력의 차이가 −133 dBFS 이하입니다.
- **프리셋 복원:** 6밴드 POST EQ가 들어간 프리셋을 다시 불러오면 소리가 비트 단위로 같습니다.
- **레벨 보정:** 채널 EQ를 거친 출력을 −20 dBFS RMS로 맞췄고, Normalize는 꺼져 있습니다.
- **무음:** 무음을 넣으면 출력이 −152 dBFS 이하입니다.

**모델별 출처**

| 채널 | 학습 | 보고서 |
|---|---|---|
| Surtr, Nidhoggr, Fimbulvetr | 16,320초 데이터, `--keep-passed` | [continued-16320-20261027](../Tools/Nam/reports/continued-16320-20261027.md) |
| Ragnarok | 채널 정규화 손실 + 피크 페널티 | [channel-loss-20261028](../Tools/Nam/reports/channel-loss-20261028.md) |
| Fenrir | Fenrir 전용 학습, 블록 정규화 손실 | [fenrir-quiet-pass-20261029](../Tools/Nam/reports/fenrir-quiet-pass-20261029.md) |

## 품질 기준

두 기준 모두 `Tools/Nam/quality_profile.py`에 있습니다.

**v2 (`nastrond-tone3000-fidelity-v2`): 원본 충실도 기준.** 바꾸지 않았고, 계속 함께 보고합니다.

| 항목 | 한도 |
|---|---|
| full ESR | 0.01 미만 |
| 연주 구간 ESR 중앙값 / p95 / 최악값 | 0.005 / 0.01 / 0.02 이하 |
| 조용한 구간 잔차 | RMS −70 dBFS, 피크 −60 dBFS 이하 |
| 레벨 | RMS ±0.5 dB, 피크 ±1 dB |

입력 이력이 전혀 없는 샘플(직전 수신 범위가 모두 무음)은 어떤 A2 모델도 예측할 수 없습니다. 그래서 이 샘플은 따로 보고만 하고 판정에서는 뺍니다.

**v3 (`nastrond-tone3000-a2-release-v3`): 출시 판정 기준** ([release-profile-v3-20261028](../Tools/Nam/reports/release-profile-v3-20261028.ko.md)). 측정 방법은 v2와 같고, 연주 구간과 full ESR 한도만 채널 종류별로 다릅니다.

| 항목 | 하이게인 (Fenrir, Surtr, Nidhoggr, Ragnarok) | Fimbulvetr |
|---|---|---|
| full ESR | 0.025 이하 | 0.01 이하 |
| 연주 구간 중앙값 / p95 / 최악값 | 0.02 / 0.12 / 0.40 이하 | 0.0075 / 0.03 / 0.20 이하 |
| 조용한 구간, 레벨 | v2와 같음 | v2와 같음 |

- **판정 조건:** 검증 신호와 테스트 신호를 모두 통과해야 하고, 출시에는 청음 확인도 필요합니다.
- **기준의 성격:** v3 한도는 결과를 본 뒤 측정된 A2 한계를 근거로 정했습니다. 그래서 "원본 DSP와 구별할 수 없다"는 보증이 아니라, "A2로 갈 수 있는 만큼 가까이 갔다"는 판정입니다.
- **여유가 작은 항목:**

| 항목 | 값 | 한도 |
|---|---|---|
| Nidhoggr 테스트 최악값 | 0.376 | 0.40 |
| Nidhoggr 테스트 full ESR | 0.0224 | 0.025 |
| Fenrir 검증 조용한 구간 피크 | −60.2 dBFS | −60 dBFS |

## 진행 경과

다섯 채널의 평균 full ESR입니다.

| 단계 | 검증 | 테스트 | 보고서 |
|---|---:|---:|---|
| 첫 v2 학습, 처음부터 5,000스텝 | 0.098 | | [train-v2-20261024](../Tools/Nam/reports/train-v2-20261024.md) |
| 50,000스텝 | 0.032 | | [train-v2-50k-20261025](../Tools/Nam/reports/train-v2-50k-20261025.md) |
| 낮은 학습률로 마무리 학습 | 0.025 | | [finish-ab-20261025](../Tools/Nam/reports/finish-ab-20261025.md) |
| 데이터 1,920초 | 0.021 | 0.0225 | [final-candidate-20261026](../Tools/Nam/reports/final-candidate-20261026.md) |
| 3,840초 | 0.0181 | 0.0193 | [continued-3840-20261026](../Tools/Nam/reports/continued-3840-20261026.md) |
| 7,680초 | 0.0168 | 0.0180 | [continued-7680-20261027](../Tools/Nam/reports/continued-7680-20261027.md) |
| 16,320초, `--keep-passed` | 0.0161 | 0.0174 | [continued-16320-20261027](../Tools/Nam/reports/continued-16320-20261027.md) |
| 출시 세트 (Ragnarok·Fenrir를 레벨·조용한 구간 기준에 맞춰 재학습) | 0.0167 | 0.0176 | [release-approval-20261029](../Tools/Nam/reports/release-approval-20261029.md) |

출시 세트의 평균이 16,320초 학습보다 조금 높은 이유는, 두 채널이 기준을 통과하는 대신 ESR을 조금 내줬기 때문입니다.

- **Ragnarok:** 테스트 피크 레벨이 +1.52 dB에서 +0.01 dB로 내려왔습니다. 대신 검증 ESR이 8% 늘었고(0.0197 → 0.0213), 테스트 ESR은 그대로입니다(0.0187 → 0.0186).
- **Fenrir:** 검증 조용한 구간 피크가 −53.3 dBFS에서 −60.2 dBFS로 좋아졌습니다. 대신 ESR이 검증·테스트 모두 약 8% 늘었습니다.

## 알게 된 것

**효과가 있었던 것**
- **학습량:** 스텝을 10배 늘리자 오차가 절반으로 줄었습니다.
- **데이터 양:** 효과는 있지만 갈수록 작아졌습니다. 데이터를 두 배로 늘릴 때마다 테스트 ESR이 −14%, −7%, −3.6% 줄었습니다. 이 추세로 보면 A2 너비에서는 약 0.017에서 멈춥니다.
- **모델 너비 (TONE3000은 받지 않음):** 같은 480초 데이터에서 너비 16은 Fenrir 오차를 34% 줄였고, 너비 32는 13%를 더 줄였습니다([capacity-diagnostic-20261026](../Tools/Nam/reports/capacity-diagnostic-20261026.md)).
- **학습 구간별 피크 페널티:** Ragnarok 레벨 문제를 해결했습니다([peak-penalty-20261027](../Tools/Nam/reports/peak-penalty-20261027.md), [peak-weight-20261027](../Tools/Nam/reports/peak-weight-20261027.md)). 이전에 쓰던 샘플 단위 오버슈트 페널티는 효과가 없었습니다. 피크 한도를 넘기는 것은 어택의 몇 샘플뿐이라, 모든 샘플에 걸면 평균에 묻히기 때문입니다.
- **블록 정규화 손실:** 오차를 512샘플 단위로 정규화해서, 큰 소리 구간 안에 있는 작은 여운도 학습에 반영되게 했습니다. 이걸로 Fenrir 조용한 구간 문제를 해결했습니다([fenrir-retrain-20261029](../Tools/Nam/reports/fenrir-retrain-20261029.md), [fenrir-quiet-pass-20261029](../Tools/Nam/reports/fenrir-quiet-pass-20261029.md)).

**효과가 없었던 것**
- **모델 기억 길이 늘리기:** 원본 DSP 자체에 132 ms보다 긴 기억이 없습니다([history-diagnostic-20261026](../Tools/Nam/reports/history-diagnostic-20261026.json), [memory-diagnostic-20261024](../Tools/Nam/reports/memory-diagnostic-20261024.md)).
- **큰 음 위주 학습, 진폭 가중 손실, 채널 정규화 손실:** 셋 다 full ESR을 줄이지 못했습니다([loud-crops-ab-20261026](../Tools/Nam/reports/loud-crops-ab-20261026.md), [finish-ab-20261025](../Tools/Nam/reports/finish-ab-20261025.md), [channel-loss-20261028](../Tools/Nam/reports/channel-loss-20261028.md)).
- **여운 구간만 더 많이 학습:** Fenrir 조용한 구간이 움직이지 않았습니다([fenrir-quiet-20261028](../Tools/Nam/reports/fenrir-quiet-20261028.md)).

**제약**
- **TONE3000은 A2만 불러옵니다.** 너비 3 또는 8에, 층 구성도 고정돼 있어야 합니다([tone3000-a2-only-20261028](../Tools/Nam/reports/tone3000-a2-only-20261028.md)). 더 큰 모델은 다른 NAM 플레이어에서만 쓸 수 있습니다.
- **오차 대부분은 세게 친 음에서 나옵니다.** 입력 −6 dBFS 이상인 큰 음은 작은 음보다 오차가 3~6배 크고, 그 오차의 약 80%가 5 kHz 아래에 있습니다.

**정정 기록**
- **Fenrir 조용한 구간 원인:** 처음에는 합성 신호 끝부분이 갑자기 끊기기 때문이라고 판단했습니다. 하지만 끝을 페이드아웃시켜도 문제가 그대로였습니다([faded-heldout-20261029](../Tools/Nam/reports/faded-heldout-20261029.md)). 실제 원인은 입력이 끝난 뒤의 작은 여운을 A2가 제대로 따라가지 못하는 것이었습니다.

## 재현 방법

도구는 `Tools/Nam`에 있고, 전체 흐름은 [NASTROND_NAM_TOOLCHAIN](NASTROND_NAM_TOOLCHAIN.ko.md)에 설명돼 있습니다. 이번 출시는 다음 순서로 만들었습니다.

1. **빌드:** `Tools/Nam/ci-sources.json`에 고정된 JUCE와 TONE3000 플레이어 커밋으로 `Tools/Nam/CMakeLists.txt`를 사용해 `ChimeraNamRender`와 `ChimeraTone3000Tool`을 빌드합니다.
2. **캡처:** `capture_channels.py --train-seed N`으로 480초 단위를 캡처합니다. 검증·테스트·청음용 시드는 고정돼 있습니다.
3. **학습 데이터 늘리기:** `combine_captures.py`나 `append_captures.py`로 합칩니다. 렌더러가 한 번에 600초까지만 받기 때문입니다.
4. **학습:** `train_a2.py --recipe legacy --center-training-output`으로 학습합니다. 최고 모델에서 이어서 학습할 때는 `--keep-passed`와 학습률 워밍업을 씁니다. 채널 하나만 고칠 때는 `--train-channels`, `--target-limits`, `--block-norm-samples`, `--peak-weight`를 함께 씁니다.
5. **패키징과 검증:** `package_a2.py`로 패키지를 만들고, `ci_pipeline.compare_package`로 검증합니다.
6. **승인:** `approve_release.py`로 승인합니다. 모든 채널이 v3를 양쪽 신호에서 통과하지 않으면 승인을 거부하고, 오너의 청음 확인 내용을 기록합니다.

**실무 메모**
- **데이터 크기:** 16,320초 데이터는 18 GB로 메모리(16 GB)보다 큽니다. 그래서 메모리 매핑으로 읽고, 채널 하나만 학습할 때는 그 채널 데이터만 읽습니다.
- **비정규 부동소수점:** 1e-30 근처로 줄어든 가중치 때문에 CPU 학습이 4.5배 느려진 적이 있습니다. 지금 `train_a2.py`는 이런 수를 0으로 처리합니다.
- **학습 속도:** 4스레드 CPU 기준으로 5채널 동시 학습 10,000스텝에 약 1시간 걸립니다.

## 남은 과제

- **청음 기록:** 오너의 청음 확인은 받은 그대로 기록했습니다. 어떤 방법이나 음원으로 들었는지는 기록에 없습니다.
- **실사용 검증 없음:** GUI/DAW에서 실제로 써 보는 검증은 하지 않았습니다. 모든 측정은 합성 신호로 했고, 실제 악기 DI 녹음은 쓰지 않았습니다.
- **조건이 바뀌면 다시 확인:**
  - `4ef21c16`에 고정한 DSP 헤더가 바뀌면 지금 캡처와 모델은 더 이상 제품을 대표하지 않습니다. 다시 캡처하고 재판정해야 합니다.
  - TONE3000 플레이어가 `ef6f178`보다 새 버전이면 패키지 검증을 다시 돌려야 합니다.
- **v3 이상의 충실도:** A2보다 큰 모델이 필요합니다. 즉 TONE3000이 아닌 별도 빌드가 필요합니다.
