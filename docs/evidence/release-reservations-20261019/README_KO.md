# 2026-10-19 — 조기 공개 후 예약·버전 정합성

기존 Draft #93의 후속 수정이다. #50의 일정/검증 변경을 보존하고 최신 main `ec6fd639`를 통합해 main 대상으로 직접 검토한다. publisher import 충돌 한 곳은 `check_release_calendar`와 최신 `beta_lifecycle`를 모두 유지해 해결했다. #50은 이 PR로 통합 대체한다.
실제 공개 이력: #95 통합, `v1.3.2-beta.1`, 제품 소스
`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, 2026-10-18 01:51:47 KST.
태그와 공개 릴리스는 각각 9개이고 beta.1 공개 자산은 30개다.
공개 사실은 strict acceptance PASS가 아니며 공식 공개의 미검증 범위를 유지한다.

- beta.1: published receipt와 `completed_reservations`로 보존. 원래 10/21·22·23 목표는 `original_target`, 활성 날짜는 null. 활성 주간 행 재삽입 및 publisher 선택 거부.
- 미공개 1.3.1-beta.3: 상위 버전 선공개 정책으로 cancelled. 원래 10/14·15·16 목표, 지연 사유, 취소일·대체 공개 태그 보존. 미래 날짜로 재활성화해도 하위 버전 차단.
- 다음 후보 beta.2: 10/21 테스트·10/22 후보·10/23 개발 및 조건부 공개 검토. 이슈 #97 네 P0와 시스템 요구사항/실제 환경표. 만료일·공개 승인 미정, 소스·후보 PR 미배정.
- 10/30은 beta.3, 11월은 beta.4–beta.7로 미사용 번호 재배정. 원래 64개 금요일과 1.3.3 이후 날짜/번호 유지.
- #68/#90은 과거 후보 기록이며 새 소스의 검증·공개 승인이 아님. AAX는 과거 11/27 후속 검토 대신 #97에 따라 다음 업데이트 필수. 서명·일반 Pro Tools 미완료를 일정으로 충족시키지 않음.

## 검증

8개 suite 총 **239 PASS / Windows 전용 3 SKIP / 0 FAIL**.
Calendar 33, publisher 40, weekly approval 29, evidence 20, packaging 19(+3 skip),
historical publisher 45, historical approval 23, consolidator 30.
CMake 미설치로 패키징 한 검사가 최초 실행에서 오류였으며, CMake 4.4.4를 임시 PATH에 제공한 재실행에서 통과했다.
원시 로그와 최종 suite 결과는 이 디렉터리에 보존한다.

공개된 태그 재예약, 취소 버전의 원래/미래 날짜 재활성화, early-publication receipt 날짜/소스 위조,
다음 버전·미러 불일치, 만료일/공개 승인 승계, 업로드 중 태그 소비, KST 자정 경과,
lookup 실패 시 차단과 기존 중복 태그 검사를 검증한다. 제품 바이너리·DAW 검사 결과로 확대 해석하지 않는다.

GitHub connector로 읽은 전체 9개 태그·9개 릴리스 스냅샷을 기존 live inventory 경로에 입력해
**82 active / 0 deferred / 1 cancelled / 9 published**, beta.2 예약 정합성 통과를 확인했다.
직접 live 네트워크 검사 및 Windows/macOS/Linux calendar·publisher 계약은 기존 Actions에서 별도로 확인한다.

현재 main 대비 제품 Source, VERSION/RELEASE_CHANNEL, transport, 베타 수명주기 검사를 보존했다.
#50/#93에서 추가된 publisher 준비 전·공개 직전 예약 검사도 유지했다.
승인 정책, 공개 태그·바이너리·업데이트 피드는 변경하지 않았다.
catalog JSON의 roadmap 외 런타임 필드도 원본과 동일하다.
이번 변경은 일정/검증 수정이며 실제 새 제품 배포나 main으로의 병합을 수행하지 않는다.
main을 Draft 소스에 동기화한 뒤 동일한 8개 suite를 재실행한 최종 로그를 보존한다.
