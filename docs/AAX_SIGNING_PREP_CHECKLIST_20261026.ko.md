# AAX 서명 준비 체크리스트 — 2026-11-04 KST 갱신

P0 **AAX Native**의 신청·발급과 실제 활성화·서명·호스트 검증을 구분하는 진행표입니다. **PACE 승인·도구 지급 및 PACE 3종 USB 활성화는 기존 확인 기록을 유지합니다. 11/3 소유자의 발송 메일에서 Avid 등록 완료와 개발자 라이선스의 iLok 계정 지급 성공을 확인했습니다. Pro Tools 라이선스의 USB/Cloud 활성화·Developer 실행·실제 AAX 서명·일반 Pro Tools 수용은 별도 미확인으로, AAX 배포는 BLOCKED입니다.** 절차의 근거와 명령은 기존 문서를 따릅니다.

- [AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md): 등록 경로, 빌드, codesign, `wraptool` 서명, 설치 경로, retail Pro Tools 검증(§1–§6)
- [AAX_SIGNING_CI_DESIGN.md](AAX_SIGNING_CI_DESIGN.md): 서명 자동화 설계(자체 호스팅 runner, 보호된 환경, 비밀값)
- [AAX_NATIVE_DEVELOPMENT.md](AAX_NATIVE_DEVELOPMENT.md): 서명 없는 개발 빌드

**원칙:** 활성화 코드, 계정 ID·개인 메일 주소, 비공개 메일/다운로드 링크, iLok 시리얼, 인증서·개인키, WCGUID, 토큰·암호는 저장소·PR·로그에 기록하지 않습니다. 메일 원문 대신 아래의 비밀정보 없는 확인 결과만 남깁니다. 실제 서명·최종 설치기·일반 Pro Tools 수용 증거를 확보하기 전까지 AAX는 릴리즈 범위 밖(**BLOCKED**)입니다.

## 0. 현재 상태 (2026-11-04 KST 검토, 최신 Avid 결과 11/3 21:10 KST)

| 일시 (KST) / 항목 | 확인 결과 | 남은 상태 |
|---|---|---|
| 10/27 08:15 — PACE 승인·계정 | AAX Code Signing publisher 계정과 관리자 설정 완료 | 실제 PACE Central 로그인 미확인 |
| 10/27 08:15 — PACE 도구 라이선스 | **PACE Central Access 1개, Fusion SDK 1개 지급 완료**, 만료 날짜 2027-11-01 | 10/30 확인한 USB 활성화 3종은 아래 행과 같음. 화면 표시명과 Fusion SDK의 대응·Cloud 활성화·서명 인증서 준비는 별도 미확인 |
| PACE 권한 범위 | LITE, **AAX 디지털 서명 전용** | 전체 Fusion 보호·제품 라이선싱·White-Box 권한으로 확대하지 않음 |
| 10/30 18:54 — USB iLok·PACE 활성화 | 소유자 제공 ILM 화면에서 **3세대 USB 연결·소유(Owned By You), Eden Tools·PACE Central Access·PACE Tools 각 1/1 활성화**, USB 총 3 Activations 확인 | PACE Central 접속·SDK 확보·실제 서명용 인증은 별도 미확인. Pro Tools 번들 지급·활성화의 증거가 아님 |
| 10/29 01:45 — Avid 개발 번들 | **Pro Tools Developer Bundle 코드 발급 완료** | 코드 발급은 제품 등록이나 iLok 지급·활성화 완료가 아님 |
| 10/30–11/2 — 등록 오류·문의 이력 | 10/30 invalid code 오류와 문의, 11/2 교체 코드의 iLok 인증 세션 만료·재등록 오류 및 후속 문의 | **과거 장애 / 11/3 등록 성공 보고로 종료**. 서버 측 원인은 확정하지 않음 |
| 11/3 21:00 — Devauth 재발급 | 최신 공식 회신에서 교체 코드 재발급 확인 | 코드 값은 비공개. 발급만으로 등록·활성화를 판정하지 않음 |
| 11/3 21:10 — Avid 등록·iLok 지급 | 소유자의 **발송 메일(SENT)**에서 등록 완료와 개발자 라이선스의 iLok 계정 지급 성공을 확인 | **완료(소유자 보고, 메일 대조)**. 이번 검토에서 계정 화면을 직접 확인한 것은 아님. 추가 코드 재발급 문의 불필요 |
| Pro Tools Intro / Developer Build / Prerelease | 번들의 개발자 라이선스 지급 성공은 위 메일로 확인 | 3종 각각의 ILM 표시·수량·허용 위치와 실제 USB/Cloud 활성화는 **미확인**. 계정 지급과 활성화를 구분 |
| `wraptool` / WCGUID / OS 인증서 | 이 검토에서 설치·설정 완료 증거 없음 | 플랫폼별 실물 서명 환경 확인 필요 |
| 실제 AAX 서명·Developer 실행·retail 수용 | 완료 증거 없음 | **미확인 / 이번 검토에서 실행하지 않음**, AAX 배포 BLOCKED 유지 |

PACE 만료는 날짜만 안내됐으며 정확한 시각·시간대는 미확인입니다. Chimera 베타 만료와 별개입니다. 메일이 읽지 않음으로 표시된다는 사실은 계정 미활성화의 증거가 아닙니다.

### 해결된 신청 단계와 남은 두 경로

- **이력 보존:** 기존 10/22–10/24 UTC의 Avid 전달·PACE 초대·신청 접수와 Avid 개발자 DB 확인 실패는 신청 당시 기록입니다. 현재의 PACE 승인 대기나 Avid 신규 재신청 지시로 사용하지 않습니다.
- **종료한 대기:** PACE 11/3 회신 예상은 10/27 발급으로 종료됐습니다. Avid 등록 실패·회신 대기는 11/3 등록·지급 성공 보고로 종료합니다. 반복 등록·신규 신청·추가 코드 재발급 문의를 다음 작업으로 잡지 않습니다.
- **개발 호스트 경로:** Avid 등록·iLok 계정 지급 완료(소유자 보고) → 라이선스별 USB/Cloud 활성화 확인 → 배포본 ReadMe·설치 버전 대조 → Developer 실행·unsigned AAX 시험. 이제 호스트 준비를 진행할 수 있지만 실행 검증 완료를 뜻하지 않습니다.
- **서명·배포 경로:** USB iLok/Central Access 활성화 확인 완료 → PACE Central·서명 도구/인증서·WCGUID 및 OS 자격 → 최종 후보 빌드 → 실제 서명·검증 → 최종 설치기 → 일반 Pro Tools 수용. Pro Tools 호스트 활성화와 별도 경로이므로 준비는 병행할 수 있습니다.

### 다음에 확보할 증거

1. ILM에서 Pro Tools 라이선스별 지급 표시·허용 위치·실제 USB 또는 Cloud 활성화를 확인합니다. PACE 3종의 기존 USB 활성화 기록으로 Pro Tools 활성화를 대체하지 않습니다.
2. 실제 설치된 Windows·일반/Developer Pro Tools 버전과 해당 배포본 ReadMe를 대조합니다. 11/3 제공된 `ReadMe_3PDev_Build.txt`(문서 표기 갱신일 2023-05-31)는 Windows의 이전 Pro Tools 제거, Developer 설치 후 **버전이 일치하는 일반 Pro Tools** 병행 설치를 안내합니다. 다른 버전 조합은 검증됐다고 가정하지 않으며 이 검토에서 설치·제거는 실행하지 않았습니다.
3. Developer의 스캔·모노/스테레오 오디오·자동화·PDC를 실제 호스트에서 기록합니다. 저장 관련 기능은 Developer에서 제한되므로 세션 저장·재열기는 signed AAX + 일반 Pro Tools의 별도 게이트입니다.

## 1. 소유자 준비: 발급 완료와 실제 활성화 구분

- [x] **PACE 승인·도구 지급 확인:** 관리자 계정 생성과 두 라이선스 지급 메일 확인.
- [x] **Avid 개발 번들 코드 발급 확인:** 코드 값은 비공개 유지.
- [x] **Avid 등록 장애 해소:** 11/3 21:10 KST 소유자의 발송 메일에서 등록 완료 확인(소유자 보고).
- [x] **개발자 라이선스의 iLok 계정 지급:** 같은 발송 메일에서 지급 성공 확인(소유자 보고). 라이선스별 ILM 화면 직접 검증과 구별한다.
- [ ] **Pro Tools 라이선스별 활성화:** ILM에서 Intro / Developer Build / Prerelease 각각의 표시·허용 위치·실제 USB/Cloud 활성화를 확인한다. PACE Central Access의 USB 요구 및 기존 PACE 도구 활성화와 구별한다.
- [x] **기존 iLok 계정 접근·하드웨어 확인:** 10/30 소유자 제공 ILM 화면에서 로그인 상태, 3세대 USB iLok 도착·연결·소유를 확인했다. 계정 식별자와 시리얼은 비공개로 보관한다.
- [x] **PACE USB 활성화:** 10/30 ILM 화면에서 Eden Tools·PACE Central Access·PACE Tools 각 1/1 활성화를 확인했다. Central Access는 **2세대 이상 USB iLok 필수**. 지급 안내상 Fusion SDK는 USB 또는 iLok Cloud 활성화를 지원하지만, 이것만으로 USB 없이 Central에 접속하거나 별도 Cloud Signing 서비스가 준비됐다는 뜻은 아니다. 실제 서명 경로는 계정의 도구·매뉴얼로 확인한다.
- [ ] **OS 서명 인증서:**
  - [ ] macOS: Apple Developer Program 가입, **Developer ID Application** 인증서, 공증용 App Store Connect API 키(또는 Apple ID + 앱 전용 암호)
  - [ ] Windows: Authenticode(OV/EV) 인증서 또는 서명 서비스. 설치 프로그램 서명용
  - PACE 서명과 OS 코드 서명은 별개이며 둘 다 필요하다.

## 2. 병행할 개발 준비와 Developer 호스트 검증

- [x] **현재 main의 개발 빌드 확인:** `d90511c9520af565dadc12c47793b8bdffd4c4bb`의 [AAX CI 41655816566](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41655816566)에서 Windows x64·macOS Universal 빌드와 각 5개 CTest가 성공했다. 상세 검증 범위는 §7.1을 따른다.
- [ ] **최종 후보 빌드 확인:** 실제 서명할 후보 SHA에서 빌드·정책·패키지를 다시 확인한다. 현재 main의 CI 성공을 후속 SHA의 배포 승인으로 승계하지 않는다.
- [ ] **Developer 빌드 점검(호스트 활성화·설치 조건 확인 후, NOT_RUN):** `Tools/aax_native.py package`로 만든 서명 없는 개발 패키지를 `install --acknowledge-developer-only`로 설치하고 Pro Tools Developer에서 스캔·모노/스테레오 삽입·오디오·자동화를 확인한다. 해당 배포본의 ReadMe를 따르고 **일반 Pro Tools를 먼저 실행**한다. Avid 안내상 **Developer는 세션 저장이 비활성화**되어 있으므로 저장·재열기 검증은 §5의 signed AAX + 일반 Pro Tools에서 수행한다. Developer 시험은 retail 승인을 대신하지 않는다.
- [ ] **서명 패키징 경로 준비:** 현재 `Tools/aax_native.py`는 서명 없는 개발 패키지만 다루고 `pace_signed: true`를 거부한다. 서명된 번들을 Windows·macOS 설치 프로그램의 표준 AAX 경로에 넣는 단계를 설계한다(구현은 서명 번들로 검증할 수 있을 때).
  - Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
  - macOS: `/Library/Application Support/Avid/Audio/Plug-Ins/`
- [ ] **서명 기계 결정:** 첫 서명은 iLok을 꽂은 로컬 기계에서 수동으로 할지, 자체 호스팅 runner(`ilok-signing` 레이블)를 먼저 만들지 정한다. CI 자동화(`aax-sign.yml`)는 실제 자격 증명으로 한 번 수동 서명에 성공한 뒤에 만든다.

## 3. PACE 발급 이후 실제 서명 환경 구성

- [ ] USB의 Central Access 활성화는 확인 완료. 별도 남은 단계로 ILM 재로그인 후 **PACE CENTRAL** 버튼 접속을 확인한다. 계정용 SDK·매뉴얼 확보 및 **`wraptool` 설치**(Windows·macOS 서명 기계 각각)
- [ ] iLok을 AAX 서명용으로 **인증**
- [ ] 이 플러그인의 **WCGUID** 확인 후 비공개 자격 증명 저장소에만 보관(공개 릴리즈 기록·PR·로그 제외)
- [ ] 식별자 대조: 제조사 코드 `SpFg`, 플러그인 코드 `Chm1`, 번들 ID `audio.spectralforge.chimera`(`CMakeLists.txt`와 PACE 계정 등록값이 같은지)
- [ ] 현재 `wraptool sign`의 **배포용(deployment) 서명** 옵션을 PACE 문서로 확인(체크리스트의 명령은 예시)

## 4. 첫 서명 실행 (플랫폼별)

| 단계 | macOS Universal | Windows x64 |
|---|---|---|
| 1. 릴리즈 소스로 빌드 | [ ] | [ ] |
| 2. Developer ID로 `codesign`(ad-hoc 아님) | [ ] | — |
| 3. `wraptool sign`(배포용, WCGUID) | [ ] | [ ] |
| 4. `wraptool verify` 통과 | [ ] | [ ] |
| 5. `codesign --verify --deep --strict` | [ ] | — |
| 6. 공증·staple | [ ] | — |
| 7. 설치 프로그램에 포함, 설치 프로그램 OS 서명 | [ ] | [ ] |

- 서명 기록(빌드 SHA, 서명 전후 바이너리/패키지 SHA-256, 서명 시각, 도구 버전, verify 결과)을 남긴다. 원시 vendor 출력은 비공개로 보관하고 공개 증거에는 계정·WCGUID·자격 증명을 제거한 요약만 남긴다. 설치 후에도 PACE·OS 서명을 재검증한다.

## 5. 일반(retail) Pro Tools 검증

Developer 빌드가 아닌 **일반 Pro Tools**와 일반 사용자 수준의 호스트 iLok 인증으로 확인합니다. 최종 서명 설치기를 사용하고, unsigned 거부(-7054 등)가 없다는 사실만으로 아래 수용 검증을 대체하지 않습니다. PACE의 AAX 서명 전용 권한을 Chimera 최종 사용자용 iLok 제품 라이선싱 도입으로 해석하지 않습니다.

| 항목 | Windows | macOS Apple Silicon | macOS Intel(지원 주장 시) |
|---|---|---|---|
| 플러그인 스캔 통과(AAX Native), -7054 없음 | [ ] | [ ] | [ ] |
| 모노·스테레오 트랙 삽입, 오디오 처리 | [ ] | [ ] | [ ] |
| 파라미터 자동화 기록·재생 | [ ] | [ ] | [ ] |
| 세션 저장·다시 열기, A/B·내장 IR 복원 | [ ] | [ ] | [ ] |
| 베타 수명주기 표시가 VST3/AU와 같음(새 만료일 없음) | [ ] | [ ] | [ ] |
| Pro Tools 버전·OS·테스터 환경 기록 | [ ] | [ ] | [ ] |

`AAX_NATIVE_DEVELOPMENT.md`의 추가 호스트 항목(라우팅·bypass·샘플레이트/버퍼·PDC, realtime/offline bounce, 다중 인스턴스·에디터/종료 안정성, 개인/누락 IR, 베타 경계·업데이트·설치/제거)도 그대로 적용합니다. CI 빌드·개발 패키지 검사는 실제 서명·Developer 실행·retail 수용의 증거가 아닙니다.

모두 통과하면 릴리즈 스케쥴과 범위 문서에서 AAX를 범위 안으로 옮기고 이 증거를 인용합니다.

## 6. 일정과 beta.2 영향

| 목표 (KST) | 할 일 | 막히는 조건 |
|---|---|---|
| 10/27–11/3 완료 | PACE 승인/도구 지급·기존 USB 활성화 확인, Avid 등록·개발자 라이선스 지급 성공 보고 | 신청·등록 오류·재발급 회신 대기는 종료 |
| 다음 개발 호스트 단계 | Pro Tools USB/Cloud 활성화 확인·설치 버전 대조·Developer 시험 | 실제 활성화·호스트 증거 필요. 현재 NOT_RUN |
| 11/4 검토 완료 | 현재 main AAX CI 대조, 로컬 개발 도구 31개 시험 및 Windows 산출물 격리 설치 시험 | 서명·Pro Tools 실행 검증은 아님(§7.1) |
| 병행 준비 → 실제 자격 준비 후 | USB 활성화 이후 Central·`wraptool`·인증서·WCGUID·후보 빌드·플랫폼별 첫 서명 | USB 도착·연결·PACE 3종 활성화는 확인 완료. PACE Central 접속·서명 환경 초기 설정, Windows/macOS 서명 기계·OS 자격은 별도 미확인. 소요일 미확정 |
| 11/9–11/13 | 최종 설치기·retail Pro Tools 검증, **beta.2 판정 목표 유지** | 실제 서명/검증·Windows/macOS 호스트 증거 필요 |

- 과거 11/3 PACE 회신 예상과 회신 후 2–3일 일정은 현재 예상치로 사용하지 않습니다. Avid 등록 해결은 11/3 소유자 보고로 확인됐으며, 활성화·서명·호스트 시험 완료일은 아직 미확정입니다.
- 11/13까지 실제 서명·최종 설치기·§5 수용 증거를 끝내지 못하면 P0 AAX는 미달입니다. beta.2 공개 연기 또는 범위 변경은 별도 판정이며, 이 문서 갱신은 공개일·만료일·출시 범위를 변경하거나 출시를 승인하지 않습니다.

## 7. 근거와 검토 범위

- **소유자 제공 ILM 화면**, 2026-10-30 18:54 KST: 3세대 USB의 소유·연결 및 Eden Tools·PACE Central Access·PACE Tools 각 1/1 활성화, USB 총 3 Activations를 확인한 기록을 반영했습니다. 공개 문서에는 비밀정보 없는 결과만 남깁니다. 이 확인은 PACE Central 접속·SDK 다운로드·Avid 계정 연결 권한 승인·Pro Tools 지급·실제 서명·호스트 시험의 증거가 아닙니다.

- **PACE 공식 메일 3건**, 2026-10-27 08:15 KST: `New PACE Central Administrator Account`, `PACE License Delivery Notification`, `Accessing the PACE AAX Code Signing Tools`. 계정 생성·실제 지급·권한 범위·USB/Cloud 활성화 안내를 대조했습니다.
- **Avid Devauth 공식 발급 메일**, 2026-10-29 01:45 KST: `Pro Tools Developer Bundle Activation Code`. 등록 이후 ILM에서 확인할 3종과 Developer 저장 제한·일반 Pro Tools 선행 실행 안내를 확인했습니다.
- **최신 Avid 메일 대조**, 2026-11-04 KST: 해당 스레드의 최근 3건(11/2 소유자 후속 문의, 11/3 21:00 KST Devauth 재발급, 11/3 21:10 KST 소유자 성공 회신)을 확인했습니다. 마지막 회신은 SENT이며 등록 완료·iLok 계정 지급 성공을 명시합니다. 소유자 보고와 직접 ILM/호스트 관찰을 구별하며 비공개 메일 링크·코드·계정 식별자는 남기지 않습니다.
- **소유자 제공 Developer ReadMe:** 11/3 제공된 `ReadMe_3PDev_Build.txt`의 라이선스 활성화, Windows 설치/병행 설치, Developer 저장 제한을 대조했습니다. 문서 표기 갱신일은 2023-05-31이며, 실제 설치 버전의 적합성·실행 성공을 입증하지 않습니다. 원문은 저장소에 복제하지 않습니다.
- **저장소 기준:** main `d90511c9520af565dadc12c47793b8bdffd4c4bb`의 AAX 문서, CMake, 워크플로, 도구·시험과 릴리즈 일정을 대조했습니다. PACE 과거 확인 기록은 보존하되 이번에 계정 상태를 직접 재검증한 것으로 쓰지 않습니다. unsigned 도구의 `pace_signed: true` 거부와 서명/호스트 게이트를 유지합니다.
- 관련 Draft [#171](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/171)의 발급/오류 기록도 대조했습니다. 그 PR의 미병합 도구·runbook은 main 구현이나 실제 실행 증거로 간주하지 않습니다. 과거 날짜의 절차 문서에 남은 신청 상태보다 이 진행표의 확인 날짜를 우선합니다.

### 7.1 이번에 수행·대조한 검증 (2026-11-04 KST)

| 검증 | 근거와 결과 | 적용 한계 |
|---|---|---|
| 기존 main 도구 | `Tools/aax_native.py`의 `verify/package/install/uninstall`, `Tests/test_aax_native.py`, `Tests/AAXContractTests.cpp`, `cmake/ChimeraAAX.cmake`, `aax-native.yml` 확인 | `signing-preflight`는 Draft #171의 미병합 기능이며 main 명령이 아님 |
| AAX 전용 CI | 위 SHA/run의 Windows·macOS job 모두 success. 각 5개 CTest, Windows DLL export·macOS Universal/ad-hoc seal, 격리/일회용 시스템 경로 설치·갱신·제거 단계 성공 | 기존 CI 로그 대조이며 이번에 새 CI를 실행하지 않음. ad-hoc seal은 PACE 서명이 아님 |
| 로컬 도구 회귀 | Linux x86_64 / Python 3.12.14에서 `python3 Tests/test_aax_native.py`: **31/31 PASS**, Python 구문 검사 PASS | 합성 PE/Mach-O와 파일 복구 시험. 실제 Windows/macOS 호스트 동작을 증명하지 않음 |
| Windows CI 산출물 | artifact `13240628486` 다운로드, 외부·내부 ZIP SHA-256 일치, receipt의 소스/플랫폼/전체 payload 해시 대조 PASS | `pace_signed=false`, `normal_host_accepted=false`, `release_approved=false` |
| 실제 Windows 패키지 격리 시험 | 기존 `--native-packages` 도구로 설치·동일 빌드 갱신·제거 및 개인 데이터 보존 **PASS** | Linux 임시 경로에서 Windows 파일을 다룬 시험. DLL 로드·사용자 Windows 설치·Pro Tools 실행은 하지 않음 |
| 라이선스 활성화·Developer·서명·일반 호스트 | **미확인 / NOT_RUN**, AAX 배포 **BLOCKED** | 이 실행 환경에는 Windows/macOS 호스트·Pro Tools·ILM/USB 접근·`wraptool`·OS 서명 도구가 없음 |

Windows 산출물 식별:

- 외부 artifact ZIP SHA-256: `b375dbe2e11c8bf2eea40f5cbc4560c83f990803f13f731dd0abe3c62ee638ca`
- 내부 개발 ZIP SHA-256: `bf719aa8e10baa0c21f6a3ca0964bb236f2c17b7a18a0b19a5858cfc482fe2a5`
- AAX 바이너리 SHA-256: `8b5e07f2db9d1cf676c789cd0a65f0bde2484dde66871a816b571425f18d88ea`
- 패키지 설치기 소스는 Windows CRLF를 LF로 정규화한 뒤 main 도구와 일치했습니다.
- CI는 qualification 옵션을 요청하지만 이 Windows 패키지의 실제 베타 메타데이터는 `mode=approved`, `expiry_approved=true`, 만료 `2027-03-31T14:59:00Z`입니다. 이는 베타 만료 정책 승인만 뜻하며 AAX 공개 승인이 아닙니다.
- macOS는 CI job·로그·artifact 메타데이터만 대조했으며, 이번 로컬 산출물 다운로드/검사는 Windows에 한정됩니다.
