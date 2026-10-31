# AAX 서명 준비 체크리스트 — 2026-10-31 KST 갱신

P0 **AAX Native**의 신청·발급과 실제 활성화·서명·호스트 검증을 구분하는 진행표입니다. **PACE 승인·도구 라이선스 지급과 Avid 코드 발급은 완료됐지만, Avid 코드 등록은 거절됐고 실제 AAX 서명·Pro Tools 실행은 미확인입니다.** 절차의 근거와 명령은 기존 문서를 따릅니다.

- [AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md): 등록 경로, 빌드, codesign, `wraptool` 서명, 설치 경로, retail Pro Tools 검증(§1–§6)
- [AAX_SIGNING_CI_DESIGN.md](AAX_SIGNING_CI_DESIGN.md): 서명 자동화 설계(자체 호스팅 runner, 보호된 환경, 비밀값)
- [AAX_NATIVE_DEVELOPMENT.md](AAX_NATIVE_DEVELOPMENT.md): 서명 없는 개발 빌드

**원칙:** 활성화 코드, 계정 ID·개인 메일 주소, 비공개 메일/다운로드 링크, iLok 시리얼, 인증서·개인키, WCGUID, 토큰·암호는 저장소·PR·로그에 기록하지 않습니다. 메일 원문 대신 아래의 비밀정보 없는 확인 결과만 남깁니다. 실제 서명·최종 설치기·일반 Pro Tools 수용 증거를 확보하기 전까지 AAX는 릴리즈 범위 밖(**BLOCKED**)입니다.

## 0. 현재 상태 (2026-10-31 17:13 KST 메일 재확인)

| 일시 (KST) / 항목 | 확인 결과 | 남은 상태 |
|---|---|---|
| 10/27 08:15 — PACE 승인·계정 | AAX Code Signing publisher 계정과 관리자 설정 완료 | 실제 PACE Central 로그인 미확인 |
| 10/27 08:15 — PACE 도구 라이선스 | **PACE Central Access 1개, Fusion SDK 1개 지급 완료**, 만료 날짜 2027-11-01 | 실제 기기/Cloud 활성화와 서명 인증서 준비는 미확인 |
| PACE 권한 범위 | LITE, **AAX 디지털 서명 전용** | 전체 Fusion 보호·제품 라이선싱·White-Box 권한으로 확대하지 않음 |
| USB iLok | 기존 설계 문서에 주문 이력 | 도착·연결·2세대 이상 여부·Central Access 활성화 미확인 |
| 10/29 01:45 — Avid 개발 번들 | **Pro Tools Developer Bundle 코드 발급 완료** | 코드 발급은 제품 등록이나 iLok 지급·활성화 완료가 아님 |
| 10/30 약 18:56 — Avid 등록 시도 | 발급 메일과 일치하는 코드를 제출했으나 **“Sorry, the code you provided is invalid”** 오류 발생(소유자의 발송 메일로 확인) | **BLOCKED_INVALID_CODE**. 제품 등록 완료·iLok 대상 계정 확인·라이선스 지급 단계에 도달하지 못함 |
| 10/30 19:22 — Devauth 문의 | **발송 완료(SENT)**. 코드 상태·계정 연결·정상 등록 방법 확인 요청 | 10/31 17:13 재조회 시 해당 스레드는 발급 메일+문의 메일 2건이며 후속 회신 없음. 같은 기간 별도 관련 수신 메일도 발견되지 않음 |
| Pro Tools Intro / Developer Build / Prerelease | 발급 안내가 등록 후 지급 대상으로 명시한 3종 | 실제 iLok License Manager 표시·활성화 **미확인**. My Products의 기존 Intro 표시만으로 이번 번들 지급을 판정하지 않음 |
| `wraptool` / WCGUID / OS 인증서 | 이 검토에서 설치·설정 완료 증거 없음 | 플랫폼별 실물 서명 환경 확인 필요 |
| 실제 AAX 서명·Developer 실행·retail 수용 | 완료 증거 없음 | **미확인 / 이번 검토에서 실행하지 않음**, AAX 배포 BLOCKED 유지 |

PACE 만료는 날짜만 안내됐으며 정확한 시각·시간대는 미확인입니다. Chimera 베타 만료와 별개입니다. 메일이 읽지 않음으로 표시된다는 사실은 계정 미활성화의 증거가 아닙니다.

### 해결된 신청 단계와 남은 두 경로

- **이력 보존:** 기존 10/22–10/24 UTC의 Avid 전달·PACE 초대·신청 접수와 Avid 개발자 DB 확인 실패는 신청 당시 기록입니다. 현재의 PACE 승인 대기나 Avid 신규 재신청 지시로 사용하지 않습니다.
- **종료한 대기:** PACE 11/3 회신 예상·미회신 문의 일정은 10/27 발급으로 종료합니다. Avid 재신청 단계는 코드 발급으로 대체됐고, 현재 필요한 회신은 **이미 발급된 코드의 등록 실패 해결**입니다.
- **개발 호스트 경로:** Devauth 답변 → 코드 상태/계정 연결 해결 → Avid 등록 완료 → 실제 iLok 지급·활성화 → Developer 실행·unsigned AAX 시험.
- **서명·배포 경로:** USB iLok/Central Access → PACE Central·서명 도구/인증서·WCGUID 및 OS 자격 → 최종 후보 빌드 → 실제 서명·검증 → 최종 설치기 → 일반 Pro Tools 수용. Avid 등록 실패와 별도 경로이므로 준비는 병행할 수 있습니다.

### Devauth 회신으로 확인할 사항

10/30 문의는 이미 발송됐으며 이번 문서 갱신에서 추가 메일을 보내지 않았습니다. 답변을 받으면 다음을 확인합니다.

1. 발급 코드의 유효성·사용/등록 상태와 invalid 오류의 원인. 만료·기사용·오타 등을 원인으로 단정하지 않습니다.
2. 의도한 Avid 개발자 계정과의 연결 및 정상 등록 방법. 계정 실제 값은 비공개 채널에서만 확인합니다.
3. 재발급 또는 서버 측 수정이 필요한지, 조치 완료 후 재시도 시점/절차. 코드 재발급은 아직 요청 결과나 완료 사실로 기록하지 않습니다.
4. 등록 해결 후 확인할 iLok 대상과 3종 라이선스 지급 절차. **회신만으로 완료 처리하지 않고** 등록 결과·ILM 지급·활성화·호스트 실행 증거를 각각 확보합니다.

## 1. 소유자 준비: 발급 완료와 실제 활성화 구분

- [x] **PACE 승인·도구 지급 확인:** 관리자 계정 생성과 두 라이선스 지급 메일 확인.
- [x] **Avid 개발 번들 코드 발급 확인:** 코드 값은 비공개 유지.
- [x] **등록 실패 문의 발송 확인:** 10/30 19:22 KST Devauth 발송 메일 확인.
- [ ] **Avid 등록 장애 해소:** 위 회신 항목을 확인한 뒤 안내에 따라 등록 완료. 현재 재신청이나 반복 코드 제출을 완료 항목으로 만들지 않는다.
- [ ] **Pro Tools 라이선스 지급·활성화:** iLok License Manager에서 Intro / Developer Build / Prerelease 각각의 지급과 허용 위치·활성화를 확인한다. Avid 안내상 USB 또는 iLok Cloud 경로가 가능하며 PACE Central Access의 USB 요구와 구별한다.
- [ ] **기존 iLok 계정 접근·하드웨어 확인:** 기존 계정 로그인과 2세대 이상 USB iLok 도착·연결을 확인한다. 시리얼은 비공개로 보관한다.
- [ ] **PACE 도구 활성화:** Central Access는 **2세대 이상 USB iLok 필수**. 지급 안내상 Fusion SDK는 USB 또는 iLok Cloud 활성화를 지원하지만, 이것만으로 USB 없이 Central에 접속하거나 별도 Cloud Signing 서비스가 준비됐다는 뜻은 아니다. 실제 서명 경로는 계정의 도구·매뉴얼로 확인한다.
- [ ] **OS 서명 인증서:**
  - [ ] macOS: Apple Developer Program 가입, **Developer ID Application** 인증서, 공증용 App Store Connect API 키(또는 Apple ID + 앱 전용 암호)
  - [ ] Windows: Authenticode(OV/EV) 인증서 또는 서명 서비스. 설치 프로그램 서명용
  - PACE 서명과 OS 코드 서명은 별개이며 둘 다 필요하다.

## 2. 병행할 개발 준비와 Developer 호스트 검증

- [ ] **빌드 확인:** 릴리즈 후보 소스에서 `aax-native.yml`이 통과하고, Windows x64·macOS Universal(arm64 + x86_64) `.aaxplugin`이 만들어진다.
- [ ] **Developer 빌드 점검(등록·지급·활성화 확인 후):** `Tools/aax_native.py package`로 만든 서명 없는 개발 패키지를 `install --acknowledge-developer-only`로 설치하고 Pro Tools Developer에서 스캔·모노/스테레오 삽입·오디오·자동화를 확인한다. 해당 배포본의 ReadMe를 따르고 **일반 Pro Tools를 먼저 실행**한다. Avid 안내상 **Developer는 세션 저장이 비활성화**되어 있으므로 저장·재열기 검증은 §5의 signed AAX + 일반 Pro Tools에서 수행한다. Developer 시험은 retail 승인을 대신하지 않는다.
- [ ] **서명 패키징 경로 준비:** 현재 `Tools/aax_native.py`는 서명 없는 개발 패키지만 다루고 `pace_signed: true`를 거부한다. 서명된 번들을 Windows·macOS 설치 프로그램의 표준 AAX 경로에 넣는 단계를 설계한다(구현은 서명 번들로 검증할 수 있을 때).
  - Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
  - macOS: `/Library/Application Support/Avid/Audio/Plug-Ins/`
- [ ] **서명 기계 결정:** 첫 서명은 iLok을 꽂은 로컬 기계에서 수동으로 할지, 자체 호스팅 runner(`ilok-signing` 레이블)를 먼저 만들지 정한다. CI 자동화(`aax-sign.yml`)는 실제 자격 증명으로 한 번 수동 서명에 성공한 뒤에 만든다.

## 3. PACE 발급 이후 실제 서명 환경 구성

- [ ] ILM에서 Central Access를 USB에 활성화하고 재로그인한 뒤 **PACE CENTRAL** 버튼으로 접속. 계정용 SDK·매뉴얼 확보 및 **`wraptool` 설치**(Windows·macOS 서명 기계 각각)
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
| 10/27·10/29 완료 | PACE 승인/도구 지급·Avid 코드 발급 | 신청 대기를 현재 차단 요인에서 제거 |
| 10/30 문의 발송 → 회신 대기 | Avid 등록 오류 해소 | Devauth의 코드 상태·계정 연결·등록 방법 확인. 회신 기한 미확정 |
| 회신 해결 후 | Avid 등록·ILM 지급/활성화·Developer 시험 | 실제 계정/호스트 증거, 배포본 ReadMe |
| 병행 준비 → 실제 자격 준비 후 | USB/Central·`wraptool`·인증서·WCGUID·후보 빌드·플랫폼별 첫 서명 | USB 실물, PACE 초기 설정, Windows/macOS 서명 기계·OS 자격. 소요일 미확정 |
| 11/9–11/13 | 최종 설치기·retail Pro Tools 검증, **beta.2 판정 목표 유지** | 실제 서명/검증·Windows/macOS 호스트 증거 필요 |

- 과거 11/3 PACE 회신 예상과 회신 후 2–3일 일정은 현재 예상치로 사용하지 않습니다. Avid 등록 해결일도 확정하지 않습니다.
- 11/13까지 실제 서명·최종 설치기·§5 수용 증거를 끝내지 못하면 P0 AAX는 미달입니다. beta.2 공개 연기 또는 범위 변경은 별도 판정이며, 이 문서 갱신은 공개일·만료일·출시 범위를 변경하거나 출시를 승인하지 않습니다.

## 7. 근거와 검토 범위

- **PACE 공식 메일 3건**, 2026-10-27 08:15 KST: `New PACE Central Administrator Account`, `PACE License Delivery Notification`, `Accessing the PACE AAX Code Signing Tools`. 계정 생성·실제 지급·권한 범위·USB/Cloud 활성화 안내를 대조했습니다.
- **Avid Devauth 공식 발급 메일**, 2026-10-29 01:45 KST: `Pro Tools Developer Bundle Activation Code`. 등록 이후 ILM에서 확인할 3종과 Developer 저장 제한·일반 Pro Tools 선행 실행 안내를 확인했습니다.
- **소유자의 발송 메일**, 2026-10-30 19:22 KST: 같은 제목의 회신. 18:56경 등록 실패·iLok 확인 단계 미도달·코드 상태/계정 연결 문의를 확인했습니다. 웹 오류 자체를 이번 작업에서 재현한 것은 아닙니다.
- **재조회 범위:** 2026-10-31 17:13 KST에 해당 스레드 전체 및 10/28 이후 Avid/PACE/iLok 관련 수신 메일 검색을 확인했습니다. 이후 도착하는 회신은 이 스냅샷에 포함되지 않습니다.
- **저장소 기준:** main `472b93e68d20bcad07a028cc67a17b50f8171838`의 위 AAX 문서 3종, `.github/workflows/aax-native.yml`, `Tools/aax_native.py`, 릴리즈 일정과 대조했습니다. unsigned 개발 도구의 `pace_signed: true` 거부와 서명/호스트 게이트는 보존합니다.
- 관련 Draft [#171](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/171)의 발급/오류 기록도 대조했습니다. 그 PR의 미병합 도구·runbook은 main 구현이나 실제 실행 증거로 간주하지 않습니다. 과거 날짜의 절차 문서에 남은 신청 상태보다 이 진행표의 확인 날짜를 우선합니다.
