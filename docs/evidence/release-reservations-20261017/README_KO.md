# 2026-10-17 KST 미공개 예약 정합성 수정

## 결론과 범위

PR #50 기준 소스 `78899a879395d83823f7bf4130c1be632d6be95a` 위의 별도 수정이다.
최신 공개는 `1.3.1-beta.2` (2026-10-13 16:30:29 KST), 공개 릴리스/태그는 각각 8개다.
10/16 beta.3 공개 목표는 미달이며 새 공개일은 미정이다.

- beta.3: `deferred`, 활성 테스트/후보/공개일 모두 `null`. 원래 10/14·15·16 날짜는 `missed_target`으로 보존.
- beta.1: `1.3.2-beta.1`의 10/23 조건부 예약 유지. beta.3 선공개 의무 없음.
- 상위 Chimera 버전이 먼저 공개되면 미공개 beta.3를 `cancelled`로 정리하며, 이후 낮은 버전 공개를 금지.
- 두 후보가 모두 지연되면 각 예약을 재검토한다. 자동 이월이나 새 날짜·만료일 승인은 없음.
- #68의 기존 1.3.2-beta.1 전용 만료일만 보존하며 beta.3에 승계하지 않음.
- 공개 승인·만료일 정책·VERSION·RELEASE_CHANNEL·제품 Source·기존 태그·바이너리·업데이트 피드 변경 없음.

## 검증한 경계

`release-calendar.json` schema 3은 원래 64주 체크포인트와 활성 예약을 분리한다.
일정 Markdown 행, catalog의 날짜/상태 미러, 중앙 로드맵의 상태 표 및 후보 기록을 함께 검증한다.
publisher는 기존 준비 전/공개 직전 검사 경로에서 수정된 공통 검증기를 사용한다.
실시간 조회와 특정 대상 버전 검사는 현재 한국 날짜를 사용하며, 오프라인 일정 감사는 기록된 검토일 기준이다.

회귀 검사는 지연·취소된 대상 선택, 지난 날짜의 재활성화, 명시적 재예약 시 미러 불일치,
상위 버전 뒤에 낮은 버전 배정, 상위 공개 후 취소 정리, 자정 경과 및 업로드 중 상위 버전 공개를 포함한다.
공개 직전 검사에서 실패하면 transport의 공개 전환에 도달하지 않는다.

## 실행 결과

로컬 Linux에서 기존 publisher workflow가 실행하는 일곱 suite와 일정 suite를 실행했다.
총 232개 중 **229 PASS / Windows 전용 3 SKIP / 0 FAIL**.
패키징 버전의 CMake 검사는 설치된 CMake 실행파일을 PATH에 제공해 실행했다.

| Suite | 통과 | 로컬 제외 |
|---|---:|---:|
| Calendar | 26 | 0 |
| Weekly publisher | 37 | 0 |
| Weekly publication approval | 29 | 0 |
| Publication evidence | 20 | 0 |
| Packaging version | 19 | 3 |
| Historical beta.131 publisher | 45 | 0 |
| Historical beta.131 approval | 23 | 0 |
| Release consolidator | 30 | 0 |

[원시 실행 로그](local-tests.log), [suite 종료 코드](local-results.json), [GitHub 조회 스냅샷](github-snapshot.json),
[스냅샷 기반 검증 결과](snapshot-validation.json)를 함께 보존한다.
스냅샷 기반 검사는 GitHub connector로 조회한 태그·릴리스 목록을 기존 `live_consumed_tags` 경로에 넣었다.
직접 네트워크 조회 `--live` 및 Windows/macOS/Linux 일정 계약은 Draft PR의 기존 Actions workflow에서 별도로 확인한다.
활성 예약 **82**, 지연 **1**, 취소 **0**, 기존 공개 **8**이며 `publication_approved=false`다.

## 제품 후보에 대한 판정 유지

#62 `5c858442`는 Draft, 만료일·공개 승인 미정이다. #68 `c2263e4c`도 Draft다.
#90 `428d4b98`의 EQ 편집은 세 OS 성공이나 macOS 튜너·CAB·제품 빌드,
Linux/macOS EQ 상태, 통합 gate는 실패다. 해당 check URL과 결과는 스냅샷에 있다.
이번 예약 검사 PASS는 이러한 제품 실패를 해소하거나 실제 DAW·청감·공개 수용을 승인하지 않는다.

현재 공개 이력, 10/23 이후 날짜, catalog의 `roadmap` 외 필드는 기준 소스와 동일함을 확인했다.
PR은 일정 PR #50을 대상으로 쌓으며 main이나 기존 개발 PR을 병합하지 않는다.
