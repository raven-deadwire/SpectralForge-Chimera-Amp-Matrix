# Náströnd NAM 비교 도구 독립 검증 — 2026-10-25 KST

PR #145의 검증 도구는 아래 보강 후 로컬 회귀·실제 NAM 비교·기존 지표 호환성 검증을 통과했다. **앰프 채널의 음색 목표는 전부 통과하지 않았다.** FAIL/BLOCKED를 유지하며 DSP나 판정 임계값은 변경하지 않았다.

검증 소스: `ef4a256c921ebb10d169b39ea31b436fe767a52e`.
초기 리뷰 소스: `ae75e76128dea6f354afe2eed08862b6ba4e886c`.
이 보고서와 증거 추가는 검증 소스의 Tools/Tests/Source를 바꾸지 않는다.

## 1. 발견한 결함과 수정

| 재현한 결함 | 수정 및 회귀 검증 |
|---|---|
| 보고서 세 개에 같은 누락이 있으면 26개 중 25개만 있어도 PASS. 중복 ID도 dict 변환에서 사라짐 | 매니페스트의 정확한 참조 집합과 대조하고 중복 ID를 거부 |
| 세 보고서의 NAM SHA-256을 함께 바꾸면 PASS | 선택된 매니페스트 파일 자체의 SHA-256 및 각 캡처의 SHA·family·gain group·scope·입력 게인/variant를 직접 대조 |
| 프로토콜이나 기본 상태를 세 보고서 모두에서 바꾸면 상호 비교만으로 감지 불가 | 고정 프로토콜 및 17개 기본 상태 정의와 직접 대조 |
| `--min-improvement-db nan`이 실패 조건을 우회 | 유한한 비음수 개선 임계값, 유한한 양수 드리프트 한계만 허용 |
| 무음 입력의 상관계수는 NaN인데 polarity가 +1로 기록됨 | 무음·상수·비유한·길이 불일치·불충분한 분석 창을 명시적으로 거부 |
| 채널 상태 dict의 키 순서만 달라도 다른 정의로 판단 가능 | 문자열 표현 대신 구조적 동등성 비교. 불완전한 매크로 끝점도 거부 |

기존 매니페스트의 SHA 형식과 boolean 필드 검증도 보강했다. 캐시 파일 변조 시 해당 렌더만 다시 실행되는지, 모델 파일 변조 시 SHA 불일치로 중단하는지 시험했다. 임계값은 종전의 **각 대상 캡처·각 구간마다 0.1dB 이상 개선**, 기본 레벨 드리프트 **0.001dB 미만**을 유지했다.

## 2. 동일 입력 합성 오차 시험

48kHz, 8초, NumPy RNG seed 145의 광대역 신호에 명시적인 FFT 대역 게인을 적용했다. 표의 점수는 동일 reference/candidate를 기존 지표와 새 지표에 넣은 결과다. 스펙트럼 비교 전에 전체 RMS를 맞추는 기존 규약을 유지했다.

| 오차 | 기존 선형 빈 지표 (dB) | 42개 로그 대역 지표 (dB) |
|---|---:|---:|
| 동일 신호 | 0 | 0 |
| 65–250Hz −12dB | 1.783196 | 6.091449 |
| 2–8kHz −12dB | 9.436315 | 5.869379 |
| 65–250Hz −20dB | 2.950280 | 9.977181 |
| 2–8kHz +20dB | 8.634166 | 12.155098 |
| 극성 반전 | 0 | 0 |
| 전체 게인 ×2 | 0 | 0 |
| 32샘플 지연 | 0.006799 | 0.002958 |

비슷한 약 2옥타브 폭의 −12dB 오차에서 종전 지표는 고역을 5배 이상 크게 평가했다. 새 지표는 두 점수가 0.223dB 차이여서 저역 과소 반영을 줄였다. 완전히 같지 않은 이유에는 전체 RMS 정규화와 유한 창·대역 경계가 포함된다.

극성 반전은 correlation −1, polarity −1로 식별했다. 전체 게인 ×2는 match_gain_db −6.020600으로, 32샘플 지연은 lag_samples −32로 식별했다. **두 스펙트럼 점수는 위상에 둔감한 크기 오차의 RMS이며, 값 하나로 오차의 위치·방향·음질을 결정할 수 없다.** 일반 신호에서 음의 상관 부호만으로 물리적 극성 반전을 확정할 수도 없다.

새 지표는 65–8000Hz 안의 로그 대역을 균등 취급한다. 65Hz 미만과 8kHz 초과는 평가 범위 밖이다. reference의 상대 파워가 −50dB 아래인 대역은 제외한다. 이번 실제 참조 구간에서 활성 대역은 39–42개였다. 청각 가중치나 지각 품질 점수로 해석하지 않는다.

전체 수치: [synthetic-errors.json](evidence/nastrond-toolchain-review-20261025/synthetic-errors.json), [active-bands.json](evidence/nastrond-toolchain-review-20261025/active-bands.json).

## 3. 실제 NAM 26개와 39개 상태

이번 환경에서는 공개 참조 파일에 접근할 수 있었다. **26/26개 파일이 동결 매니페스트 SHA-256과 일치**했다. NAM 가중치와 렌더된 오디오는 저장소에 추가하지 않았다.

- NAM Core: `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`, `--slim 1`.
- Original renderer: 위 소스의 `Tests/RenderOriginalAmp.cpp`, JUCE 8.0.8, 4배 오버샘플링. GCC 13.3.0으로 실제 JUCE DSP/audio_formats/audio_basics/core 모듈과 직접 빌드했다. 여기서 전체 제품 패키지를 새로 빌드한 것은 아니다.
- 입력: 동결된 37초 합성 프로브. 실제 NAM 파일을 사용했지만 연주자가 녹음한 instrument DI는 아니다.
- 17개 기본 상태 + 22개 추가 채널 상태 = 39개.
- 전체 26개 캡처의 동특성을 측정하고, 앰프 단독 비교가 허용된 18개 캡처와 보정 variant를 포함해 **1,638개 구간 비교**를 생성했다.
- report의 source.tool_commit은 검증 SHA, tool_tree_dirty는 false다. 매니페스트·프로브·추가 상태·양쪽 렌더러·각 출력 오디오의 SHA-256은 [comparison.json](evidence/nastrond-toolchain-review-20261025/comparison.json)에 있다.

현재 소스의 매크로 방향만 감사하므로 baseline/incoming/candidate에 같은 새 보고서를 사용했다. 이것은 이전 DSP 버전 대비 개선량 측정이 아니다.

| 평가 범위 | 기존 지표 | 로그 대역 지표 | 새 지표의 기준 미달 항목 |
|---|---|---|---|
| 기본 상태 CRUSH/IMPACT/ROT/BLOOM | PASS | PASS | 없음 |
| Fenrir / CLANK | BLOCKED | BLOCKED | 참조가 amp+cab이므로 앰프 단독 대상 없음 |
| Surtr / CRUSH | PASS | FAIL | 569474 pluck −0.076418dB; 569477 chord −0.010286dB |
| Nidhoggr / ROT | FAIL | FAIL | 421075 pluck −0.097745dB; 421074 pluck +0.004400dB; 421075 chord +0.056546dB |
| Fimbulvetr / BLOOM | PASS | PASS | 없음 |
| Ragnarök / IMPACT | FAIL | FAIL | 652436 pluck +0.071245dB |

개선량은 `endpoint_0 점수 − endpoint_1 점수`다. 음수는 목표와의 거리가 늘어난 것이고, 양수라도 0.1dB 미만이면 FAIL이다. Surtr는 새 지표에서 기존 PASS가 FAIL로 바뀐다. Ragnarök pluck은 평균으로 개선됐지만 한 캡처가 기준에 미달한다. 평균으로 개별 실패를 숨기지 않았다. Fenrir는 [scope-blockers.json](evidence/nastrond-toolchain-review-20261025/scope-blockers.json)에 별도 기록했다.

결과 파일은 [증거 폴더](evidence/nastrond-toolchain-review-20261025/)의 `audit-v1-*.json`, `audit-band-*.json`에 있으며 실패한 감사도 JSON을 보존했다.

## 4. 호환성과 동결 증거

1. 2026-10-05 baseline으로 gain CSV를 다시 생성: 동결 파일과 바이트 단위 동일.
2. 동결 baseline/voicing/macro-direction 보고서 감사: 기존 summary의 모든 키·값 동일.
3. 동결 39상태 채널 보고서 감사: 기존 지표에서 Surtr/Nidhoggr/Fimbulvetr PASS, Ragnarök의 652436 pluck +0.034591dB FAIL 유지. 이는 위의 **현재 소스 측정**과 다른 과거 증거다.
4. 리팩터링 전 `24273e99df2d58d97d9cb907e60f1de0b5c0166d`의 비교 스크립트를 그대로 실행하고, 같은 실제 NAM/Original 오디오 캐시를 재사용했다. **기존 스칼라 필드 15,369개에서 차이 0개**. v2의 추가 필드는 비교에서 제외했다.
5. v1/v2 voicing fit에 같은 실제 11개 목표 렌더와 같은 보고서를 전달: 출력 바이트 단위 동일. 현재 default를 pre-contour와 baseline 모두에 준 호환성 실험으로, 제안된 EQ를 제품에 적용하거나 새 보이싱을 승인하지 않았다.
6. 기존 네 동결 증거 폴더는 수정하지 않았다.

[verification.json](evidence/nastrond-toolchain-review-20261025/verification.json)에 환경·구버전 보고서 해시·fit 해시를 기록했다. [SHA256SUMS.txt](evidence/nastrond-toolchain-review-20261025/SHA256SUMS.txt)는 새 증거 파일의 무결성 목록이다.

## 5. 테스트와 CI 적용 범위

- 로컬 `Tools/test_nastrond_nam.py`: 10개 회귀 함수 PASS. 합성 오차, 무효 입력, 동결 재생성, 매니페스트 대조, 채널 실패 보존, 39상태 대체 렌더러 통합, 캐시/모델 변조를 포함한다.
- 실제 바이너리 `Tests/test_original_renderer.py`: PASS. 상태·preset·gain 라우팅, 결정적 출력, 유한 float, 잘못된 상태 거부와 진단 메시지를 검증했다.
- 초기 PR SHA `ae75e76`의 [CI 실행 40105669492](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40105669492): NAM validation-contract 및 Windows/macOS/Linux build 작업 성공. macOS UI 시험은 재실행에서 성공했다.
- 같은 실행의 전체 결론은 failure다. `consolidate-release-gate`가 **1 PASS / 127 BLOCKED**로 종료했으며, 미실행 항목과 재시도 간 Windows/Linux evidence identity 불일치가 기록돼 있다. 이를 PASS로 변경하지 않았다.
- 보강 소스 `ef4a256`의 [CI 실행 40128623316](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40128623316)은 보고서 작성 시 대기 중이었다. 로컬 결과를 새 GitHub CI 성공으로 표시하지 않는다. 후속 문서 커밋이나 통합 SHA의 상태는 해당 체크를 별도로 확인해야 한다.

## 6. 재실행

가중치·오디오는 저장소 밖의 개인 경로에 둔다. 렌더러 준비는 [측정 절차](NASTROND_NAM_BAND_MEASUREMENT.md)를 따른다.

```sh
python Tools/test_nastrond_nam.py
python Tests/test_original_renderer.py /path/to/ChimeraOriginalAmpRender
python Tools/nastrond_nam_tool.py fetch --out /private/nam
python Tools/nastrond_nam_tool.py compare \
  --models /private/nam --nam-render /path/to/nam-core/build/tools/render \
  --original-render /path/to/ChimeraOriginalAmpRender \
  --states docs/evidence/nastrond-channels-20261005/channel-states.json \
  --out /private/nastrond-comparison --workers 4
python Tools/nastrond_nam_tool.py audit \
  --baseline /private/nastrond-comparison/comparison.json \
  --incoming /private/nastrond-comparison/comparison.json \
  --candidate /private/nastrond-comparison/comparison.json \
  --metric band_spectrum_rms_db --scope channel_surtr \
  --out /private/nastrond-comparison/audit-band-surtr.json
```

기존 지표는 `--metric log_spectrum_rms_db`로 같은 보고서를 감사한다. scope를 빼면 기본 상태, `channel_nidhoggr`, `channel_fimbulvetr`, `channel_ragnarok`이면 각 채널이다. FAIL의 비정상 종료는 관측 결과이므로 그 보고서를 삭제하거나 임계값을 낮춰 통과시키지 않는다.

실제 하드웨어 매칭, 절대 입력 캘리브레이션, Granophyre 출처 확인, 소유자의 instrument-DI 청취 검증은 완료하지 않았다. 이번 결과가 허용하는 결론은 **검증 도구의 호환성·오차 감지·증거 검증이 개선됐고, 현재 모델의 남은 실패를 더 명확히 드러낸다**는 것이다.
