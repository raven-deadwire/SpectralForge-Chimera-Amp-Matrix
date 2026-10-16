# Signature JSON·CLI 회귀 보강 — A11.06 / A11.07

## 범위와 기준

- 기준 PR: #89, `d27e0f549e1115d4ae7d918af0e7ea26caa1584d`.
- PR89에서 재검증한 제품 SHA는 `72ecf35fa90293418da8635225435095dbca1135`이다. 이번 변경 후보의 검증과 구분한다.
- 사용자 요청: 중복 JSON 키·비표준 상수·손상 입력·오류 후 이전 결과 처리 보강. 정상 계산·점수·임계값 유지. 별도 Draft PR과 실행 근거 제공.
- PR84의 `finite()` 불리언 거부와 기존 21개 계산기 검사를 재사용한다.

## 구현

1. 정책과 render 파일 모두에서 중복 키를 깊이와 관계없이 거부한다. 같은 값의 중복 및 escape 해제 후 같은 키도 거부한다. 서로 다른 객체에서 같은 키를 사용하는 정상 JSON은 허용한다.
2. `NaN`, `Infinity`, `-Infinity` 토큰은 사용하지 않는 필드에 있어도 로딩 단계에서 거부한다. UTF-8 BOM·문자열·정상 숫자·기존 스키마 처리는 유지한다.
3. 인자가 정상 파싱된 실행은 입력을 읽기 전에 지정 출력의 이전 파일을 제거한다. 정상 결과를 같은 디렉터리의 임시 파일에 완전히 기록한 후 교체한다. 직렬화는 `allow_nan=False`이다.
4. 예외 발생 시 현재 출력과 임시 파일을 제거하고 기존 종료 코드 2를 반환한다. 삭제 불가능한 파일은 별도 오류를 출력한다. 권한/파일시스템 오류를 무시하거나 성공으로 처리하지 않는다.
5. 출력이 입력 파일 자체 또는 동일 파일의 별칭이면 종료 2로 거부하고 입력을 보존한다. PASS=0, REVISE/INVALID=1과 그 결과 파일은 유지한다.

점수 함수 `evaluate()`의 AST와 `Validation/signature-benchmark-policy.json` 전체 바이트는 기준 소스와 같다. 점수 가중치, hard/secondary gate, 판정 기준, provenance 정책, 공개/만료 승인 정책을 변경하지 않는다.

## 실제 로컬 결과

Python 3.12.14 / Linux에서 실행했다. `local-after.json`은 수정 작업 트리의 개별 소스 SHA-256을 기록한다. `source_commit`은 부모 커밋이고 `worktree_dirty=true`이며, 커밋된 최종 소스의 CI 영수증과 혼동하지 않는다.

| 검증 | 결과 | 근거 |
|---|---|---|
| 최초 회귀 suite / 수정 전 | 9 tests, 47 failing assertions, errors/skips 0 | `local-validation.json`; 최초 재현 기록 |
| JSON·CLI suite / 수정 후 | 9/9 PASS, 실제 CLI subprocess 68회 | `local-after.json`, `local-after.log` |
| 기존 계산기 suite | 21/21 PASS | `local-calculator.json` |
| 기존 Signature 시나리오 | PASS/REVISE/INVALID 3개 PASS | `local-validation.json` |
| 기존 소스와 정상 결과 비교 | 432/432 전체 JSON byte-identical: PASS 8 / REVISE 423 / INVALID 1 | `local-parity.json`, `compare_normal_results.py` |
| release gate/scope 계약 | PASS | `local-validation.json` |
| release consolidator 보호 검사 | 31/31 PASS | `local-validation.json` |

새 suite는 양쪽 입력의 중복/비표준 토큰·잘린 JSON·빈 파일·trailing data·trailing comma·control character·잘못된 UTF-8, 누락 파일·잘못된 schema/구조를 실제 subprocess에서 검사한다. 각 실패 직전에 실제 PASS 파일을 생성하여 이전 결과 제거를 확인한다. 직렬화·부분 쓰기·교체·stdout 오류와 삭제 권한 실패는 결정적 fault injection으로 구분한다. 이 fault injection은 실제 디스크 고장이나 실제 Windows 파일 잠금 측정이 아니다.

## 수용 기준에 연결할 수 있는 범위

| ID | 이번 근거 | 판정 경계 |
|---|---|---|
| A11.06 | 깊은 중복 키·비표준 상수 거부, 정상 JSON/BOM 유지, allow_nan=False 출력 roundtrip | 해당 자동 계약은 로컬 실행 PASS. 최종 소스의 OS별 CI는 Actions 원시 로그·artifact로 별도 확인한다. |
| A11.07 | 실제 0/1/2 종료, 실패 전 PASS 생성→실패 후 출력 부재, 성공 후 현재 결과 교체, source/input/output/log digest와 run/attempt 기록 | 실행된 synthetic CLI 계약만 지원한다. 실제 측정 producer/consumer의 digest 검증 및 reviewer 승인을 발급하지 않는다. |

`--help`와 argparse 사용법 오류는 기존 argparse 동작이며 출력 소유권을 얻기 전 종료한다. 출력 경로가 지정되지 않았거나 입력과 충돌한 호출에는 파일을 삭제하지 않는다. 권한으로 삭제할 수 없는 출력은 남을 수 있으므로 소비자는 종료 코드 2의 파일을 사용하면 안 된다. 동일 출력 경로를 공유하는 동시 실행·외부 강제 종료에 대한 새 보장은 도입하지 않는다.

`signature-json-cli.yml`은 Windows 2022, macOS 14, Ubuntu 24.04에서 기존/신규 검사와 432개 정상 결과 비교를 실행하고 로그/JSON을 업로드한다. 기존 Product/Windows Candidate 계산기 단계에도 신규 suite를 추가한다. CI 결과는 PR 본문에서 실제 source SHA/run/job에 연결한다.

첫 CI run `38913899883`의 macOS job `117016330307`은 기존 21개·신규 9개·432개 동일성 비교까지 통과했다. Windows job `117016330208`도 21개·9개는 통과했으나 비교 도구가 체크아웃의 CRLF 변환을 정책 변경으로 판단해 실패했다. 후속 수정은 committed policy blob을 기준 소스와 바이트 단위 비교하고, 실행 파일에는 LF/CRLF 변환만 허용하여 확인한다. 정책 내용·평가기·통과 기준은 바꾸지 않았다. 실패 이력을 유지하고 후속 CI를 별도로 확인한다.

## 승인 및 증거 보존

- `policy_claims=[]`, `review_only=true`, `publisher_input=false`, `release_approved=false`, `expiry_approved=false`.
- 기존 88 PASS / 40 BLOCKED는 PR89의 고정 제품에 대한 검토 기록이다. 이번 변경으로 90/38 또는 다른 전체 판정을 자동 산출하지 않는다.
- 평가기/검사 변경으로 엄격한 지문은 달라진다. PR89의 다른 39개 증거를 새 소스에 자동 승계하거나 기존 지문을 완화하지 않는다.
- `remaining-40.csv`, `Validation/accepted-baseline.json`, 공식 gate/publisher, waiver, 승인 원장은 수정하지 않는다.
- A11.08/A11.09, 실제 DI·오디오·DAW·청음·패키지 수용 및 beta.3 공개 승인은 별도 과제로 남는다.

재현:

```sh
python Tools/test_signature_benchmark.py
python Tools/test_signature_calculator_contracts.py --report build/calculator.json
python Tools/test_signature_json_cli.py --report build/json-cli.json
python docs/evidence/signature-json-cli-20261016/compare_normal_results.py --output build/parity.json
```
