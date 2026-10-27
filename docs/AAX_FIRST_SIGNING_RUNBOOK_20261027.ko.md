# 첫 AAX 서명 준비 및 인계 — 2026-10-27 KST

**상태: 입력 검증 도구 준비. 실제 PACE 서명·retail Pro Tools·signed 설치기 수용은 BLOCKED.**

## 1. 공식 절차와 구현의 대조

기준: main `b7d08a72252a6aaf85104f62ed070ee25e5d4837`와 기존 Draft #171 `c3774af9850e5f73a52e0c29cb2b25326d270e5b`를 병합한 작업 소스. #171의 비AAX 정정과 기존 링크를 보존하며 같은 PR에 후속 변경을 추가합니다.

| 단계 | 현재 Chimera 구현/근거 | USB 없이 가능한 범위 | 남은 실제 확인 |
|---|---|---|---|
| 컴파일 | `cmake/ChimeraAAX.cmake`, JUCE 8.0.8 내장 AAX SDK, `CHIMERA_BUILD_AAX=ON` | Windows x64/MSVC 및 macOS Universal 빌드 | 최종 후보 SHA의 두 OS CI |
| ABI·공유 회귀 | `aax-native.yml`, AAXContract/HostBypass/BetaLifecycle/Gate | exports·아키텍처·파라미터·상태·지연 계약 검사 | 실제 Pro Tools PDC/자동화/오디오 |
| 개발 패키징 | `Tools/aax_native.py package` | 버전·베타·바이너리 해시, unsigned ZIP | signed 배포 패키징은 미구현 |
| 입력 인계 | 이번 `signing-preflight` | 소스·플랫폼·receipt/build/beta·번들 일치 | 서명 자격·프로세스는 검사하지 않음 |
| 개발 설치 | `Tests/test_aax_native.py` | 격리/일회용 시스템 경로 설치·갱신·제거 | 일반 사용자 최종 native 설치기 |
| PACE Central | 10/27 공식 관리자/지급 메일 | 도구 대조와 ILM 준비 | Central Access를 2세대 이상 USB에 활성화해야 접속 |
| Fusion SDK | 지급 완료, USB 또는 iLok Cloud 활성화 지원 | Cloud 활성화 지원 사실 확인 | 실제 활성화와 도구 설치 미확인 |
| AAX 서명 | 자동화/실행 도구 미구현 | source/hash 인계와 서명 기계 계획 | USB 서명 인증서·제품 설정·WCGUID·현재 매뉴얼 |
| 배포 | 기존 public publisher에 AAX 자동 추가 없음 | 기존 배포 차단 유지 | PACE/OS 서명·설치 후 재검증·retail 수용 |

Fusion SDK의 iLok Cloud 활성화는 **별도 AAX Cloud Signing 서비스**와 다릅니다. 발급 메일로 그 서비스의 권한을 추정하지 않습니다. Windows/macOS 서명 기계, USB 도착·활성화, `wraptool` 설치 상태는 이 Linux 환경에서 확인하지 못했습니다.

## 2. USB 도착 전 실행 가능한 명령

일반 빌드·패키지 명령은 [개발 문서](AAX_NATIVE_DEVELOPMENT.md)를 사용합니다.
Windows x64와 macOS Universal 각 플랫폼에서 동일 후보 SHA로 빌드합니다.
CI의 qualification-only 베타 정책은 배포 승인이 아니며 승인된 실제 베타 정책과 구분합니다.

저장소의 **정확한 후보 커밋 checkout**에서, 신뢰한 CI 실행과 다운로드 ZIP 체크섬을 먼저 대조하고 압축 해제합니다. 아래 경로는 예시이며 `<FULL_SOURCE_SHA>`는 해당 실행의 40자리 소스 SHA로 바꿉니다.

```sh
python Tools/aax_native.py signing-preflight --package /path/to/extracted-package --platform windows --expected-source <FULL_SOURCE_SHA>
python Tools/aax_native.py signing-preflight --package /path/to/extracted-package --platform macos --expected-source <FULL_SOURCE_SHA>
```

실제 배포 서명 입력을 고를 때는 **`--require-approved-beta`를 추가**합니다. 미승인 qualification-only 베타를 거부합니다. 이 옵션 통과 역시 제품·서명·호스트·공개 승인 게이트를 대신하지 않습니다.

검사는 읽기 전용이며 PACE 접속·계정 환경변수 조회·도구 호출·서명·설치를 하지 않습니다.
리포트의 `input_integrity: PASS`는 파일/메타데이터 일치만 의미합니다.
`pace_verification`과 `platform_signature_verification`은 항상 `NOT_RUN`,
`normal_host_accepted`와 `release_approved`는 항상 `false`입니다.
receipt의 자가 선언만으로 빌드 출처를 인증하지 않으므로 CI 실행·다운로드 체크섬 확인은 필수입니다.

네이티브 ZIP 설치 테스트가 실제 압축 해제본에도 이 검사를 실행하고
`dist-aax/signing-preflight.json`을 CI artifact에 포함합니다.
이 도구는 개발 설치기와 달리 소스 checkout의 `beta_lifecycle`/`chimera_version` 모듈이 필요합니다.
서명 이후의 파일은 원래 unsigned receipt와 달라지므로 이 명령 대신 PACE/OS 서명 검증을 사용합니다.

## 3. USB 도착 후 첫 수동 서명

1. [준비 체크리스트 §2](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md)의 ILM 설치 → Central Access USB 활성화 → 동일 계정 재로그인 → PACE CENTRAL 순서를 완료합니다. Fusion SDK 활성화는 별도 확인합니다.
2. PACE Central HELP/다운로드와 설치된 SDK에서 버전·deployment 서명·인증서 설정·검증 명령을 확인합니다. 공개 웹에서 상세 매뉴얼을 열지 못했으므로 임의의 `--keypassword`/WCGUID 생성 방식/자동 로그인 가정을 실행하지 않습니다.
3. 제품 식별자 `SpFg` / `Chm1` / `audio.spectralforge.chimera`를 대조하고 WCGUID와 인증서는 비공개 자격 증명 저장소에 보관합니다. 첫 서명은 USB를 연결한 로컬 기계에서 진행합니다.
4. 검증한 unsigned archive를 보존하고, 번들 복사본을 별도 비공개 작업 디렉터리로 옮깁니다. 복사 전후 파일 해시가 일치하는지 확인합니다. 원본을 in-place 서명하지 않습니다.
5. 설치된 PACE 매뉴얼의 OS 서명 통합 순서로 배포 서명하고 PACE 검증을 수행합니다. macOS Developer ID 및 Windows 코드 서명 방식은 실제 자격·SDK 기준으로 확정합니다. macOS ad-hoc seal은 배포 서명이 아닙니다.
6. 서명된 최종 번들을 native 설치기에 통합합니다. 현재 unsigned 개발 packager의 `pace_signed: true` 거부를 해제하거나 JSON만 수정하지 않습니다. 설치기 서명·공증 및 설치 후 PACE/OS 검증을 완료해야 합니다.
7. [retail 게이트](AAX_RELEASE_CHECKLIST.md)대로 Windows 및 macOS의 일반 Pro Tools에서 실제 설치본을 검증합니다. Developer 라이선스 발급/검증과 별개입니다.

## 4. 증거와 자격 증명 분리

공개할 수 있는 증거: 후보 소스 SHA, 버전·플랫폼, 서명 전/후 번들과 최종 설치기 해시, 도구 버전, 검증 결과 요약, Pro Tools 버전·OS/CPU·실제 검증 항목.

비공개로 둘 값: 계정 ID·개인 메일·메일 링크·USB 시리얼·WCGUID·개인키·인증서 파일·패스워드·토큰·원시 vendor 로그. shell tracing이나 전체 명령/환경변수 출력도 피합니다. 저장소용 요약은 민감정보 제거 후 검토합니다. 신규 secret/workflow/계정 설정은 이번 변경에서 수행하지 않았습니다.

라이선스 갱신 후 USB의 라이선스/코드 서명 인증서는 ILM Synchronize로 업데이트합니다. 실행 도구 자체가 갱신하지 않는다는 공식 지급 안내를 유지합니다. 2027-11-01은 도구 라이선스 만료 날짜이며 Chimera 베타 만료일 변경 근거가 아닙니다.

## 5. 이번 검증 범위

- Linux에서 `python Tests/test_aax_native.py`: **40개 테스트 PASS**. CLI 실행/실패 종료, 입력 SHA/플랫폼 불일치, receipt/build 불일치, 베타 바이트·정책 변조, payload 변조, 잘못된 signed 주장, 누락/링크 metadata, qualification-only 배포 차단을 포함합니다.
- 기존 개발 설치/복구·사용자 데이터 보존 테스트도 같은 실행에서 통과했습니다.
- 이는 합성 번들/로컬 도구 검사입니다. **실제 Windows/macOS AAX 빌드·서명·설치기·Pro Tools 검증 결과로 확대하지 않습니다.** 최종 PR head CI는 해당 SHA의 Actions에서 별도 확인합니다.
- #171 원본 비AAX 수정과 링크, P0 및 기존 10/29 Developer 재신청·11/9–11/13 판정 목표를 보존합니다. 승인 대기와 11/3 회신 예상은 발급 사실로 대체합니다.

## 공식 근거

2026-10-27 08:15 KST PACE 관리자 생성/라이선스 지급 메일을 직접 확인했으며 원문·계정 식별자는 저장하지 않았습니다.

- [PACE 시작 안내](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
- [PACE 서명 플랫폼](https://paceap.com/code-signing-platform/)
- [iLok License Manager](https://help.ilok.com/ilm_overview.html)
- [Avid AAX SDK](https://developer.avid.com/aax/)

상세 PACE Central 매뉴얼의 로그인 후 설정·명령은 **미검증**입니다. 현재 설치 SDK의 매뉴얼과 HELP에서 확정하기 전에는 서명 성공을 주장하지 않습니다.
