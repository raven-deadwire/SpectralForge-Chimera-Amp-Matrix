# 테스터 코드 경로 검증 — 2026-11-01 KST

## 결론과 기준

기준 main: `0c8f850b0561792a1d7063665521f31cace86f1a`.
현재 main은 HMAC 방식이다. 형식이 같은 허용목록 방식은 main에 들어오지 않았다. 기존 초안 2건의 코드 5개는 문법상 유효하고 기존 승계 후보 5개와 모두 일치하지만, **운영 HMAC 비밀값에 대한 인증과 실제 공개 배포판의 수락은 미확인**이다.

이 변경은 main의 HMAC 경로를 보완한다. 신규 운영 코드/비밀값 발급, 키 교체, 승계 후보 활성화, 기존 PR 병합, 공개 릴리스 및 메일 변경·발송은 수행하지 않았다. 코드 원문·확인 문자·비밀값·메일 주소를 이 보고서와 저장소에 넣지 않는다.

## 경로별 대조

| 단계 | 확인 결과 | 증거/한계 |
|---|---|---|
| 소유자 발급 | `Tools/tester_code.py`가 이름 정규화 후 HMAC-SHA256 상위 30비트를 Crockford Base32 6자리로 인코딩 | 동일 이름+비밀값이면 동일 코드. 제품에도 공통 비밀값이 들어가는 기존 구조 유지 |
| 초안 | 개인 1개+팀 4개, 고유 코드 5개, 모두 CHT1 형식 | Gmail 읽기 전용 조회에서 두 건 모두 DRAFT. 팀 초안에는 수신인 없음. 운영 키를 읽거나 추출하지 않음 |
| 기존 승계 후보 | 5개 모두 PR #199의 후보 다이제스트와 일치 | `84b2cbdccd4eb5e84a91b0dc6eb43de7ab564488`, `docs/evidence/tester-migration-20261027/allowlist.candidate.json`. 메모리 내 SHA-256 대조, 원문 미출력. HMAC 인증 성공을 의미하지 않음 |
| 브랜치 관계 | #191은 #186 브랜치에 병합됐지만 main에는 없음. #199도 별도 스택 | #186 base=`claude/inspiring-shannon-lac28e`, #199 base=`codex/offline-tester-allowlist`. 이 PR은 main 대상 독립 수정이며 허용목록 채택은 별도 판단 |
| Actions 주입 | `build.yml`의 비PR 후보에서 비밀값을 받아 CMake 헤더 생성 | `prepare-release.yml`은 `secrets: inherit`. 원래 PR도 job 환경으로 비밀값을 받았음. 수정 후 PR에는 빈 값, 비PR 사전 검사/구성 단계에만 전달 |
| 실제 후보 실행 기록 | run `41222043832`, source `ac4c19026212cd4d027f3fdc0459f7e5a6ba42ed`, 2026-11-01 01:56 KST 시작 | workflow_dispatch, 세 OS 빌드·CTest·패키징 성공, 비밀값이 비었을 때만 실패하는 단계는 skipped. **당시 비어 있지 않았다는 추론**이며 현재 secret 설정 조회나 원문 검증은 아님 |
| 공개 버전 | 최신 플러그인 공개 태그는 `v1.3.2-beta.1`; 이후 프리셋 태그는 별도 | 해당 태그 `Source/PluginEditor.cpp`에 코드 입력 메뉴 없음. 공개 beta.2 확인되지 않음 |
| 인증/보존 | 제품은 코드 파일을 읽어 인증하고 인스턴스 수명주기에 반영 | 프로젝트/프리셋/A-B 직렬화에는 코드나 권한을 넣지 않음. 사용자 데이터 경로 유지 |

## 결함과 수정

- **삭제 실패 오표시:** 저장 파일을 지울 수 없어도 권한 표시를 지우고 성공 문구를 보여줬다. 삭제 결과를 반환하고 실패 시 기존 표시를 보존한다.
- **다른 창의 변경 누락:** RECHECK CLOCK은 최초 유효 코드 추가만 반영했다. 교체된 이름과 제거도 반영하며 이미 허용된 오디오는 유지한다.
- **평문 저장 권한/특수 파일:** 기본 생성 권한으로 노출될 수 있었다. POSIX는 처음부터 0600 임시 파일을 생성해 원자 교체하며, 제한된 읽기와 최종 경로의 symlink/FIFO/디렉터리 차단을 추가한다. Windows는 기존 사용자 디렉터리 ACL을 상속하므로 실제 ACL 검증은 별도다.
- **Python/C++ 입력 차이:** Python은 Unicode 공백을 제거해 네이티브와 다르게 판정했다. ASCII 경계·붙여넣기 상한·확인 문자 검사를 통일하고 짧거나 비어 있는 키의 발급을 거부한다.
- **홀수 길이 비밀값:** 기존 CMake는 33자리 hex도 받아들였다. 완전한 바이트 단위·16바이트 이상·hex 형식을 검증한다. 비PR 후보의 누락은 CMake에서도 차단한다.
- **발급 파일 관리:** 기본 비밀값 stdout 출력을 없애고 명시적 `--show-secret`만 허용한다. Git 내부 생성과 기존 파일/링크 덮어쓰기를 거부하며 POSIX 최초 권한을 0600으로 한다.
- **회귀 공백:** 실제 CMake 함수가 만든 enabled/disabled 헤더로 제품 인증 함수를 각각 컴파일한다. Python/C++ 교차 입력, 저장/삭제와 프로세서 직렬화 검사를 CI에 포함한다.

저장소의 공개 시험용 fixture만 사용했다. #186의 기존 HMAC 보완안 `4fb68caf97170cb63b4237d09ff3fea8d992053e`에서 관련 저장·발급 테스트를 재사용하고 최신 main 프로세서에 한정 적용했다. 오래된 DSP/파라미터 변경은 가져오지 않았다.

## 검증 기록

- 수정 전 기본 Python tester 6/6, lifecycle 12/12 통과. 별도 재현에서 Unicode NBSP 입력 수락, 빈 키 발급, CMake 홀수 길이 키 수락 확인.
- 수정 후 Python tester 9개(네이티브 연동은 CTest에서 실행), CMake/workflow 4개, lifecycle 12개.
- 릴리스 게시 도구 51/51, 프리셋 유지보수 23/23, 패키징 27 통과/Windows 전용 3 skip.
- release-scope, CTest evidence, CI producer-map 검사 통과. 기존 release verdict는 BLOCKED 유지.
- 네이티브 빌드·CTest 결과는 아래 최종 검증 기록에 명시한다.

## 실제 배포 확인이 남은 항목

1. 운영 발급 비밀값과 각 OS 후보 빌드의 비밀값 일치, 실제 코드 5개 입력 수락.
2. 승인된 정확한 Windows/macOS/Linux 패키지의 새 설치·업그레이드 후 인증 유지 및 파일 ACL.
3. 실제 DAW/OBS에서 코드 입력, 여러 창 교체·삭제, 프로젝트/A-B/개인 IR·프리셋 복원.
4. HMAC 공통 비밀값의 바이너리 포함 및 30비트 코드의 한계를 받아들일지, 별도 허용목록 스택을 채택할지 결정.

자동 시험값으로 확인한 코드 경로를 실제 사용자 코드/키/호스트의 배포 승인으로 확대하지 않는다. 이 문서는 초안 발송의 근거가 아니며 기존 초안은 보류 상태로 유지한다.

근거: [최근 후보 실행](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41222043832), [공개 beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.2-beta.1), [#186](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/186), [#199](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/199).
