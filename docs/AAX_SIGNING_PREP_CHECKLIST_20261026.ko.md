# AAX 서명 준비 체크리스트 — 2026-10-26 KST

P0 **AAX Native**를 위해 PACE 승인 → iLok → 서명 → 일반(retail) Pro Tools 검증까지 남은 일을 한곳에 모은 진행표입니다. 절차의 근거와 명령은 기존 문서를 따릅니다.

- [AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md): 등록 경로, 빌드, codesign, `wraptool` 서명, 설치 경로, retail Pro Tools 검증(§1–§6)
- [AAX_SIGNING_CI_DESIGN.md](AAX_SIGNING_CI_DESIGN.md): 서명 자동화 설계(자체 호스팅 runner, 보호된 환경, 비밀값)
- [AAX_NATIVE_DEVELOPMENT.md](AAX_NATIVE_DEVELOPMENT.md): 서명 없는 개발 빌드

**원칙:** iLok, PACE 계정, 인증서, WCGUID 같은 자격 증명은 저장소에 넣지 않고 이름으로만 적습니다. 이 체크리스트의 항목이 모두 끝나기 전까지 AAX는 릴리즈 범위 밖(BLOCKED)입니다.

## 0. 현재 상태 (메일 기준, 10/26 확인)

| 날짜 (UTC) | 내용 | 상태 |
|---|---|---|
| 10/22 | Avid Audio SDK 팀에 PACE 서명 도구 신청, devauth에 Pro Tools Developer 라이선스 요청 | 보냄 |
| 10/23 | Avid Audio SDK 팀: PACE 전달 후 **초대 회신**까지 보통 7영업일 안내 | 이후 초대 수신으로 해당 단계 완료 |
| 10/23 | PACE: AAX 코드 서명 도구 초대 메일 | 확인 |
| 10/23 | devauth: Developer 라이선스 요청 메일 주소가 AAX 개발자 DB에서 확인되지 않아 재신청 안내 | 별도 10/29 재신청 일정 유지 |
| 10/24 | PACE: 서명 도구 신청 **접수 확인** | 제출 완료. 승인·라이선스 발급과 구분 |
| 10/24 | 신청 과정에서 iLok **Validate 성공을 사용자 확인**, 이후 PACE 최종 제출·접수 완료 | 메일 읽음 상태만으로 미활성화를 추정하지 않음 |

초대 전 전달 단계의 **7영업일 안내와 접수 후 심사는 다른 단계**입니다. 이미 초대를 받아 제출했으므로 이를 다시 적용해 11/3을 심사 예정일·문의 기한으로 계산하지 않습니다. 접수 화면의 안내 기록과 후속 공식 회신을 기준으로 기존 심사 확인을 계속합니다. 접수 확인 메일은 제출 사실을 확인할 뿐 승인이나 확정 발급일을 뜻하지 않습니다.

메일의 읽음/안읽음 표시는 계정 활성화 상태가 아닙니다. Validate 성공은 서명 라이선스 발급·USB iLok 준비·실제 서명 성공까지 증명하지 않으므로 그 후속 단계는 따로 검증합니다.

## 1. 지금 할 수 있는 일 (소유자, 승인 전)

- [x] **신청 단계 iLok Validate 확인:** 사용자 확인과 접수 완료 기록을 보존한다. 안읽음 메일만을 근거로 활성화를 다시 요구하거나 계정을 새로 만들지 않는다.
- [ ] **Pro Tools Developer Bundle 재신청:** 이미 정한 **10/29 별도 일정**을 유지한다. 기존 승인·중복 확인 조건에 따라 진행하며, 이 체크리스트를 근거로 즉시 중복 제출하지 않는다. Developer 빌드의 서명 없는 AAX 점검은 retail 수용을 대신하지 않는다.
- [ ] **하드웨어 iLok 확보:** 서명 자격은 USB iLok에서만 동작한다는 보고가 있다(설계 문서 기준 "주문됨"). 도착 여부와 기기 시리얼을 기록한다(저장소에는 넣지 않음).
- [ ] **PACE 심사 회신 추적:** 기존 확인 일정으로 승인·거절·추가 정보·설치 안내를 확인한다. 새로운 기한은 실제 공식 회신에서 확인한다. 추가 회신·재신청·계정 생성·약관 동의·결제·설치·서명 자격 설정은 이 문서만으로 실행하지 않는다.
- [ ] **OS 서명 인증서:**
  - [ ] macOS: Apple Developer Program 가입, **Developer ID Application** 인증서, 공증용 App Store Connect API 키(또는 Apple ID + 앱 전용 암호)
  - [ ] Windows: Authenticode(OV/EV) 인증서 또는 서명 서비스. 설치 프로그램 서명용
  - PACE 서명과 OS 코드 서명은 별개이며 둘 다 필요하다.

## 2. 지금 할 수 있는 일 (개발, 승인 전)

- [ ] **빌드 확인:** 릴리즈 후보 소스에서 `aax-native.yml`이 통과하고, Windows x64·macOS Universal(arm64 + x86_64) `.aaxplugin`이 만들어진다.
- [ ] **Developer 빌드 점검(라이선스가 나오면):** 서명 없는 개발 패키지(`Tools/aax_native.py package/install --acknowledge-developer-only`)를 Pro Tools Developer에서 불러 스캔·삽입·오디오·자동화·세션 복원을 미리 확인한다. retail 승인을 대신하지 않는다.
- [ ] **서명 패키징 경로 준비:** 현재 `Tools/aax_native.py`는 서명 없는 개발 패키지만 다루고 `pace_signed: true`를 거부한다. 서명된 번들을 Windows·macOS 설치 프로그램의 표준 AAX 경로에 넣는 단계를 설계한다(구현은 서명 번들로 검증할 수 있을 때).
  - Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`
  - macOS: `/Library/Application Support/Avid/Audio/Plug-Ins/`
- [ ] **서명 기계 결정:** 첫 서명은 iLok을 꽂은 로컬 기계에서 수동으로 할지, 자체 호스팅 runner(`ilok-signing` 레이블)를 먼저 만들지 정한다. CI 자동화(`aax-sign.yml`)는 실제 자격 증명으로 한 번 수동 서명에 성공한 뒤에 만든다.

## 3. PACE 승인 후

- [ ] PACE 개발자 계정 로그인, **`wraptool` 설치**(Windows·macOS 서명 기계 각각)
- [ ] iLok을 AAX 서명용으로 **인증**
- [ ] 이 플러그인의 **WCGUID** 확인 후 릴리즈 기록에만 적기(저장소 제외)
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

- 서명 기록(빌드 SHA, 서명 시각, 도구 버전, verify 출력)을 증거 폴더에 남긴다. 자격 증명 값은 남기지 않는다.

## 5. 일반(retail) Pro Tools 검증

Developer 빌드가 아닌 **일반 Pro Tools**와 일반 사용자 수준의 iLok 인증으로 확인합니다. 서명 없는 AAX는 오류 **-7054**로 거부되고, 올바르게 서명되면 이 오류가 사라집니다.

| 항목 | Windows | macOS Apple Silicon | macOS Intel(지원 주장 시) |
|---|---|---|---|
| 플러그인 스캔 통과(AAX Native), -7054 없음 | [ ] | [ ] | [ ] |
| 모노·스테레오 트랙 삽입, 오디오 처리 | [ ] | [ ] | [ ] |
| 파라미터 자동화 기록·재생 | [ ] | [ ] | [ ] |
| 세션 저장·다시 열기, A/B·내장 IR 복원 | [ ] | [ ] | [ ] |
| 베타 수명주기 표시가 VST3/AU와 같음(새 만료일 없음) | [ ] | [ ] | [ ] |
| Pro Tools 버전·OS·테스터 환경 기록 | [ ] | [ ] | [ ] |

모두 통과하면 릴리즈 스케쥴과 범위 문서에서 AAX를 범위 안으로 옮기고 이 증거를 인용합니다.

## 6. 일정과 beta.2 영향

| 목표 (KST) | 할 일 | 막히는 조건 |
|---|---|---|
| 승인 전 | Validate 확인·접수 완료 상태를 유지하고 iLok 하드웨어·인증서 준비 상태 확인 | 준비 상태와 필요한 권한은 별도 확인 |
| 10/29 | 기존 Pro Tools Developer Bundle 재신청 일정 유지 | 최신 승인·중복 여부 확인 |
| 접수 후 심사 중 | PACE 후속 회신 확인(초대 전 7영업일로 날짜를 재산정하지 않음) | 외부 심사, 발급일 미확정 |
| 회신 후 2–3일 | `wraptool`·iLok 인증, macOS·Windows 첫 서명 | 하드웨어 iLok, 인증서 |
| 11/9–11/13 | retail Pro Tools 검증, beta.2 판정 | 위 단계 완료 |

- PACE 회신이나 iLok·인증서 준비가 늦어 11/13까지 §5를 끝내지 못하면, P0 AAX는 미달입니다. 그 경우 beta.2 공개를 미룰지, AAX를 뺀 범위로 낼지 판정 회의에서 결정합니다.
