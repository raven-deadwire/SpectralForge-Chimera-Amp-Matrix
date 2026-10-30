# Windows AAX Developer 테스트 — 2026-10-30 KST

**현재 결과: 준비 문서와 기존 CI 산출물 확인. Avid 등록·iLok 활성화·사용자 Windows 설치·실제 호스트 검증은 완료되지 않았습니다.**
이 절차는 기존 unsigned AAX를 이용한 개발 시험이며 PACE 서명이나 공개 배포 승인을 대신하지 않습니다.

## 1. 실제로 확인한 범위

| 구분 | 확인한 증거 | 상태 |
|---|---|---|
| PACE 관리자 및 도구 라이선스 | 10/27 08:15 KST 공식 메일: Central Access/Fusion SDK 각 1개, 2027-11-01 만료, AAX 서명 전용 | 발급 확인; 실제 ILM/USB 활성화 미확인 |
| Avid 개발 번들 | 10/29 01:45 KST Devauth 발급 메일을 10/30 직접 확인 | 코드 발급 확인; 등록 완료 아님 |
| Avid 계정 | 연결 브라우저에서 my.avid.com 로그인 화면 확인 | 로그인 필요; 코드 입력/제출 없음 |
| Avid 라이선스 3종 | 발급 안내에 Intro / Developer Build / Prerelease 명시 | 실제 지급/활성화는 UNKNOWN |
| Windows AAX 개발 패키지 | 아래 Actions run, 두 OS job의 성공 및 만료되지 않은 artifact 메타데이터 확인 | CI BUILT; 사용자 PC 설치/실행 NOT_RUN |
| 로컬 제어 | 웹 브라우저만 연결, Windows 네이티브 앱·파일시스템 직접 접근 불가 | ILM, USB 연결, Pro Tools 실행 및 청감 검증 불가 |

공개 문서에는 코드, 개인 이메일/메일 링크, 계정 ID, USB 시리얼, 라이선스 고유 ID, WCGUID, 개인키, 로그인 화면/계정 원문을 넣지 않습니다. 결과 공유 전 스크린샷과 로그의 식별자도 제거합니다.

## 2. Avid 등록과 ILM 확인

1. [Avid 계정](https://my.avid.com)에 **발급 대상 개발자 계정**으로 로그인합니다. 비밀번호는 채팅이나 문서에 입력하지 않습니다.
2. `Register Software With Code`에서 발급 메일의 코드를 사용합니다. 코드를 URL·터미널·GitHub/CI에 넣지 않습니다. 이미 등록됐다는 메시지가 나오면 반복 제출하지 말고 등록 제품과 ILM을 확인합니다.
3. 제품과 약관을 확인한 뒤 등록을 진행합니다. 약관 동의가 필요한 화면은 사용자가 확인합니다. 이어 표시되는 iLok 대상이 본인 계정인지 확인합니다. 계정이 다르면 등록을 중단하고 올바른 계정을 확인합니다.
4. 라이선스 설정 완료 화면을 확인합니다. 발급 메일에 따르면 Avid Link 열기 요청은 취소할 수 있습니다. 이 단계의 성공은 **Avid 등록 성공**으로만 기록합니다.
5. Windows **iLok License Manager**에 같은 iLok 계정으로 로그인합니다. `All Licenses`에서 아래 3종을 검색하고, `Available`과 `All Activations`도 확인합니다. 이미 활성화된 라이선스는 Available에 없을 수 있습니다.
6. 각 항목의 `Valid Locations`와 실제 활성화 위치를 확인합니다. 발급 안내는 USB 또는 iLok Cloud 사용을 허용하지만, 현재 라이선스의 허용 위치가 최종 기준입니다. Cloud를 쓸 경우 `File → Open Your Cloud Session`을 사용하며 다른 PC의 활성 세션을 임의로 옮기지 않습니다.
7. PACE Central Access는 **2세대 이상 USB iLok** 활성화가 필요합니다. Fusion SDK의 USB/Cloud 활성화 및 Pro Tools 호스트 라이선스와 구별합니다. USB 준비가 미확인이어도 호스트 라이선스의 Cloud 조건을 만족하면 Developer 시험 준비는 별도로 진행할 수 있습니다.

| 라이선스 | 목적 | 지급 확인 | 활성화/실행 확인 |
|---|---|---|---|
| Pro Tools Intro | 정식 배포 Pro Tools에서 플러그인 시험; unsigned AAX 허용 아님 | UNKNOWN | NOT_RUN |
| Pro Tools Developer Build | unsigned AAX 로드, 디버거 연결; 세션 저장 불가 | UNKNOWN | NOT_RUN |
| Pro Tools Prerelease | 별도 일반 Pro Tools 라이선스와 함께 prerelease 호스트 사용 | UNKNOWN | NOT_RUN |
| PACE Central Access | PACE Central 접속; USB 요구 | 메일상 지급 확인 | UNKNOWN |
| Fusion SDK | PACE 서명 도구 사용; USB/Cloud 지원 | 메일상 지급 확인 | UNKNOWN |

## 3. Windows 호스트 준비

1. [Avid 개발자 다운로드](https://my.avid.com/products/cppsdk)에서 Windows Developer 배포본과 함께 제공되는 `ReadMe.txt`를 확인합니다. 정확한 호스트 버전, 지원 Windows, 일반 Pro Tools와의 설치 관계는 이 배포본 기준으로 정합니다. 로그인 전에는 특정 버전을 추정하거나 비공개 다운로드 URL을 재배포하지 않습니다.
2. 해당 ReadMe에 따라 일반 Pro Tools와 Developer를 준비합니다. Avid 발급 메일의 필수 순서대로 **일반 Pro Tools를 먼저 한 번 실행한 후 Developer**를 실행합니다.
3. 오디오 인터페이스의 ASIO 드라이버를 선택하고 우선 48kHz / 128 samples로 설정합니다. 스피커/헤드폰 출력과 DI 입력 경로를 확인하고 중복 다이렉트 모니터링은 시험 결과와 구분합니다.
4. Developer는 세션 저장이 비활성화됩니다. 필요하면 unsigned Chimera 설치 전에 일반 호스트에서 DI 오디오가 들어 있는 빈 시험 세션을 만들고 저장해 둡니다. 원본 음악 작업 세션을 시험용으로 덮어쓰지 않습니다.

## 4. 이번에 특정한 기존 CI 패키지

| 항목 | 값 |
|---|---|
| 소스 | `340a6e081336c373f7bb3f6bf4ef44f823298797` (main의 해당 커밋) |
| 실행 | [AAX Native Development run 41037134450](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41037134450) |
| 시작 | 2026-10-30 18:57 KST |
| CI 결과 | Windows x64 / macOS Universal job 모두 success |
| Windows artifact | `AAX-UNSIGNED-DEVELOPMENT-windows-340a6e081336c373f7bb3f6bf4ef44f823298797` |
| 다운로드 | [Windows artifact 12968808790](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41037134450/artifacts/12968808790) (GitHub 로그인 필요) |
| 외부 artifact ZIP SHA-256 | `968db3bbee31d3528ee69927d7452204cf234d5f05a4dc31329415c19b0f8d68` |
| 조회 당시 만료 | false; 만료 예정 2027-01-28 18:57 KST |

CI에서 컴파일, 공유 파라미터/IR/베타/지연 회귀, Windows DLL export, 격리 및 일회용 시스템 경로의 개발 설치/갱신/제거 단계가 성공했습니다. 이것은 **CI의 해당 SHA 결과**입니다. 사용자 PC의 설치·백신/잠금·ASIO·Pro Tools 실행을 확인한 것이 아닙니다. PR #171의 코드나 향후 main에 이 성공을 자동 승계하지 않습니다.

외부 artifact에는 내부 개발 ZIP과 `.sha256`, CI 보고서가 있습니다. 바깥 ZIP의 digest와 안쪽 개발 ZIP의 checksum을 혼동하지 않습니다. CI는 qualification-only 설정을 요청하므로 `BETA_LIFECYCLE.txt`와 `chimera-beta-lifecycle.json`의 실제 정책을 읽고 시험 유효 범위를 기록합니다. 미승인 만료일을 바꾸거나 시스템 시간을 조작하지 않습니다. 이 패키지는 공개 배포 대상이 아닙니다.

### 다운로드 파일 검증·압축 해제 (사용자 Windows에서 실행)

아래는 **준비한 명령이며 이번 작업에서 Windows 실행하지 않았습니다**. PowerShell 5.1 이상, Python 3.9 이상이 필요합니다. 새 빈 작업 폴더를 지정하며 기존 파일을 덮어쓰지 않습니다.

```powershell
$ErrorActionPreference = 'Stop'
$aaxArchive = Read-Host '다운로드한 Windows artifact ZIP의 전체 경로'
$aaxStage = Read-Host '새로 만들 빈 시험 폴더의 전체 경로'
$aaxExpected = '968db3bbee31d3528ee69927d7452204cf234d5f05a4dc31329415c19b0f8d68'
if ((Get-FileHash -LiteralPath $aaxArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $aaxExpected) {
    throw 'Artifact checksum mismatch. Stop.'
}
if (Test-Path -LiteralPath $aaxStage) { throw 'Use a new empty staging path.' }
Expand-Archive -LiteralPath $aaxArchive -DestinationPath $aaxStage
$aaxPackages = @(Get-ChildItem -LiteralPath $aaxStage -Recurse -File -Filter '*-windows-AAX-UNSIGNED-DEVELOPMENT.zip')
if ($aaxPackages.Count -ne 1) { throw 'Expected exactly one inner Windows development ZIP.' }
$aaxPackageZip = $aaxPackages[0]
$aaxChecksumFile = $aaxPackageZip.FullName + '.sha256'
$aaxInnerExpected = ((Get-Content -LiteralPath $aaxChecksumFile -Raw).Trim() -split '\s+')[0]
if ($aaxInnerExpected -notmatch '^[0-9a-fA-F]{64}$') { throw 'Invalid package checksum.' }
if ((Get-FileHash -LiteralPath $aaxPackageZip.FullName -Algorithm SHA256).Hash -ne $aaxInnerExpected) {
    throw 'Inner package checksum mismatch. Stop.'
}
$aaxPackageDir = Join-Path $aaxStage 'package'
if (Test-Path -LiteralPath $aaxPackageDir) { throw 'Package destination already exists.' }
Expand-Archive -LiteralPath $aaxPackageZip.FullName -DestinationPath $aaxPackageDir
$aaxReceipt = Get-Content -LiteralPath (Join-Path $aaxPackageDir 'SpectralForge-Chimera-AAX-development.json') -Raw | ConvertFrom-Json
if ($aaxReceipt.source_sha -ne '340a6e081336c373f7bb3f6bf4ef44f823298797' -or $aaxReceipt.platform -ne 'windows') {
    throw 'Unexpected package source or platform.'
}
if ($aaxReceipt.pace_signed -cne $false -or $aaxReceipt.normal_host_accepted -cne $false -or $aaxReceipt.release_approved -cne $false) {
    throw 'Not the expected unsigned development package.'
}
Get-Content -LiteralPath (Join-Path $aaxPackageDir 'BETA_LIFECYCLE.txt')
python (Join-Path $aaxPackageDir 'aax_native.py') verify --bundle (Join-Path $aaxPackageDir 'SpectralForge Chimera.aaxplugin') --platform windows
if ($LASTEXITCODE -ne 0) { throw 'Bundle verification failed.' }
```

`verify`는 바이너리 구조/해시를 출력하며 PACE 서명이나 호스트 동작을 보증하지 않습니다. 뒤의 `install`은 모든 payload를 receipt와 대조한 뒤 씁니다. main 패키지에는 #171의 추가 `signing-preflight` 명령이 있다고 가정하지 않습니다.

### 개발용 설치 및 복구

모든 Pro Tools/DAW를 종료합니다. 위 검증이 성공한 **package 폴더**에서 관리자 PowerShell을 열어 실행합니다.

```powershell
python .\aax_native.py install --package . --acknowledge-developer-only
if ($LASTEXITCODE -ne 0) { throw 'Install failed; inspect the reported error/recovery path.' }
```

목적지는 `%CommonProgramW6432%\Avid\Audio\Plug-Ins\SpectralForge Chimera.aaxplugin`이며 development receipt를 함께 둡니다. 기존 번들이 다른 설치기 소유이거나 receipt/해시가 맞지 않으면 중단합니다. 이 경우 기존 번들을 지우거나 강제로 복사하지 않습니다. 실패 시 보고된 복구 경로를 보존합니다.

이후 **Pro Tools Developer**를 실행합니다. 일반 Pro Tools에서 unsigned AAX가 거부되는 것은 Developer 시험의 대체 결과가 아닙니다. 복구/제거가 필요하면 모든 호스트 종료 후 같은 package 폴더의 관리자 PowerShell에서 다음 명령을 실행합니다.

```powershell
python .\aax_native.py uninstall --platform windows --acknowledge-developer-only
if ($LASTEXITCODE -ne 0) { throw 'Uninstall failed; preserve recovery files.' }
```

## 5. 실제 호스트 시험 순서

다음은 제안 시험 기준이며 **모든 항목이 NOT_RUN**입니다. 사용자 Windows에서 실행하고 정확한 OS·Pro Tools Developer 버전·ASIO 드라이버·샘플레이트/버퍼·소스/패키지/바이너리 해시를 기록해야 결과로 전환합니다.

| 순서 | 시험 | 통과 기준 | 현재 |
|---|---|---|---|
| D1 | Developer 실행 및 AAX 스캔 | 호스트 edition이 Developer임을 확인하고 Chimera가 거부/충돌 없이 목록에 나타남 | NOT_RUN |
| D2 | 모노→모노, 스테레오→스테레오 트랙에 각각 삽입·제거 | 양쪽에서 에디터/파라미터 접근 가능, 재삽입 가능. multi-mono/HDX/AudioSuite는 지원 주장 제외 | NOT_RUN |
| D3 | 48kHz/128 samples에서 보유 기타/베이스 DI 60초 반복 재생 | 입력·출력 계측과 실제 소리 확인, bypass 대비 처리 변화, 예기치 않은 무음·채널 소실·크래시 없음. 입력 종류/프리셋과 관찰을 기록 | NOT_RUN |
| D4 | 호스트 자동화 목록에서 Input gain (`input`), Output gain (`output`) 활성화, Touch/Write로 한 번에 하나씩 10초 조작 | 타임라인 곡선이 실제 기록되고 Read로 되감아 재생할 때 노브/미터/소리가 함께 따라감. 단순 UI 움직임만으로 PASS 불가 | NOT_RUN |
| D5 | 동일 열린 세션에서 Output −18→−6→−18dB, Input −12→0→−12dB 자동화 반복 | 의도한 값과 왕복·반복 재생 일치, 다른 파라미터 오동작 없음. 고게인·리미터가 있으므로 출력 dB 선형성 자체를 통과 기준으로 삼지 않음 | NOT_RUN |
| D6 | 48kHz에서 64/128/256 samples; 이어 44.1/96kHz (장치/호스트가 허용하는 설정만) | 각 조건에서 재준비·오디오·bypass·자동화 재확인. 미지원 조합은 사유와 NOT_SUPPORTED, 과부하는 실패/제한으로 기록 | NOT_RUN |
| D7 | 에디터 10회 열기/닫기, 2개 인스턴스, 제거·세션/앱 종료 | 충돌·멈춤·예상치 않은 출력 없음 | NOT_RUN |
| D8 | Transpose on/off·품질 변경 후 보고 지연 및 PDC | 보고값과 실제 타이밍을 별도 측정; D1–D7 통과로 대신하지 않음 | NOT_RUN |
| R1 | 세션 저장·종료·재열기, A/B·IR/파라미터 복원 | **Developer 저장 미지원. signed AAX + 일반 Pro Tools에서 추후 시험** | BLOCKED |
| R2 | 최종 signed 설치기, 일반 호스트 스캔·realtime/offline bounce·플랫폼별 수용 | [배포 체크리스트](AAX_RELEASE_CHECKLIST.md)의 별도 게이트 | BLOCKED |

처음에는 Classic과 내부 CAB로 확인하고 이후 Dual/Matrix·개인 IR로 확장합니다. 오류가 나면 실제 오류 코드와 재현 설정을 남기며 관련 없는 플러그인/세션/프리셋을 삭제하지 않습니다. Developer에서 bounce/export가 제한되면 NOT_SUPPORTED로 기록하고 일반 호스트 검증으로 넘깁니다. 영상을 증거로 남길 때 계정·라이선스 화면을 함께 촬영하지 않습니다.

## 6. 시험 결과 기록 양식

```text
checked_at_kst: NOT_RUN
source_sha: 340a6e081336c373f7bb3f6bf4ef44f823298797
workflow_run: 41037134450
artifact_id: 12968808790
artifact_sha256: 968db3bbee31d3528ee69927d7452204cf234d5f05a4dc31329415c19b0f8d68
package_sha256: NOT_CHECKED_LOCALLY
binary_sha256: NOT_CHECKED_LOCALLY
windows_version / cpu: UNKNOWN
pro_tools_edition / version: UNKNOWN
interface / asio_driver: UNKNOWN
sample_rate_hz / buffer_samples: UNKNOWN
avid_redemption: NOT_COMPLETED_IN_THIS_TASK
ilok_intro / developer / prerelease_deposit: UNKNOWN
host_license_activation: UNKNOWN
pace_central_usb_activation / fusion_activation: UNKNOWN
D1 / D2 / D3 / D4 / D5 / D6 / D7 / D8: NOT_RUN
developer_session_save: NOT_SUPPORTED
signed_retail_session_restore: NOT_RUN
pace_verification: NOT_RUN
release_approved: false
errors / observations / sanitized_evidence: NONE
```

## 근거

- 10/27 PACE 관리자 생성·라이선스 지급 메일 및 10/29 Avid Devauth 개발 번들 발급 메일. 원문과 코드는 비공개로 유지.
- [iLok 공식 라이선스/활성화 안내](https://help.ilok.com/faq_licenses.html), [Cloud 안내](https://help.ilok.com/faq_cloud.html).
- [Avid AAX SDK Pro Tools Guide](https://learn-cdn.avid.com/AAX_SDK_2p1p1/Documentation/Doxygen/output/html/a00274.html)의 Developer 제한. 오래된 SDK 문서이므로 현재 설치 조건은 내려받은 빌드 ReadMe와 발급 메일을 우선.
- 위 Actions run과 동일 소스의 `.github/workflows/aax-native.yml`, `Tools/aax_native.py`, `Source/PluginProcessor.cpp`의 파라미터 정의.
