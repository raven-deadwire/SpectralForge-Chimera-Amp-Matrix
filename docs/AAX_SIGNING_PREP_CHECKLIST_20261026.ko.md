# AAX 서명 준비 체크리스트 — 2026-10-30 KST 갱신

현재 단계는 **PACE 발급 완료 → USB iLok 초기 설정 → 첫 서명 → 일반(retail) Pro Tools 검증**입니다. 발급 완료는 AAX 배포 승인이나 실제 서명 완료가 아닙니다.

개발 호스트 경로는 **Avid 개발 번들 코드 발급 완료 → Avid 등록 → iLok 지급/활성화 확인 → Windows Developer 호스트 시험**입니다. 두 경로의 완료 여부를 따로 기록합니다.

- [AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md): 빌드·서명·설치·retail 수용 게이트
- [AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md](AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md): 공식 초기 설정과 도구 대조, 첫 서명 인계
- [AAX_SIGNING_CI_DESIGN.md](AAX_SIGNING_CI_DESIGN.md): 후속 자동화 설계
- [AAX_NATIVE_DEVELOPMENT.md](AAX_NATIVE_DEVELOPMENT.md): unsigned 개발 빌드
- [Windows Developer 시험 절차](AAX_WINDOWS_DEVELOPER_TEST_20261030.ko.md): 지정 CI 패키지, 등록·설치·로드·오디오·자동화와 결과표

**원칙:** 계정 ID·메일 주소·메일 원문/비공개 링크·iLok 시리얼·인증서/개인키·WCGUID·토큰·암호는 저장소와 CI 로그에 기록하지 않습니다. 실제 서명·최종 설치 패키지·일반 Pro Tools 검증 전까지 AAX 배포 기준은 **BLOCKED**입니다.

## 0. 확인한 사실과 남은 상태

**2026-10-27 08:15 KST** 수신한 PACE 관리자 계정 생성 및 라이선스 지급 공식 메일 두 건을 직접 확인했습니다.
**2026-10-29 01:45 KST** 수신한 Avid Devauth의 개발 번들 코드 발급 메일도 10/30에 직접 확인했습니다. 코드와 계정 정보는 기록하지 않습니다.

| 항목 | 확인 결과 | 이후 별도 확인 |
|---|---|---|
| PACE AAX Code Signing publisher / 관리자 설정 | 완료 | 실제 PACE Central 로그인 미확인 |
| 제공 범위 | LITE, AAX 디지털 서명 전용 Eden SDK 접근 | 전체 Fusion 보호 기능·White-Box 권한으로 확대 해석하지 않음 |
| PACE Central Access | 1개 지급, 만료일 **2027-11-01** | 2세대 이상 USB iLok 활성화 미확인 |
| Fusion SDK | 1개 지급, 만료일 **2027-11-01** | USB iLok 또는 iLok Cloud 활성화 미확인 |
| USB iLok | 주문 이력만 있음 | 도착·세대·연결·활성화·서명 인증서 미확인 |
| AAX Cloud Signing 서비스 | 별도 발급 근거 없음 | Fusion SDK의 Cloud 활성화 지원과 다른 권한 |
| Pro Tools Developer Bundle 코드 | **10/29 01:45 KST 발급 확인** | 재신청 불필요. Avid 코드 등록·대상 iLok 계정 확인은 미완료 |
| Pro Tools Intro / Developer Build / Prerelease | 등록 후 지급될 라이선스 3종을 발급 메일에서 확인 | 실제 iLok 지급·활성화는 미확인 |
| Avid 계정 작업(10/30) | 연결된 브라우저에서 로그인 화면까지 확인 | 로그인 필요; 이 작업에서 코드 입력·등록·약관 동의·라이선스 이전을 하지 않음 |
| 개발 빌드 CI | 소스 `340a6e081336c373f7bb3f6bf4ef44f823298797`, [run 41037134450](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41037134450)의 Windows/macOS 두 작업 성공 | 이 SHA의 빌드·회귀·개발 설치 검사만 해당. 실제 Pro Tools 호스트 결과 아님 |
| 실제 서명 / retail Pro Tools / 최종 설치기 | 미완료 | Windows·macOS 각 검증 필요 |

만료 **날짜**만 확인됐으며 정확한 시각·시간대는 명시되지 않았습니다. Chimera 베타 만료일과 별개입니다.

### 기존 #171의 정정과 이력 보존

- 10/22–10/24 UTC의 Avid 전달 → PACE 초대 → 신청 접수 이력은 보존합니다.
- 초대 전 7영업일을 접수 후 심사 기한으로 재사용한 **11/3 회신 예상·문의 일정은 폐기**합니다. 승인 대기 항목은 이제 발급 완료로 대체됩니다.
- 신청 단계 iLok **Validate 성공**은 기존 사용자 확인 이력입니다. 안읽음 메일만으로 미활성화를 추정하거나 계정을 다시 만들지 않습니다.
- Validate, 라이선스 지급, USB 활성화, 서명 성공은 각각 다른 상태입니다.
- Avid 코드 발급으로 기존 **10/29 재신청 항목은 종료**합니다. 코드 발급을 등록·지급·활성화 완료로 확대하지 않습니다.
- #171의 #164/#167 분류·SHA별 CI 이력·비AAX P0·청음/DI 링크와 작업창은 보존합니다.

## 1. USB iLok 도착 전 준비

- [x] 공식 메일의 관리자 설정·라이선스 지급·범위·만료일 확인.
- [x] Avid Pro Tools Developer Bundle 코드 발급 확인(코드 비공개).
- [x] CMake, AAX workflow, 개발 패키징과 공식 절차 대조(runbook 참조).
- [x] 읽기 전용 `Tools/aax_native.py signing-preflight` 추가. 소스 SHA·플랫폼·버전·베타 정책/해시·번들 무결성을 검사하며 서명·계정 접속·설치를 실행하지 않음.
- [ ] **최종 후보 소스의 네이티브 CI:** Windows x64·macOS Universal 실제 AAX 빌드, exports·공유 회귀·개발 ZIP 설치/제거. 과거 소스의 성공을 승계하지 않음.
- [ ] 서명 기계의 OS·도구·인증서 준비 상태 확인. 이 Linux 작업 환경에서 사용자 Windows/macOS 설치 상태는 확인하지 못함.
- [ ] 필요 시 최신 License Support/iLok License Manager 설치와 지급 라이선스 표시 확인. 지급과 활성화는 별도.
- [ ] **Avid 등록:** 로그인 후 Register Software With Code에서 발급 코드를 등록하고 본인 iLok 계정을 확인. 이미 등록된 경우 재사용/재신청 대신 등록 제품·지급 상태 확인.
- [ ] **iLok 확인:** Intro / Developer Build / Prerelease의 실제 지급과 각 라이선스의 허용 위치·활성화를 별도로 확인. Avid 등록 화면만으로 ILM 확인을 대체하지 않음.
- [ ] **Windows 호스트 준비:** 해당 Developer 배포본의 ReadMe에 맞춰 일반 Pro Tools를 먼저 실행하고 Developer 실행. Pro Tools 호스트 라이선스는 발급 안내상 USB 또는 Cloud 사용 가능하나 실제 Valid Locations 확인 필요. Central Access의 USB 요구와 구별.
- [ ] **Developer 시험:** unsigned 개발 패키지의 스캔·모노/스테레오 삽입·실제 DI 오디오·자동화 기록/재생 확인. 현재 모두 NOT_RUN.
- [ ] **세션 저장·재열기:** Developer는 저장이 비활성화되므로 signed AAX + 일반 Pro Tools에서 별도 검증. Developer 체크를 PASS로 바꾸지 않음.

## 2. USB iLok 도착 후 공식 초기 설정

10/27 PACE 관리자 안내 및 지급 메일 기준 순서입니다.

1. 최신 License Support(iLok License Manager 포함)를 준비하고 기존 계정으로 로그인.
2. **2세대 이상 USB iLok** 연결 후 **PACE Central Access**를 USB에 활성화.
3. ILM에서 로그아웃한 뒤 **같은 계정**으로 다시 로그인.
4. 계정 아래 **PACE CENTRAL** 버튼으로 접속.
5. **Fusion SDK**를 USB iLok 또는 지원되는 iLok Cloud에 활성화. Cloud 활성화만으로 USB 서명 인증서나 별도 AAX Cloud Signing 서비스가 생기지는 않음.
6. PACE Central HELP/다운로드에서 해당 계정용 AAX 서명 도구와 매뉴얼 확보.
7. 매뉴얼대로 USB 서명 인증서·제품 설정·WCGUID 및 설치된 `wraptool`의 배포 서명/검증 옵션 확인. 실제 값은 비공개 자격 증명 저장소에만 보관.

- [ ] 위 1–7의 실제 완료 확인.
- [ ] 제조사 `SpFg`, 제품 `Chm1`, 번들/효과 ID `audio.spectralforge.chimera` 대조.
- [ ] Windows Authenticode, macOS Developer ID Application 및 PKG용 Developer ID Installer·공증 자격 확인.
- [ ] 갱신 시 ILM USB **Synchronize**로 라이선스와 서명 인증서를 갱신하는 절차 확인. `wraptool` 실행만으로 갱신되지 않음.

## 3. 첫 서명 실행 (플랫폼별)

첫 실행은 로컬 서명 기계에서 수동 검증하는 경로로 준비합니다. self-hosted 자동화는 실제 첫 서명 이후 별도 구현합니다.

| 단계 | macOS Universal | Windows x64 |
|---|---|---|
| 최종 소스·버전·베타 승인 일치 및 입력 preflight | [ ] | [ ] |
| 설치된 PACE 매뉴얼의 OS/배포 서명 옵션 확정 | [ ] | [ ] |
| 최종 번들 복사본에 PACE 배포 서명 | [ ] | [ ] |
| PACE 및 OS 서명 검증 | [ ] | [ ] |
| 최종 네이티브 설치기 통합, 설치기 서명/공증 | [ ] | [ ] |
| 설치/업데이트/제거, 설치 후 PACE·OS 서명 재검증 | [ ] | [ ] |

서명 전후 소스 SHA·바이너리/패키지 SHA-256·도구 버전·결과를 각각 기록합니다. 원본 unsigned 패키지는 보존하고, 서명으로 바뀐 바이트를 기존 development receipt에 덮어쓰지 않습니다. 원시 vendor 로그는 비공개로 보관하고 공개 증거는 식별자·자격 증명을 제거한 결과 요약만 사용합니다.

현재 `Tools/aax_native.py package`는 **unsigned development 전용**입니다. `pace_signed: true` 거부 보호를 유지합니다. Windows/macOS 최종 signed 설치기 통합은 아직 미구현입니다.

## 4. 일반(retail) Pro Tools 검증

서명된 **최종 설치 패키지**로 설치한 뒤 일반 Pro Tools에서 검사합니다. unsigned 거부(-7054 등)가 없다는 사실만으로 전체 수용 검증을 대체하지 않습니다. 서명 전용 권한은 Chimera 사용자에게 새 iLok 제품 라이선스를 요구한다는 뜻이 아닙니다.

| 항목 | Windows | macOS Apple Silicon | macOS Intel(지원 주장 시) |
|---|---|---|---|
| AAX Native 스캔·모노/스테레오 삽입·실제 오디오 | [ ] | [ ] | [ ] |
| Classic/Dual/Matrix·bypass·샘플레이트/버퍼·PDC | [ ] | [ ] | [ ] |
| 자동화 기록·재생, realtime/offline bounce | [ ] | [ ] | [ ] |
| 세션 재열기·A/B·내장/개인/누락 IR 복원 | [ ] | [ ] | [ ] |
| 베타 수명주기·업데이트·사용자 데이터 보존 | [ ] | [ ] | [ ] |
| 다중 인스턴스·에디터 재열기·삭제/종료 안정성 | [ ] | [ ] | [ ] |
| 소스·설치기 해시·Pro Tools·OS/CPU·환경 기록 | [ ] | [ ] | [ ] |

실제 서명과 위 증거 확보 후 기존 릴리즈 게이트에 따라 판정합니다. 발급 확인·사전 점검·CI만으로 범위 문서를 PASS로 바꾸지 않습니다.

## 5. 일정과 beta.2 영향

| 목표 (KST) | 할 일 | 남은 조건 |
|---|---|---|
| 10/27 | PACE 발급 반영, 도구 대조·사전 점검 준비 | 문서/도구 작업이며 서명 완료 아님 |
| USB 도착 전 | 최종 소스 네이티브 빌드·개발 패키지 검증 | OS/toolchain·최종 후보 소스 |
| 10/29 발급 → 10/30 확인 | Pro Tools Developer Bundle 코드 발급 확인, 재신청 종료 | Avid 로그인·등록 → ILM 지급/활성화 → Windows 호스트 시험 필요 |
| USB 도착·활성화·인증서 준비 후 | Central 초기 설정, 두 플랫폼 첫 서명 | 실제 하드웨어·도구·OS 자격 |
| 11/9–11/13 | retail Pro Tools·설치기 검증 및 beta.2 판정 목표 유지 | 선행 증거 필요 |

11/3 회신 예상 및 회신 후 2–3일의 임의 일정은 사용하지 않습니다. 목표일까지 하드웨어·서명·설치기·호스트 검증이 끝나지 않으면 P0 AAX는 미달입니다. 공개 연기 또는 범위 변경은 별도 판정이며 자동 승인되지 않습니다.

## 근거

- 2026-10-27 08:15 KST PACE 공식 관리자 생성 및 지급 메일 두 건을 직접 확인. 계정 정보·비공개 링크는 미수록.
- 2026-10-29 01:45 KST Avid Devauth 공식 개발 번들 발급 메일 직접 확인. 코드·개인 메일 링크·iLok ID는 미수록.
- 2026-10-30 브라우저의 Avid 로그인 화면 및 위 CI run/아티팩트 메타데이터 확인. ILM·USB·Windows 네이티브 앱 접근 불가로 설치/활성화/호스트 시험 결과는 미확인.
- [PACE 공식 시작 안내](https://paceap.com/getting-started-with-aax-code-signing-for-pro-tools-plugins/)
- [PACE Code Signing Platform](https://paceap.com/code-signing-platform/)
- [iLok License Manager 공식 안내](https://help.ilok.com/ilm_overview.html)
- [Avid AAX SDK 공식 안내](https://developer.avid.com/aax/)
- PACE Central 상세 매뉴얼은 이번 공개 웹 조회로 열지 못함. 실제 계정 HELP/설치 SDK 매뉴얼의 정확한 명령·제품 설정은 미검증 상태로 유지.
