# 1.3.1-beta.3 조건부 릴리스 수용검사 정리 — 2026-10-16 KST

**판정: 공개 보류. 고정 제품 소스 `5c858442d0ed8360703e0ce63241a567736608a9`는 21 PASS / 107 BLOCKED다.**
이 문서와 도구는 증거 재사용 및 누락 검사 준비를 진전시킨다. 릴리스 승인이나 새로운 제품 바이너리 인증을 발급하지 않는다.

## 최신 상태와 재사용한 증거

- PR #50 `78899a87`의 10월 16일 조건부 목표는 `1.3.1-beta.3`다. PR #62는 Draft이고 소스가 바뀌지 않았다.
- GitHub 재조회에서 동일 소스의 12개 워크플로가 모두 성공했다. 구체적인 원본·job·조회 시각은 `github-snapshot.json`에 보존했다.
- [제품 CI 38386305674](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38386305674)의 원본 승인 게이트 ZIP은 GitHub artifact ID `11812195536` 및 SHA-256 `d34d6e1ac9335a5c6daf66b043f50818e9585e99d04b2bf8722482ff9e25bc14`와 일치한다.
- ZIP 내부의 Windows/macOS/Linux/assembly 매니페스트와 모든 원시 파일 해시를 검증하고 승인 게이트를 재계산했다. Windows 51, macOS 47, Linux 48개의 구성된 CTest와 기존 21개 자동 판정의 증거를 재사용했다. 전체 제품 테스트를 로컬에서 반복하지 않았다.
- 기존 lifecycle CI는 3개 OS의 실제 프로세서·직렬화·호스트 ID/순서/값·A/B·IR·MIDI와 beta.2 고정 소스로 만든 상태의 만료 전/시점/후 복원·재저장을 검증했다. 이 상태는 사용자 DAW 프로젝트가 아니다.
- Windows 기존 설치 증거에는 설치·경로 선택·복구·제거와 **공개 beta.1 설치본 업그레이드**가 있다. 문서 호환성 CI의 macOS PKG/Linux dpkg/Windows Inno 시험용 패키지는 실제 출시 제품 설치 수용검사와 구분한다.

## 107개 BLOCKED의 실제 작업 분류

| 분류 | 개수 | 지금 판정할 수 있는 내용 | 남은 조치 |
|---|---:|---|---|
| 개별 통과 조건 누락 | 86 | 구현·회귀·패키징의 부분 증거는 존재한다. 해당 ID의 원래 assertion과 producer가 없어 공식 PASS로 연결할 수 없다. | 원래 명세 복구 또는 명시적인 대체 명세 검토 → 기존 증거 매핑 → 빠진 부분만 실행 |
| 입력·기준음 증거 미연결 | 13 | 측정기·렌더러·일부 상태 왕복 검사는 존재한다. 동일 DI/기준음·최종 렌더의 연결이 부족하다. | 승인된 입력/기준의 해시·정규화·렌더 조건과 현재 소스를 묶어 측정·음향 왕복 검사 |
| 실제 시험 기록 누락 | 7 | 합성/네이티브 검사로 대체할 수 없다. | 청음 A/B, Windows DAW, Studio One 8, 30분 soak, 실입력 Gate, UI/오디오 타이밍, 실제 DAW live removal |
| 종합 승인 | 1 | B14.RESULT: 필수 조건이 미충족이다. | 필수 증거 완료 후 버전·소스·패키지별 최종 승인 |

107개 전부의 ID, hard gate 여부, 기존 증거, 다음 작업은 [blocked-checks.csv](triage/blocked-checks.csv)에 있다. [triage.json](triage/triage.json)은 기계 판독본이다.

86개의 stage별 수는 A1 6 / A2 8 / A3 9 / A4 5 / A5 8 / A6 8 / A7 7 / A8 4 / A9 9 / A10 6 / A11 9 / A14 7이다. 현재 소스의 `Validation/weekly-publication-policy.json`도 이 86개를 `ORIGINAL_DEFINITION_MISSING`으로 명시한다. 따라서 107개 모두를 단순한 실행 대기 테스트라고 해석하면 안 된다.

특히 A5/A6는 역사적인 Hartke/Marshall 개인 IR 목표를 가리킨다. 현재 1.3 공장 프리셋은 모델링 CAB 레시피를 사용하고 개인 IR은 일반 파일로 가져온다. 이 차이는 범위·명세 검토가 필요하며, 자동 N/A 처리나 사적 IR 재배포 승인으로 해석하지 않는다.

**기존 증거만으로 새로 공식 PASS 처리한 BLOCKED ID는 0개다.** 기존 21 PASS는 재검증됐고, 107개에는 후속 실행·검토 경로가 연결됐다. 명세 누락을 임의의 쉬운 조건으로 바꾸지 않았다.

## 이번 구현과 추가 자동검증

1. `Tools/triage_release_acceptance.py`: 고정 Git 소스의 정책을 읽고, GitHub artifact 해시·source/run/attempt·원시 CI 증거와 게이트를 다시 검증한다. 107개 전부를 CSV/JSON으로 분류한다. 오래된 소스, 해시 변조, 누락·중복 항목, 거짓 PASS 요약을 거부한다. 진단 성공은 공개 승인으로 사용되지 않는다.
2. `Tools/release_evidence.py`: A13.07의 assembly 검증을 강화했다. 업데이트 매니페스트의 소스 SHA, beta 호환 피드, 공식 release/download URL, 각 OS의 아키텍처를 후보와 함께 확인한다. 기존 publisher의 별도 검사에 더해 조기 진단을 보완한다. 현재 beta.3 원본은 강화된 검사도 통과했다.
3. `Tools/Verify-Beta3InstalledUpgrade.ps1` 및 전용 CI: 빠져 있던 **실제 공개 beta.2 Setup → 기존 beta.3 CI Setup** 설치 경로를 시험한다. 기존 설치 파일과 portable ZIP을 SHA-256으로 고정해 재사용한다. Standalone/VST3·설치 경로·lifecycle 메타데이터·사용자 파일 sentinel·수정한 매뉴얼 보존, 고의 손상 후 repair, uninstall을 검사한다. sentinel은 실제 DAW 프로젝트나 유효한 IR 음향 파일로 주장하지 않는다.

전용 CI 결과는 이 변경의 Draft PR에서 확인한다. 이 문서는 실행 전 작성된 계획과 확인된 기존 증거를 기록하며, 새 Windows 검사가 통과했다고 선행 선언하지 않는다. 검사에서 사용하는 제품 SHA는 `5c858442`이고 도구 SHA는 별도로 기록한다.

## 남은 실제 시험과 승인

| 작업 | 환경/기록 |
|---|---|
| 사용자 역사적 프로젝트 열기·재생·automation·bounce·재저장·재열기 | Studio One 8 및 사용 중인 Windows DAW/장치. 실제 프로젝트·프리셋·내장/개인 IR 유지 확인 |
| 호스트 종료·재생 중 인스턴스 제거·여러 인스턴스 생존 | Studio One/Cakewalk/Cubase 등 승인 대상 호스트별 버전과 로그 기록 |
| 실기 30분 soak·UI 조작 중 콜백 여유·클릭/드롭아웃 | 오디오 인터페이스·드라이버·sample rate/buffer·route·CPU 조건 기록 |
| Gate 실입력·Signature/E670FE 기준음·청음 | 입력 DI와 기준 캡처 해시, 게인/정규화 및 판단 기록 필요 |
| macOS/Linux 설치 후 오디오·호스트 스캔 | 기존 압축 해제/아키텍처 검사를 실제 제품 설치·장치 수용검사로 확장 |
| 만료일 승인 | 90/120/180일 중 선택은 미정. 최종 공개 예정 시점 기준의 **절대 UTC 날짜·시각**을 승인해야 한다. 다른 버전의 승인 날짜를 가져오지 않는다. |
| 최종 바이너리 검증·릴리스 승인 | 승인된 만료 정책을 커밋한 뒤 해당 소스로 새 패키지를 만들고 필요한 3개 OS 및 설치/수용검사를 수행. 버전·소스·패키지 해시별 별도 승인 |

`expires_at_utc=null`, `expiry_approved=false`, `RELEASE_REVIEW_PENDING`을 유지했다. 시험용 합성 만료일 패키지는 공개할 수 없다. #50 일정은 승인이나 만료일 결정을 대신하지 않는다. 릴리스·태그·공개 업데이트 피드를 변경하지 않는다.

## 재현

```sh
python Tools/test_release_consolidator.py
python Tools/test_publication_evidence.py
python Tools/test_triage_release_acceptance.py
python Tools/triage_release_acceptance.py \
  --archive docs/evidence/beta3-acceptance-20261016/ci-gate-38386305674.zip \
  --snapshot docs/evidence/beta3-acceptance-20261016/github-snapshot.json \
  --output build/beta3-acceptance-audit
```

도구가 종료 코드 0을 반환해도 출력의 `strict_verdict`는 BLOCKED다. 최종 승인 게이트나 publication receipt를 생성하는 도구가 아니다.
