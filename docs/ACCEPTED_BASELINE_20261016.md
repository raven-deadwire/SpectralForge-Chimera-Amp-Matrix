# 기존 동작 기준 86개 검사항목 적용 — 2026-10-16 KST

사용자 지시 **“이건 그동안 맞았으니까 처리해”**를 기존 동작·현재 모델링 CAB 구조의 수용 기준 적용 승인으로 기록했다. 미실행 검사, 새 프로토콜, 만료일 또는 beta.3 공개를 승인받은 것으로 확장하지 않았다.

## 처리 결과

- 86개 항목을 각각 정의·증거·담당 검증 코드 또는 구체적인 미충족 사유에 연결했다.
- 기존 동작 81개와 앞서 승인된 bool 수정 1개는 적용 대상이다. 중복 키/엄격 파싱·stale output·측정/실행 provenance의 새 프로토콜 4개는 별도 과제로 유지했다.
- 현재 로컬 component 판정: **61 PASS / 25 BLOCKED**. 이 결과는 새 native 바이너리 또는 실 DAW 수용 완료가 아니다.
- 재사용 가능한 37개는 원래 5c858442 제품·38386305674 run 및 38777238017 upgrade run을 그대로 표시한다. 제품·시험·자산·설치 관련 **612개 파일**이 이전 검증본과 같을 때만 기존 동작의 회귀 근거로 인정한다. 새로운 Source/Assets/Tests/Installer 파일도 재사용을 차단한다.
- 나머지 PASS는 현재 소스의 계산기 계약 20개, IR 배포 규칙 2개, 후보 버전/검증 상태 문서 2개다.
- 기존 자동 producer 21개가 성공한 통합 fixture에서 **82 PASS / 46 BLOCKED**를 재계산했다. 실제 새 Product 게이트 결과는 해당 CI가 완료되기 전에는 주장하지 않는다.
- 실제 native/package 검사 21개를 기존 증거로 덮어쓰지 않는다. 음원·기준 13개, 실사용 7개, 종합 승인 1개도 그대로 유지한다.

## 구현과 재검증

`Validation/accepted-baseline.json`은 원본 PR #82 제안 문서를 수정하지 않는 승인 후속 원장이다. 원래 번호 연결이 복구됐다고 주장하지 않으며, registry SHA-256으로 적용 정의를 고정한다.

`Tools/accepted_baseline.py`는 원본 archive/hash/JUnit/전체 로그와 source identity를 재검증하고 현재 component 검사를 실행한다. Product workflow의 validation-contract job에서 같은 commit/run/attempt에 묶인 artifact를 생성하고, consolidator가 독립적으로 원시 결과와 판정을 대조한다. 다른 run, 변조된 결과, 실패·누락·중복·skip 사례, 바뀐 관련 소스는 통과 근거로 사용할 수 없다.

새 artifact가 없으면 기존 게이트의 미실행 처리 방식이 유지된다. 기존 beta.2의 고정 publication 정책·receipt를 소급해서 변경하지 않기 위한 호환 계약이다. 기존 원본 21/107 보고서는 그대로 보존한다.

PR #84의 bool 거부와 18개 검사를 재사용했고, 전체 metric 산술/clamp·정확한 tie/empty pair·전체 결과 필드/schema CLI 검사 3개를 보강해 **계산기 21/21 PASS**를 확인했다. 기본 점수식·가중치·기준 수치·임계값은 변경하지 않았다.

새 evidence 경계 **17/17**, 기존 consolidator **31/31**, release-gate 절차 검사 PASS. publication-evidence 20/20, weekly-publication 29/29, beta.3 publisher 45/45도 통과했다. 이는 단위/회귀 검사 결과이며 사용자 청음·DAW 결과가 아니다.

## 남은 25개

기존의 “무엇을 검사할지 모름” 대신 아래 작업을 명시했다. 사용자에게 이미 승인한 기존 동작을 다시 승인해 달라고 요구하지 않는다.

| ID | 항목 | 남은 근거/작업 |
|---|---|---|
| A2.07 | 공장 CAB 독립성 | 기존 modeled-CAB/empty-target 증거 재사용. 과거 파일명 유무를 비교하는 직접 negative fixture는 추가 연결 필요. |
| A3.03 | binary state 복원 | Guitar4 및 lifecycle 사례 재사용; Bass3 개별 저장 상태를 수용 증거에 연결한다. |
| A3.04 | A/B 비교 슬롯 | 기존 A/B 및 expiry processor 증거 재사용; 각 Bass Signature의 구체 슬롯 값 목록을 연결한다. |
| A5.01 | 공장 모델링 CAB 계약 | recipe/empty-target 기존 증거를 재사용. 과거 파일명 파일의 존재/변조를 넣은 직접 비교 사례는 추가 연결한다. |
| A5.02 | 개인 IR 명시적 선택 | 기존 WAV/UI 및 installed companion 시험은 부분 증거. AIFF 및 각 악기 유형·경로의 실행 사례를 추가 연결한다. |
| A5.05 | 유효하지 않은 IR 거부 | corrupt/silent은 실행됨. 모든 경계·NaN/Inf의 직접 negative fixture coverage는 추가 확인/연결이 필요하다. |
| A5.06 | 개인·embedded IR 상태 호환 | 기존 lifecycle/IR isolation 증거는 부분 재사용. Hartke/Marshall 실물 개인 파일 및 실제 DAW 프로젝트의 bit-identical fixture가 없으므로 호환 완료로 주장하지 않는다. |
| A5.08 | 배포·업그레이드 경계 | 분포 allowlist·설치 파일 보존 증거 재사용. private 캡처 청음·권리·정품 hash 인증은 별도 입력이 없으면 BLOCKED. |
| A6.01 | 공장 모델링 CAB 계약 | recipe/empty-target 기존 증거를 재사용. 과거 파일명 파일의 존재/변조를 넣은 직접 비교 사례는 추가 연결한다. |
| A6.02 | 개인 IR 명시적 선택 | 기존 WAV/UI 및 installed companion 시험은 부분 증거. AIFF 및 각 악기 유형·경로의 실행 사례를 추가 연결한다. |
| A6.05 | 유효하지 않은 IR 거부 | corrupt/silent은 실행됨. 모든 경계·NaN/Inf의 직접 negative fixture coverage는 추가 확인/연결이 필요하다. |
| A6.06 | 개인·embedded IR 상태 호환 | 기존 lifecycle/IR isolation 증거는 부분 재사용. Hartke/Marshall 실물 개인 파일 및 실제 DAW 프로젝트의 bit-identical fixture가 없으므로 호환 완료로 주장하지 않는다. |
| A6.07 | 라이브러리 제거·재등록 | 파일 보존/재등록 실행 결과를 재사용. 저장 상태와 결합한 후속 열기 사례는 별도 연결한다. |
| A6.08 | 배포·업그레이드 경계 | 분포 allowlist·설치 파일 보존 증거 재사용. private 캡처 청음·권리·정품 hash 인증은 별도 입력이 없으면 BLOCKED. |
| A8.02 | 깨진 개인 파일은 사용 불가 | invalid readiness는 기존 증거. 실패 후 마지막 정상 오디오 보존의 직접 assertion은 추가 필요. |
| A8.03 | 외부 경로 누락 시 embedded 복원 | embedded-only 복원은 기존 증거. 양쪽 모두 무효인 정책·UI 결과를 명시하고 negative fixture를 추가 연결해야 한다. |
| A11.06 | JSON roundtrip·엄격 파싱 | PR41 output roundtrip은 재사용. 현재 load는 일반 json.loads이므로 duplicate/nonstandard constant 정책 보강 필요. |
| A11.07 | CLI 종료 코드·실패 산출물 | 원래 exit contract는 코드에서 복구. stale-output 방지 producer와 CLI subprocess negative receipt 추가 필요. |
| A11.08 | 입력·정책·render provenance | 현재 evaluator JSON 자체는 provenance를 보증하지 않는다. 실제 외부 입력·측정 producer가 필요하다. |
| A11.09 | 실행·review provenance | PR41은 component receipt 예시일 뿐 release receipt가 아니다. 실제 acceptance producer/검토자는 별도 승인·연결이 필요하다. |
| A14.02 | EN/DE/KO 설명 동등성 | 파일 존재/embedded packaging 증거는 부분 지원. 3개 언어 전체 의미 검토 receipt가 없다. |
| A14.03 | 카탈로그·프리셋 일치 | catalog contract/public naming의 source 및 native 일부 증거 재사용. 문서 전체 의미 일치는 별도 검토. |
| A14.04 | CAB·IR 정책 설명 | docs/IR_DISTRIBUTION 및 EXTERNAL_BASS_IRS의 현재 설명을 기준으로 3개 언어·체크리스트를 대조. |
| A14.06 | 패키지 문서 일치·개인 주석 보존 | 기존 assembly 및 PR78 Windows receipt 재사용. macOS/Linux 실제 출시 패키지 설치 보존은 fixture와 구분하여 연결한다. |
| A14.07 | 출시 제한·사용 안내 | 현재 설명·패키지 metadata와 비교한 reviewer receipt가 필요하다. 공개 일정은 승인으로 사용하지 않는다. |

## 실행과 승인 경계

```sh
python Tools/test_accepted_baseline.py
python Tools/test_release_consolidator.py
python Tools/accepted_baseline.py --commit <HEAD> --run-id <RUN> --run-attempt <ATTEMPT> --output build/accepted-baseline
```

실제 evidence 생성은 깨끗한 고정 소스를 요구한다. `--diagnostic` 결과는 소비자가 거부하므로 로컬 작업 중의 결과를 CI receipt로 승계할 수 없다. 위 통합 수치는 controlled fixture이며 공개 승인이 아니다.

`release-policy.json`, `ci-check-map.json`, 원래 hard-gate membership, waivers, Signature 고정 policy, beta.2 publication 정책, beta.3 lifecycle policy 및 `RELEASE_REVIEW_PENDING`은 변경하지 않았다. beta.3 `expires_at_utc=null`, `expiry_approved=false`, `release_approved=false`를 유지한다. 자동 병합·공개·태그 변경은 수행하지 않는다.
