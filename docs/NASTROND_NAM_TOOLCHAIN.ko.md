# Náströnd NAM 툴체인: 점검과 재설계

[English](NASTROND_NAM_TOOLCHAIN.md)

2026-10-24에 `24273e9` 기준으로 점검했습니다. 대상은 Náströnd NAM 레퍼런스를 받고,
`OriginalAmpProcessor`와 나란히 렌더링하고, 결과를 요약하는 도구들입니다
(`fetch_nastrond_references.py`, `compare_original_nam.py`, `audit_nastrond_macros.py`,
`fit_original_voicing.py`, `ChimeraOriginalAmpRender`).
리포트는 여전히 관찰 기록일 뿐입니다. 하드웨어와 같은 소리라거나 오너 청음을 통과했다는 인증이 아닙니다.

## 찾은 문제

| # | 문제 | 영향 | 근거 |
|---|---|---|---|
| 1 | 스펙트럼 점수 `log_spectrum_rms_db`가 균등 간격 Welch 칸을 평균합니다. 1354칸 중 1024칸이 2–8 kHz이고 250 Hz 아래는 31칸입니다. | 모든 매크로 audit와 보이싱 기록이 기대는 점수가 사실상 고음을 잽니다. 저음이 20 dB 빠져도 거의 움직이지 않는데, 로우튠 리듬 앰프에서는 중요한 문제입니다. 보이싱 fit는 이미 42개 로그 밴드를 쓰고 있어서 fit와 audit의 스펙트럼 가중치가 서로 달랐습니다. | `test_band_metric_and_polarity`: 250 Hz 아래를 −20 dB 깎으면 옥타브 밴드 점수가 v1 점수의 두 배를 넘습니다. |
| 2 | 상관계수의 부호가 무엇을 뜻하는지 기록하지 않습니다. | VH4 CH3는 기본 상태 대비 `correlation −0.63`입니다. 제한된 지연 탐색에서 음의 상관이 관측됐다는 뜻입니다. 상관의 크기는 부분적 유사성이며 부호만으로 물리적 극성 반전을 확정할 수 없습니다. | `nastrond-nam-20261005/comparison.json` |
| 3 | 파생 증거를 만드는 도구가 없습니다. 증거 폴더 세 곳의 `gain-summary.csv`가 두 가지 형식이고, 채널 `summary.json`은 생성 코드가 없습니다. | 공개된 요약을 다시 만들거나 검증할 수 없습니다. | `nastrond-nam-…`와 `nastrond-voicing-…`의 CSV 헤더 |
| 4 | 프로토콜 상수를 도구마다 다시 적었습니다. `37*SR`, `21*SR`, `int(hz*k/4)`, 26/17 개수, 그리고 매크로 대상 모델 ID 목록이 audit와 fit 양쪽에 있습니다. | 프로브나 매니페스트를 한 번 고치면 도구들이 조용히 어긋납니다. 하모닉 칸 계산은 측정 창이 250 ms일 때만 맞습니다. | `compare_original_nam.py`, `fit_original_voicing.py`, `audit_nastrond_macros.py` |
| 5 | audit가 정확히 17개 상태만 받습니다. | `--states`로 만든 리포트(채널 리포트는 39개)를 읽지 못해서, 채널 결론은 손으로 정리됐습니다. | `audit_nastrond_macros.py` |
| 6 | 매니페스트를 검증하지 않습니다. role, scope, gain group, 비교 가능 여부가 들어 있지만 아무도 읽지 않고, 중복이나 경로 검사도 없습니다. | 대상 그룹이 매니페스트와 어긋나도 오류가 나지 않습니다. | `nastrond-nam-manifest.json` |
| 7 | 리포트에 소스 출처도, 상태 파일 해시도 없습니다. 렌더러 정보는 자유 텍스트입니다. | 리포트를 체크아웃이나 추가 상태 집합으로 추적할 수 없고, 지연값을 숫자로 읽을 수 없습니다. | `comparison.json` 최상위 |
| 8 | Original 상태를 하나씩 순서대로 렌더링하고 float64로 메모리에 들고 있습니다. | 상태 39개와 캡처 2개에 4분 20초가 걸렸습니다. 39 × 37초를 float64로 들면 약 550 MB입니다. | 실제 렌더러와 대체 NAM으로 측정 |
| 9 | 렌더러가 메시지 없이 종료 코드 2–8만 내고, `preset`과 `channel`을 같이 주면 조용히 channel을 적용합니다. | 실패해도 `stderr`가 비어 있고, 애매한 상태가 적힌 것과 다른 소리로 측정됩니다. | `Tests/RenderOriginalAmp.cpp` |
| 10 | 이 Python 도구들을 다루는 테스트나 CI job이 없습니다. | 위 문제들이 생겨도 잡을 수 없습니다. | `.github/workflows` |
| 11 | 추가 상태 이름이 작업 파일 이름(`comparison`, `probe`)과 겹칠 수 있습니다. | 상태가 프로브나 리포트 설정 파일을 덮어쓸 수 있습니다. | `compare_original_nam.py` |

## 재설계

```
Tools/nastrond_nam_tool.py        하나의 CLI: fetch | compare | gain-csv | audit | groups
Tools/nastrond_nam/
  protocol.py    프로브 구조; 모든 측정 창, 오프셋, 하모닉 칸을 여기서 계산
  references.py  매니페스트 검증; role/scope/gain에서 매크로 대상 그룹 도출
  analysis.py    레벨, THD, v1 스펙트럼 지표 + 극성 + 옥타브 밴드 지표
  renderers.py   내용 해시 기반 렌더 캐시 (v1 파일 이름과 키 유지)
  states.py      고정 v1 상태 집합; 검증되고 이름이 겹치지 않는 추가 상태
  compare.py     comparison.json (v1 키 그대로, 추가 항목은 아래)
  summary.py     리포트만으로 계산하는 gain CSV와 매크로 방향 audit
  fetch.py       해시 검증 다운로드 (동작은 같고 매니페스트를 검증)
```

`compare_original_nam.py`, `audit_nastrond_macros.py`, `fetch_nastrond_references.py`는
이제 얇은 호환용 진입점입니다. 그래서 이전 Náströnd 문서에 적힌 명령이 그대로 동작합니다.
`fit_original_voicing.py`는 그룹, 오프셋, 밴드를 패키지에서 가져옵니다.

### 리포트 추가 항목 (`analysis_version: 2`)

- 스펙트럼 비교마다 `polarity`(±1)와 `band_spectrum_rms_db`가 추가됩니다. 후자는 65–8000 Hz를
  42개 로그 밴드로 나눠 dB 차이의 RMS를 낸 값입니다. `log_spectrum_rms_db`는 v1 의미를 유지합니다.
- 최상위에 `source`(도구 커밋과 수정 여부), `states_sha256`, `analysis_version`이 추가됩니다.
  Original 상태마다 렌더러 출력에서 읽은 `renderer_info`(예: `latency`)가 추가됩니다.
- `protocol`은 v1과 바이트 단위로 같습니다. 그래서 새 리포트도 고정 baseline과 함께 audit할 수 있습니다.

### Audit

`nastrond_nam_tool.py audit`의 기본값은 v1 지표, 고정 상태, v1 기준값입니다. 기본값으로 실행하면
`nastrond-macro-direction-20261005/summary.json`을 키 하나하나 그대로 재현합니다. 추가된 옵션은 다음과 같습니다.

- `--metric band_spectrum_rms_db`: 옥타브 가중 방향 판정. 이 지표가 없는 리포트는 근사하지 않고 거부합니다.
- `--scope channel_surtr` (또는 다른 접두어): `--states`로 만든 리포트에서 채널 자신의 매크로 양 끝값을 audit합니다.
- `--min-improvement-db`, `--max-default-drift-db`: 이전에는 한 번의 작업에 맞춰 코드에 박혀 있던 값입니다.

## 검증

| 확인 항목 | 결과 |
|---|---|
| 고정 2026-10-05 baseline으로 `gain-csv` 실행 | `nastrond-nam-20261005/gain-summary.csv`와 바이트 단위로 동일 |
| 고정 baseline, voicing, macro-direction 리포트로 `audit` 실행 | 고정 `summary.json`의 모든 키 동일, PASS |
| 같은 실제 `ChimeraOriginalAmpRender`(채널 포함 39개 상태)와 대체 NAM 렌더러로 v1/v2 `compare` 비교 | v1 키와 값 전부 동일. 4분 20초 → worker 4개로 1분 23초, 캐시 재실행 11초 |
| 같은 대상 렌더 11개로 v1/v2 `fit_original_voicing.py` 비교 | 출력 바이트 단위로 동일 |
| `Tests/test_original_renderer.py` | PASS. 진단 메시지와 preset+channel 거부도 확인 |
| `Tools/test_nastrond_nam.py` (CI validation-contract job에 추가) | PASS |

초기 검토 환경에서는 api.tone3000.com 접근 제한으로 실제 캡처 26개를 받지 못했습니다. 아래 2026-10-25 독립 검증에서는 26개 모두 확보·해시 확인 후 실제 대역 지표 결과를 기록했습니다.

## 다음 측정

명령까지 포함한 전체 절차는
[NASTROND_NAM_BAND_MEASUREMENT.ko.md](NASTROND_NAM_BAND_MEASUREMENT.ko.md)에 있습니다.

1. 현재 소스로 비공개 NAM 폴더와 채널 상태 파일을 써서 `nastrond_nam_tool.py compare`를 실행합니다.
2. `--metric band_spectrum_rms_db`로 기본 scope와 각 `--scope channel_<이름>`을 audit합니다.
3. IMPACT와 BLOOM의 결론을 v1 지표와 비교합니다. 두 매크로의 효과는 주로 500 Hz 아래에 있는데,
   v1 지표는 이 대역을 작게 반영합니다.

이 단계들로도 기존 차단 사항은 해결되지 않습니다. Meshuggah 캡처에 캐비닛이 포함돼 있고,
Granophyre 출처가 확인되지 않았고, 입력 레벨 절대 보정이 없고, 오너 DI 청음은 아직 하지 않았습니다.

## 독립 검증 및 보강 (2026-10-25 KST)

실제 26개 캡처, 동일 입력 v1/v2 호환성, 채널별 실패, 합성 오차 시험과 감사 검증 보강은 [검증 보고서](NASTROND_NAM_VERIFICATION_20261025.ko.md)에 기록했습니다. 기존 동결 증거는 변경하지 않았습니다.
