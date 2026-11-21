# Windows 배포 서명 안내서 (AAX + 설치본) — 2026-11-19 KST

**상태:** PACE AAX 서명은 소유자 PC에서 성공했습니다. 그 서명본이 같은 PC의 일반 Pro Tools에서 동작하는 것도 확인했습니다(소유자 보고). 남은 것은 **다른 사람의 Windows에서도 신뢰되는 Authenticode 인증서**입니다. 인증서를 구매·신청하기 전이므로 Windows 배포는 아직 **BLOCKED**입니다.

이 문서는 다음 단계를 한 곳에 모은 안내서입니다. 기존 문서와의 관계는 다음과 같습니다.

- PACE 계정·iLok·Pro Tools 준비 이력: [AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md)
- 첫 수동 서명 절차와 증거 분리 원칙: [AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md](AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md)
- 설치본 서명 스크립트와 SmartScreen 설명: [WINDOWS_SIGNING.md](WINDOWS_SIGNING.md)
- 일반 Pro Tools 수용 게이트: [AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md)

**비공개 원칙(기존과 동일):** 계정 ID, 개인 메일, 실명이 들어간 인증서 정보, 인증서 지문, iLok 시리얼, WCGUID, eSigner 자격 증명·TOTP 비밀값, 암호·토큰, 원시 vendor 로그는 저장소·PR·CI 로그에 남기지 않습니다.

## 1. 결정 사항 (2026-11-19 기준)

| 항목 | 결정 | 이유 |
|---|---|---|
| 인증서 종류 | **SSL.com 개인용 IV(Individual Validation) 코드 서명** | 사업자등록이 아직 없습니다. IV는 사업자 없이 개인 신원으로 발급됩니다. |
| 개인키 보관 | **eSigner 클라우드 서명 + eSigner CKA**(1순위). USB 토큰은 먼저 사지 않습니다. | 공개 CA 코드 서명 키는 하드웨어/클라우드 HSM 보관이 의무입니다. CKA는 클라우드 키를 Windows 인증서 저장소에 붙여 SignTool이 일반 인증서처럼 쓰게 합니다. |
| EV / OV | 지금은 하지 않음 | Pro Tools·PACE는 EV를 요구하지 않습니다. OV는 조직 명의가 필요합니다. |
| Microsoft Artifact Signing | 사용 불가 | 공개 신뢰 등록이 미국·캐나다·EU·영국 조직과 미국·캐나다 개인으로 제한됩니다([WINDOWS_SIGNING.md](WINDOWS_SIGNING.md)). |
| 게시자 명칭 | **인증서의 법적 실명**이 표시됨 | IV는 개인 실명을 검증합니다. "RavenForge Luthier Intelligence"로 표시하려면 사업자 등록 후 조직 명의 인증서를 따로 받아야 합니다. |

실명 공개가 괜찮을 때만 IV로 진행합니다. 실명이 아니라 RavenForge 명의로 내야 한다면 지금 인증서를 사지 않고, 사업자 등록 후 조직 명의로 발급받습니다.

**비용(참고, 결제 전 재확인):** 상담 시점 조사로 IV 인증서 연 $129, eSigner 월 $20 또는 연 $180이었습니다. 연 결제 기준 합계 약 $309입니다. eSigner 요금제에는 **월 서명 횟수 한도**가 있으므로 2.3의 횟수 계산과 함께 고릅니다.

## 2. 결제 전 확인

### 2.1 SSL.com 지원팀 문의 (필수)

eSigner CKA가 SignTool을 지원한다는 공식 안내는 있습니다. 하지만 **PACE `wraptool`이 SignTool을 호출하는 경로**에서 동작한다는 공식 확인은 없습니다. 결제 전에 아래 문의를 보냅니다. 신분증이나 계정 정보는 보내지 않습니다.

> IV Code Signing with eSigner: can you confirm whether eSigner CKA works with PACE AAX Signing Only via `wraptool` 6.0.2? My workflow has `wraptool` call Microsoft SignTool with `--signtool`, use `--signid` to select the certificate by thumbprint, and sign the Windows x64 AAX plug-in binary in place. Is this supported with CKA, including timestamping and any interactive authentication prompts? Can CKA's automatic signing (TOTP registered in CKA) run without a prompt for each signature? How are signings counted against the eSigner plan when SignTool signs one binary per call? If CKA is not suitable, which SSL.com IV key-storage option do you recommend for this workflow?

### 2.2 답변별 진행

- **"지원한다"** → 3장으로 진행합니다.
- **"지원하지 않는다" / 불명확** → 결제하지 않습니다. SSL.com이 권하는 다른 보관 방식(예: USB 토큰)을 그때 비교합니다.

### 2.3 서명 횟수 계산

릴리스 1회당 최소 서명 수(Windows):

1. AAX 플러그인 바이너리 1회(`wraptool`)
2. 설치본 안의 EXE/DLL/VST3 각 1회(`Tools/Sign-WindowsArtifact.ps1`가 모두 서명)
3. Inno가 만드는 언인스톨러 1회 + Setup 1회

재빌드·재서명하면 다시 셉니다. 베타 기간의 예상 릴리스 수 × 위 개수로 eSigner 요금제 한도를 고릅니다.

## 3. 발급과 서명 PC 준비

1. SSL.com에서 IV 코드 서명 + eSigner를 신청합니다. 한국 거주 개인은 여권·사진이 있는 운전면허증 등으로 신원 확인을 받습니다. 주소 증빙을 추가로 요청받을 수 있습니다.
2. 발급 후 서명용 Windows PC에 **eSigner CKA**를 설치하고 로그인합니다. 자동 서명을 쓰려면 CKA에 TOTP를 등록합니다. 이 비밀값은 비밀번호 관리자에만 보관합니다.
3. PowerShell에서 인증서가 보이는지 확인합니다. 출력된 지문은 화면에서만 사용하고 저장소에 적지 않습니다.

   ```powershell
   Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Format-List Subject, Thumbprint, NotAfter
   ```

4. 테스트 파일 하나로 SignTool 서명·타임스탬프·검증이 되는지 먼저 확인합니다.

   ```powershell
   signtool sign /sha1 <지문> /s My /fd SHA256 /tr http://ts.ssl.com /td SHA256 test.exe
   signtool verify /pa /v test.exe
   ```

   타임스탬프 서버는 SSL.com 안내를 따릅니다. 기존 스크립트 기본값(`http://timestamp.digicert.com`)도 RFC 3161이라 사용할 수 있습니다.

## 4. 릴리스 서명 순서

깨끗한 원본 빌드만 서명합니다. 기존 런북 §3의 "검증한 unsigned 원본 보존, 복사본에서 서명" 원칙을 그대로 따릅니다.

1. **후보 빌드 확인:** CI artifact의 소스 SHA와 체크섬을 대조합니다. 그다음 `python Tools/aax_native.py signing-preflight --package <압축 해제본> --platform windows --expected-source <40자리 SHA> --require-approved-beta`를 실행합니다.
2. **AAX PACE 서명:** 소유자 PC에서 이미 성공한 `wraptool` 설정을 그대로 쓰되, 인증서만 새 IV 인증서로 바꿉니다.

   ```text
   wraptool sign --verbose --account <PACE 계정> --wcguid <WCGUID> --signtool signtool --signid <IV 인증서 지문> --in <Chimera.aaxplugin> --out <Chimera.aaxplugin>
   ```

   계정·WCGUID·지문은 명령 기록이나 로그에 남기지 않습니다. 정확한 옵션은 설치된 PACE 문서 기준으로 확정합니다.
3. **AAX 검증:** `wraptool verify`와 `signtool verify /pa`가 모두 통과하는지 확인합니다.
4. **설치본 서명:** 실명 인증서이므로 **`-ExpectedPublisher`에 인증서 CN(법적 실명)을 정확히 넘겨야 합니다.** 기본값 "RavenForge Luthier Intelligence"와 다르면 스크립트가 의도대로 서명을 거부합니다. 실명은 명령줄에서만 입력하고 저장소 기본값은 바꾸지 않습니다.

   ```powershell
   $env:CHIMERA_SIGNING_THUMBPRINT = '<IV 인증서 지문>'
   ./Tools/Build-WindowsInstaller.ps1 -Stage C:/Chimera/validated-package -OutputDirectory C:/Chimera/signed -Sign -ExpectedPublisher '<인증서 CN>'
   ```

   이 스크립트는 EXE/DLL/VST3를 서명·검증합니다. 그다음 Inno가 언인스톨러와 Setup을 같은 인증서로 서명합니다. 타임스탬프나 게시자 불일치가 있으면 중단합니다.
5. **다른 PC에서 검증:** 개인키 없는 Windows PC에서 `-VerifyOnly`로 확인합니다. 설치본 속성의 "디지털 서명" 탭에 실명과 타임스탬프가 보이는지도 확인합니다.
6. **일반 Pro Tools 최종 확인:** 서명된 설치본을 깨끗한 상태에서 설치합니다. 그다음 일반 Pro Tools에서 스캔, 삽입, 오디오, 자동화, 세션 저장·다시 열기를 확인합니다([AAX_RELEASE_CHECKLIST.md](AAX_RELEASE_CHECKLIST.md)).
7. 공개 증거에는 후보 SHA, 서명 전/후 해시, 도구 버전, 검증 결과 요약만 남깁니다.

## 5. 설치 후 사용자에게 보이는 것

- "알 수 없는 게시자" 대신 인증서의 **실명**이 게시자로 표시됩니다.
- 새 인증서·새 파일은 다운로드 이력이 쌓이기 전까지 **SmartScreen 경고가 뜰 수 있습니다.** 이는 IV 인증서라서 생기는 문제가 아닙니다. Microsoft는 2024년에 EV 인증서의 SmartScreen 즉시 통과를 없앤 것으로 알려져 있으며, 공개 전에 Microsoft 문서로 다시 확인합니다. 사용자에게 자체 서명 루트를 설치하게 하거나 보호 기능을 끄라고 안내하지 않습니다.
- 설치 프로그램 목록의 "게시자"는 `Installer/Chimera.iss`의 `AppPublisher` 값입니다. 이 값은 디지털 서명이 아닌 표시용 메타데이터이며, 서명 게시자와 다를 수 있습니다.

## 6. 체크리스트

- [x] PACE AAX 서명 성공, 소유자 PC의 일반 Pro Tools에서 동작(소유자 보고)
- [ ] 게시자 명의 결정: 개인 실명 공개 OK → IV / 아니면 사업자 등록 후 조직 명의
- [ ] SSL.com 지원팀에 2.1 문의 발송, 답변 수신
- [ ] 서명 횟수 계산 후 eSigner 요금제 선택
- [ ] IV 신청·신원 확인·발급
- [ ] eSigner CKA 설치, 자동 서명 설정, 테스트 서명·타임스탬프·검증
- [ ] 후보 빌드 preflight → AAX `wraptool` 서명·검증
- [ ] 설치본 서명(`-ExpectedPublisher` = 인증서 CN)·다른 PC `-VerifyOnly`
- [ ] 서명 설치본으로 일반 Pro Tools 최종 확인
- [ ] 공개 증거 정리 후 배포

macOS(Apple Developer ID·공증)는 이 문서 범위 밖이며 [준비 체크리스트 §1](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md)에서 따로 추적합니다.
