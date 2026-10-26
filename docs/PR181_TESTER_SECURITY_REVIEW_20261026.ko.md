# PR #181 테스터 코드 보안·운영 검토 — 2026-10-26 KST

## 판정과 검토 기준

**현재 짧은 코드 구현은 배포 보류를 권고한다.** 무기한·후속 베타 권한을 가진 코드를 무단 발급할 수 있는 구조이며, 저장·삭제 보완만으로 해결되지 않는다. 이 검토와 수정은 배포 승인 또는 권한 범위 변경이 아니다.

검토를 시작한 소스는 `272f72aeeaa0c1e128d53688e8d061413481a9a4`(RSA-2048)였다. 작업 중 PR에 두 커밋이 추가됐으므로 최종 기준은 **`c150db48dbd92221723667c06426f12b7220641b`**이다. 후자는 `CHT1-NAME-CHECK` 6자리 HMAC 코드로 바뀌었으며 RSA 공개키 파일·코드 ID·발급일 필드가 제거됐다. 사용자에게 제공된 RSA 설명은 이전 커밋의 설명이다. 초기에 작성한 RSA 전용 수정은 최신 PR에 덮어쓰지 않았다.

- 원 PR: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/181
- 최신 검증기: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/c150db48dbd92221723667c06426f12b7220641b/Source/TesterCode.h
- 이전 RSA 검증기: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/272f72aeeaa0c1e128d53688e8d061413481a9a4/Source/TesterCode.h

## 발견 사항

| 우선순위 | 발견 | 영향·처리 |
|---|---|---|
| P1 / 배포 차단 권고 | HMAC 발급 비밀값을 검증하는 모든 제품 바이너리에 넣음 | 소스에서 `CHIMERA_TESTER_SECRET_HEX`를 문자열로 읽는다. 바이너리에서 비밀값을 얻으면 발급되지 않은 이름의 정상 코드를 만들 수 있다. 아래 시험용 재현으로 확인. 이번 수정에서 알고리즘은 교체하지 않았음. |
| P1 / 배포 차단 권고 | 확인 문자가 30비트이고 오프라인 검증 횟수 제한이 없음 | 한 이름의 후보는 2^30 = 1,073,741,824개. 무작위 1회 성공 확률은 1/2^30이며, 고정 후보 공간을 중복 없이 탐색하면 평균 약 2^29회. 공격 소요 시간은 측정하지 않았음. 비밀값 추출에는 이 탐색조차 필요 없음. |
| P1 | 운영 비밀값을 `build.yml`의 PR 작업 환경에도 무조건 전달 | 동일 저장소 PR은 secrets를 받을 수 있는 경로였다. PR 이벤트에서는 빈 값을 사용하도록 수정. fork PR의 기본 비밀값 제한만으로 동일 저장소 PR은 보호되지 않음. 신뢰할 수 있는 릴리스 실행에는 기존 정책대로 전달. GitHub secret 전달 규칙: https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets . |
| P2 | 저장 파일에 소유자 전용 권한을 지정하지 않음 | JUCE 8.0.8의 POSIX 쓰기는 기본 0644를 요청한다. 보통 umask 022에서는 다른 계정이 경로에 접근할 때 복사할 수 있다. 수정은 0600 임시 파일에 처음부터 기록하고 원자적으로 교체. Windows ACL은 별도 검증 필요. |
| P2 | `removeTesterCode()`가 파일 삭제 실패를 무시 | 파일이 남아도 UI가 성공을 표시하고 다음 인스턴스는 다시 권한을 읽을 수 있다. 실패를 반환하고 UI·현재 권한 표시를 유지하도록 수정. |
| P2 | RECHECK CLOCK이 추가만 반영 | 다른 창에서 제거·교체해도 기존 이름과 연장 표시가 남음. 최신 저장 상태를 양방향으로 반영하되 연주 중 출력은 유지하도록 수정. |
| P2 | 비밀 파일을 쓰고 난 뒤 chmod, 기본 stdout에 비밀값 출력 | 생성 순간의 노출·경로 경쟁·로그 유출 위험. 독점 생성, POSIX 0600, Git checkout 내부 생성 거부, 기본 출력 비공개를 적용. `--show-secret`은 명시적 대화형 초기 등록용. |
| P2 | 입력 제한·Python 오류 처리가 불균일 | 거대한 공백 입력, 비ASCII 확인 문자, 짧은 secret, 잘못된 JSON 필드 및 홀수 길이 hex에 대해 거부 경로를 보완. POSIX 파일 읽기는 크기를 제한하고 FIFO·심볼릭 링크를 거부해 호스트 생성 중 대기를 피함. |
| P2 | 문서에 beta.1 정책을 유지한다는 오래된 문장이 남음 | beta.2 승인 표와 모순. 버전별 날짜와 정확한 만료 경계를 명시하도록 수정. |

우선순위는 이 제품의 무단 베타 권한 발급·테스터 데이터 노출·배포 운영 영향 기준이다. 원격 코드 실행이나 시스템 관리자 권한 탈취가 확인됐다는 뜻은 아니다.

## 서명 위조·복사·재사용

이전 RSA 구현은 원문 payload 전체의 해시를 RSA-2048 공개키로 검증했고, 원래 테스트에서 payload 변조·서명 변조·다른 키·서명 이동·비정규 base64를 거부했다. 단, 자체 FDH 인코딩·키 생성 구현은 별도 암호 검토가 필요하며 몇 개의 회귀 테스트가 암호학적 안전성 증명은 아니다.

최신 HMAC 구현은 비대칭 서명이 아니다. 발급과 검증에 같은 비밀값을 쓰며 그 값을 고객 프로그램에 포함한다. HMAC 자체의 암호학적 결함을 발견한 것이 아니라 **비밀을 유지할 수 없는 위치에 발급 권한을 배포한 설계 문제**다.

재현은 운영 비밀값 없이 고정 시험용 비밀값과 실제 `Source/TesterCode.h`의 검증 함수만 사용했다. Linux g++ C++20, `-O3 -s`로 만든 최소 검증 프로그램에서 hex 문자열을 추출하고, 그 값으로 발급 이력이 없는 `AUDITNEW`의 코드를 계산했다. 검증기가 ACCEPT했다. 추출 후보 1개, 임의 이름 코드 수락 true. 실제 공개 릴리스 바이너리나 운영 secret은 읽거나 공격하지 않았다. 이 재현은 배포 소스의 구조적 위험을 확인하는 것이며 실제 운영 키가 이미 유출됐다는 주장이 아니다.

정상 코드와 저장 파일은 다른 사용자·컴퓨터에서 그대로 재사용된다. 기기 ID, 계정 인증, 만료 필드, 베타 버전 범위, 사용 횟수, 취소 목록이 없다. 이름은 신원 증명이 아니며, 최신 코드는 이름 정규화 때문에 `Seo Mary`와 `SEOMARY`가 같은 코드를 얻는다. 코드별 ID·발급일도 없어 같은 이름의 재발급을 구분할 수 없다.

## 저장 위치와 비밀 관리

JUCE 8.0.8 `userApplicationDataDirectory` 기준 실제 경로:

| OS | 코드 저장 경로 | 수정 후 보장과 한계 |
|---|---|---|
| Windows | `%APPDATA%\SpectralForge\Chimera\beta-tester-code.txt` | 부모 사용자 디렉터리 ACL 상속. 별도 계정으로 읽기/쓰기/삭제 차단 검증 필요. Roaming profile이면 다른 PC에 이동할 수 있음. |
| macOS | `~/Library/SpectralForge/Chimera/beta-tester-code.txt` | POSIX 0600 생성·교체 코드 적용. 실제 macOS 실행 확인은 미완료. |
| Linux | `$XDG_CONFIG_HOME/SpectralForge/Chimera/beta-tester-code.txt`, 기본 `~/.config/...` | 0600 생성·교체, 읽기 상한, 최종 심볼릭 링크/FIFO 거부를 Linux에서 검사. |

이는 사용자 영역 저장이며 기기 귀속이 아니다. 관리자·동일 계정 프로세스·사용자의 직접 전달·백업·동기화는 막지 않는다. 파일에는 재사용 가능한 코드가 평문으로 들어간다. 프로젝트·프리셋 직렬화와 분리되어 있다.

현재 추적 파일의 `.json/.pem/.key`를 점검했으며 테스터 private-key/secret 스키마 또는 PEM 개인키는 발견되지 않았다. 테스트용 비밀값은 별도 fixture다. Git 전체 과거 이력·이미 배포된 아티팩트·외부 백업·로그에 비밀값이 없음을 입증한 검사는 아니다. `.gitignore`와 현재-tree 검사만으로 강제 추가, 다른 확장자, 로그 유출까지 막을 수 없다.

GitHub Secret은 빌드 시 평문 생성 헤더와 바이너리에 들어간다. GitHub에서 값을 비공개로 관리하거나 로그를 마스킹해도 최종 HMAC 바이너리의 문제는 해결되지 않는다. Windows 비밀 파일은 Python mode 0600만으로 POSIX 수준 ACL을 보장할 수 없으므로 사용자 전용 폴더 ACL을 별도로 확인해야 한다.

## 키 교체·취소·권한 범위의 운영 판단

- **현재 가능한 교체:** 새 secret으로 다시 빌드하면 이전 secret의 모든 코드가 새 빌드에서 무효화된다. 테스터 전원 재발급이 필요하며, 키 ID·이중 키 이행·개별 취소 기능은 없다. 이번 작업에서는 키를 생성·교체하거나 GitHub secret을 설정하지 않았다.
- **이미 배포된 바이너리:** 기존 secret을 포함하므로 기존 코드를 계속 받는다. 업데이트를 배포해도 설치하지 않은 오프라인 복사본을 회수하지 못한다.
- **개별 취소:** 오프라인이라는 이유만으로 미래 빌드의 취소까지 불가능한 것은 아니다. 이후 빌드에 서명된 code ID denylist를 포함할 수는 있다. 다만 현재 구현에는 없고, 갱신하지 않는 기존 바이너리에는 효과가 없다.
- **무기한·후속 모든 베타:** 오랫동안 신뢰할 소수에게 영구 베타 접근을 제공하고 유출을 받아들일 목적이라면 편리하다. 기간별 테스트 참여·구버전 퇴출·테스터 이탈·권한 회수를 관리하려는 일반 베타 운영에는 부적절하다. 코드 한 번의 유출이 향후 같은 secret의 모든 베타로 이어진다.
- **정식판:** lifecycle은 `betaBuild`가 아니면 notBeta로 분류한다. 테스터 코드가 상용 정식 라이선스를 발급하는 기능은 아니다.

별도 결정이 필요한 제안(미적용):

1. 우선 무단 발급을 막아야 하는지, 편의용 공유 코드로 유출을 허용할지 결정한다. 방어가 필요하면 검증된 암호 라이브러리의 비대칭 서명으로 돌아가고 앱에는 공개키만 포함한다. RSASSA-PSS는 PKCS #1 v2.2/RFC 8017에 정의되어 있다: https://www.rfc-editor.org/rfc/rfc8017.html#section-8.1 . 긴 코드는 복사·붙여넣기나 서명된 권한 파일 가져오기로 UX를 해결할 수 있다. 짧은 활성화 코드를 유지하려면 서버에서 서명된 권한으로 교환하는 별도 설계가 필요하다.
2. 테스트 회차/버전 계열, 코드 기한, 고유 code ID, 키 ID 및 이행 규칙을 승인한다. 이번 검토는 임의로 기한이나 버전 범위를 정하지 않았다.
3. 기기 제한은 다중 PC·수리·OS 재설치 지원 부담까지 함께 결정한다. 파일 권한 강화만으로 기기 제한을 달성했다고 표시하지 않는다.

## beta.1 / beta.2와 삭제 후 수명주기

| 대상 | 만료 경계 | 확인 |
|---|---|---|
| 공개 1.3.2-beta.1 | 2027-02-11 23:59:00 KST / 1802357940 | 공개 릴리스와 원본 `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`의 정책을 다시 조회. 변경하지 않음. |
| 승인 1.3.2-beta.2 | 2027-03-31 23:59:00 KST / 1806505140 | 현행 policy·RELEASE_CHANNEL·UTC/KST 변환 및 경계 -1/0/+1초 검사. |

`now >= expires_at`에서 새 인스턴스가 만료다. 23:59분 전체나 23:59:59까지 허용한다는 뜻으로 바꾸지 않았다. beta.1 정책을 beta.2 소스에 넣으면 version mismatch로 거부된다. 최신 `release-policy.json`의 준비/게시 미승인 상태도 보존했다. 12/21까지 미공개면 만료일 재승인한다는 문구는 운영 절차이며 별도의 날짜 차단 코드가 생긴 것은 아니다.

코드를 성공적으로 제거하면 저장 파일이 사라지고 새 인스턴스는 원래 만료 정책을 사용한다. 만료 후에는 출력이 음소거되며 프로젝트 상태 복원으로 연장 권한이 돌아오지 않아야 한다. 이미 실행 중인 인스턴스는 기존 active-instance grace에 따라 계속 처리한다. 이 동작은 사용자 정책대로 유지했다. 삭제 실패는 성공으로 표시하지 않는다. 다른 위치에 복사한 코드는 여전히 유효하며 REMOVE는 원격 취소가 아니다.

## 검증 기록

검증 대상은 위 최신 기준 커밋에 이 수정 diff를 적용한 트리다. 운영 secret·개인키·실제 테스터 코드는 사용하지 않았다. 시스템 시계도 변경하지 않았다.

- 최신 원본 Python: tester 6개, lifecycle 12개 통과.
- 수정 Python: tester 9개(네이티브 교차 검사는 CTest 경로에서 실행), lifecycle 13개.
- release scope/CTest 증거 계약 통과. 게시 도구 35개, 프리셋 유지보수 23개 통과. 패키징 27개 중 24개 통과, Windows 전용 3개 skip.
- Linux Release CMake 빌드: 실제 `PluginProcessor.cpp`·`PluginEditor.cpp`와 `ChimeraBetaLifecycleProcessorTests` 빌드 완료. JUCE 8.0.8, Xvfb에서 CTest **5/5 통과**(20.01초): `ChimeraBetaLifecycleTests`, `ChimeraTesterCodeTests`, `ChimeraTesterCodeToolTests`, `ChimeraBetaLifecycleProcessorTests`, `ChimeraBetaLifecycleToolTests`.
- C++/Python 교차 입력, 비밀값 없는 빌드의 코드 거부, 다른 secret 거부, 변조, 0600 저장·교체, 심볼릭 링크/FIFO/초과 크기, 정상 코드의 다른 저장 위치 재사용, 삭제 성공/실패를 검사했다. 실제 processor에서 삭제 실패 시 권한 표시 보존, 성공 후 기존 인스턴스 처리 유지, 새 인스턴스 및 프로젝트 복원 후 만료·무음, 프로젝트에 코드/이름 미포함을 확인했다.
- CMake 비밀값 설정 검사: 없음/정상 hex는 성공, 홀수 길이/16바이트 미만/비hex는 실패. 시험용 secret만 사용했다.
- 테스트 명령: `xvfb-run -a ctest --test-dir <build> -C Release -R '^(ChimeraBetaLifecycleTests|ChimeraTesterCodeTests|ChimeraTesterCodeToolTests|ChimeraBetaLifecycleProcessorTests|ChimeraBetaLifecycleToolTests)$' --output-on-failure --no-tests=error`.
- 초기 RSA 트리의 전체 processor 빌드는 최신 HMAC 전환을 확인한 뒤 중단했다. 그 결과를 최신 구현의 통과 근거로 사용하지 않았다. 위 5/5는 최신 HMAC 기준 수정 트리의 별도 빌드 결과다.
- Windows/macOS 실제 앱·DAW·설치 패키지·다른 OS 계정 ACL, 공개 beta.1 바이너리에서 beta.2로의 실제 업그레이드는 이 Linux 검토에서 실행하지 않았다.
- 배포·병합·secret 등록·발급 권한 범위 변경은 실행하지 않았다.
