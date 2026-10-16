# beta.3 Signature 측정값 불리언 거부

## 범위와 재사용 근거

- 기준: PR #62의 `5c858442d0ed8360703e0ce63241a567736608a9`.
- 재사용: PR #41의 `07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46`에서
  `finite()`의 `isinstance(x,(int,float))`를 `type(x) in (int,float)`로 바꾼 한 줄.
- 수정된 평가기 전체 파일은 PR #41의 평가기와 바이트 단위로 같다.
  PR #41의 다른 정책·복구 도구·수용 조건 연결은 가져오지 않았다.
- Python의 `bool`은 `int`의 하위 클래스이므로 기존 검사에서는 JSON `true`와
  `false`가 측정값으로 허용됐다. 실제 정수/실수만 유한성 검사로 전달한다.
- 계산식, 고정 reference, 가중치, H1–H4/S1–S4 조건, 점수/critical 임계값,
  정상 JSON 출력, CLI 종료 코드 계약은 유지한다. `dsp_valid`는 계속 불리언이다.

## 회귀 검사

`Tools/test_signature_calculator_contracts.py`는 PR #41의 13개 검사와 보고 형식을
그대로 재사용하고 5개 검사를 추가한다. 원래 `test_signature_benchmark.py`의
PASS/REVISE/INVALID 세 시나리오도 그대로 실행한다.

| 검사 범위 | 확인 내용 |
| --- | --- |
| 기존 13개 | 기준 shape, bias/RMSE/weight, common bias, tie/gap, hard/secondary/critical 경계, schema, JSON roundtrip |
| 24개 측정 위치 × 7개 비정상 값 | `true`, `false`, `null`, 문자열, NaN, +Infinity, -Infinity 각각 `ValueError` |
| 24개 측정 위치의 누락 | 정확한 `song.metric` 오류로 거부 |
| 24개 측정 위치 × 7개 정상 값 | 정수 0/1/-7, 실수 0.0/1.0/-3.25/12.5 허용 및 같은 실수값 결과와 일치 |
| 실제 파일/CLI 192개 비정상 입력 | 24개 위치 × 위 7개 값 + 누락; 종료 2, 측정 위치 오류, 새 결과 파일 미생성 |
| 실제 파일/CLI 3개 정상 제어 흐름 | PASS=0, REVISE=1, DSP INVALID=1 및 결과 JSON 일치 |

NaN/Infinity는 표준 JSON 값이 아니다. Python `json.loads`가 기본적으로 이 토큰을
읽을 수 있어 기존 유한성 검사가 실제 파일 경로에서도 계속 거부하는지 검증한다.

실행 명령:

```sh
python Tools/test_signature_benchmark.py
python Tools/test_signature_calculator_contracts.py
python Tools/test_release_gate.py
python Tools/test_release_consolidator.py
```

로컬 Linux / Python 3.12.14 결과:

- 수정 전 beta.3: 같은 18개 검사에서 불리언 관련 subtest **98개 실패**.
  기존 검사 2개 + 전체 위치 API 48개 + 전체 위치 CLI 48개가 결함을 검출했다.
- 한 줄 수정 후: **18/18 PASS**, 실패/오류/skip 없음. CLI 실행은 총 **195회**.
- 기존 Signature 세 시나리오 PASS. release-gate 절차 검사 PASS,
  release consolidator **30/30 PASS**.
- 원본 beta.3 평가기와 수정 평가기의 전체 JSON 직렬화 결과를 **431개** 입력에서
  비교했다: 원래 세 시나리오, 위 정상값 168개, 공통 bias -20/0/3/100의 4개,
  seed `20261016`의 `random.Random.uniform(-5,5)`를 각 측정값에 더한 256개.
  정책 순서와 `SONGS` 순서로 생성했다. 모두 바이트 단위 일치했다.
  결과 분포는 PASS 7 / REVISE 423 / INVALID 1이다.
- 두 평가기 출력의 순차 연결 SHA-256은 동일하다:
  `c593a3f20a8c7f6080c136f5693dcc56e87e02c4cf8e96db3166e353ba054ee7`.
  각 결과는 `json.dumps(result, indent=2, allow_nan=False) + '\n'`의 UTF-8이다.
- `Source/`, `Tests/`, `Installer/`, `Validation/`, `VERSION`, publisher 및 release
  gate/evidence 관련 보호 파일 **260개**가 기준 beta.3와 바이트 단위로 같다.
- Signature policy SHA-256 유지:
  `bc076ff811891cd168aa446c0333d8d29999b1c15f02a86c5bf1ad4f66c71f15`.

기존 Product의 Windows/macOS/Linux Python 검증 단계와 Windows Candidate 검증 단계에
새 계산기 검사 호출을 각각 한 줄 추가했다. 이 문서의 로컬 결과는 CI 전체 성공이나
새 바이너리 수용 결과가 아니다. 실제 실행 상태와 고정 head/run 링크는 PR에 기록한다.

## 별도 분석: 중복 JSON 키와 출처

### 중복 키

현재 `load()`는 기본 `json.loads`를 사용하므로 같은 객체의 중복 키는 마지막 값으로
덮어쓴다. 기준 fixture의 첫 `crest_db`를 다음과 같이 바꿔 실제 로더로 재현했다.

- `"crest_db": NaN, "crest_db": 9.69` → 이전 값이 사라져 계산기 PASS.
- `"crest_db": 9.69, "crest_db": NaN` → `Missing/non-finite crom.crest_db`.

이번 불리언 수정은 파싱 이후의 값만 검증하므로 덮어쓰기를 검출하지 않는다.
향후 검토 후보는 `object_pairs_hook`에서 중첩 객체를 포함한 중복 키를 거부하는 방식이다.
policy와 render 양쪽에 적용할지, 오류 계약 및 기존 입력 호환 범위를 먼저 결정해야 한다.
이는 **승인되지 않은 제안**이며 이번 PR은 로더나 수용 기준을 변경하지 않는다.

### 측정 출처

평가기 입력의 schema/version, DSP gate와 측정값을 통과한 수동 reference fixture도
PASS가 된다. 평가기는 DI/렌더 파일, source commit, policy/input SHA-256,
실행 환경, capture/reference 권한·출처를 자체 검증하지 않는다. 따라서 계산기 PASS만으로
실제 음원 측정, 청음 또는 최종 소스 수용 완료를 주장할 수 없다.

저장소의 `release_evidence.py`/consolidator는 별도의 소스·원시 증거 결합 검증을 갖지만,
그 존재가 이 계산기 입력의 출처를 보증하지는 않는다. 향후 증거 envelope의 필드,
해시 결합 대상, producer 소유권, source/run 검증 및 A-ID 연결은 별도 명세 검토 대상이다.
이번 PR은 새 필수 필드나 producer/receipt를 만들지 않는다.

## 승인 경계

- PR #82의 A11.03 연결은 제안 상태다. 이 수정으로 A11.03을 자동 PASS로 바꾸지 않는다.
- `policy_claims=[]`. 기존 107개 차단 항목에 대한 새 PASS·waiver는 **0**이다.
- release consolidator의 기존 fixture는 **21 PASS / 107 BLOCKED**를 유지한다.
  이는 로컬 fixture 결과이며 새 Product 실측 게이트 결과가 아니다.
- `expires_at_utc=null`, `expiry_approved=false`, `release_approved=false`,
  `RELEASE_REVIEW_PENDING`은 그대로다. 만료일·공개 승인·태그·배포물을 변경하지 않는다.
- PR #62 위의 별도 Draft로 검토하며 beta.3 브랜치나 main에 자동 병합하지 않는다.

후속: 이 문서의 18개 검사 결과는 PR #84 당시 기록이다. 승인된 기존 동작 연결에서는 3개 계산기 검사를 추가해 21개를 실행한다. [후속 적용 보고서](ACCEPTED_BASELINE_20261016.md)를 참조한다.
