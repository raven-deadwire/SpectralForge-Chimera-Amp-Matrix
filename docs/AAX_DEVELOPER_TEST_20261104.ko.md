# AAX Developer 시험과 배포 경로 — 2026-11-04 KST

## 0. 현재 상태 (소유자 확인 기준)

| 항목 | 상태 | 근거 |
|---|---|---|
| Avid Pro Tools Developer 번들 | **해결** (소유자 보고, 2026-11-04) | 10/30 `invalid code` 문제가 풀렸다는 소유자 확인. iLok에 실제로 들어온 라이선스 3종(Intro / Developer Build / Prerelease)은 아래 1단계에서 직접 확인 |
| PACE 서명 도구 (Central Access, Fusion SDK) | 지급·USB 활성화 확인 (10/27, 10/30) | `AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md` |
| PACE Central 접속, WCGUID, `wraptool` 설치 | **미확인** | — |
| OS 코드서명 인증서 (Windows Authenticode, Apple Developer ID) | **없음** (소유자 확인, 2026-11-04) | — |
| 서명 안 된 AAX 개발 빌드 | CI 빌드 있음 | 아래 2단계 |
| Pro Tools에서 실제 로드·재생 | **미실행** | — |

이 문서는 개발 시험 절차다. PACE 서명, 일반(retail) Pro Tools 수용, 공개 배포 승인을 대신하지 않는다.

## 1. 지금 할 수 있는 것과 막힌 것

| 단계 | OS 인증서 없이 | 설명 |
|---|---|---|
| **A. Pro Tools Developer에서 서명 안 된 AAX 시험** | **가능** | Developer 빌드는 서명 안 된 AAX를 로드한다. 세션 저장은 안 된다 |
| B. PACE 서명(`wraptool`) | **확인 필요** | PACE Central의 `wraptool` 매뉴얼에서 서명 조건을 확인한다. 일반적으로 OS 코드서명 인증서를 함께 요구하는 것으로 알려져 있으나, 이 문서 작성 시점에 매뉴얼로 확인하지 않았다 |
| C. 일반 Pro Tools에서 로드 (공개 배포) | **막힘** | B가 끝나야 가능하다. 서명 안 된 AAX는 일반 Pro Tools에서 거부된다 |
| D. macOS 공개 배포 | **막힘** | Apple Developer ID 서명과 공증(notarization)이 없으면 macOS가 다운로드한 플러그인·설치기를 막는다 |
| E. Windows 공개 배포 | 인증서 없이도 설치는 되지만 경고 | Authenticode 서명이 없으면 SmartScreen이 설치기를 경고한다 |

**권장 순서:** 지금 A를 진행하면서, B의 조건을 PACE 매뉴얼로 확인하고, 필요한 인증서를 준비한다.
- Apple: Apple Developer Program 가입 → Developer ID Application 인증서 → `notarytool`용 App Store Connect API 키
- Windows: 코드서명 인증서. 현재 대부분의 인증기관은 하드웨어 토큰이나 클라우드 HSM으로 발급한다. 발급 형태가 서명 기계 구성에 영향을 준다

## 2. 시험할 빌드 (main `d90511c`)

| 항목 | Windows | macOS |
|---|---|---|
| 소스 | `d90511c9520af565dadc12c47793b8bdffd4c4bb` | 같음 |
| CI 실행 | [AAX Native Development 41655816566](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41655816566) | 같음 |
| artifact | [13240628486](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41655816566/artifacts/13240628486) | [13242740376](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41655816566/artifacts/13242740376) |
| 바깥 ZIP SHA-256 (GitHub 표시값) | `b375dbe2e11c8bf2eea40f5cbc4560c83f990803f13f731dd0abe3c62ee638ca` | `6b20a675037449f4856dfddca893acd142f5c3397a294622565bd5e61965e269` |
| 만료 | 2027-02-01 | 2027-02-01 |

다운로드는 GitHub에 로그인한 상태에서 위 artifact 링크를 연다. 이 환경에서는 artifact를 내려받지 못해 해시를 GitHub API 값으로만 확인했다.

## 3. Avid·iLok 확인 (소유자)

1. **iLok License Manager**에 로그인해 `All Licenses`에서 Pro Tools **Developer Build**, **Intro**, **Prerelease**가 보이는지, 어디(USB/Cloud)에 활성화됐는지 확인한다.
2. 개발자 다운로드에서 Windows(또는 macOS)용 **Pro Tools Developer**와 함께 제공되는 `ReadMe`를 읽고, 지원 OS와 일반 Pro Tools 설치 관계를 확인한다.
3. Avid 안내대로 **일반 Pro Tools를 먼저 한 번 실행한 뒤 Developer**를 실행한다.
4. 오디오 인터페이스(ASIO / Core Audio)를 고르고 48 kHz / 128 samples로 맞춘다.
5. Developer는 세션 저장이 안 된다. 필요하면 일반 Pro Tools에서 DI 오디오가 든 빈 시험 세션을 미리 만들어 둔다.

라이선스 코드, 계정 ID, iLok 시리얼은 채팅·GitHub·로그에 넣지 않는다.

## 4. 내려받은 파일 검증과 설치

### Windows (PowerShell, Python 3.9+)

```powershell
$ErrorActionPreference = 'Stop'
$zip   = Read-Host '내려받은 artifact ZIP 전체 경로'
$stage = Read-Host '새로 만들 빈 시험 폴더 전체 경로'
if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'b375dbe2e11c8bf2eea40f5cbc4560c83f990803f13f731dd0abe3c62ee638ca') { throw 'Artifact checksum mismatch. Stop.' }
if (Test-Path -LiteralPath $stage) { throw 'Use a new empty folder.' }
Expand-Archive -LiteralPath $zip -DestinationPath $stage
$inner = @(Get-ChildItem -LiteralPath $stage -Recurse -File -Filter '*-windows-AAX-UNSIGNED-DEVELOPMENT.zip')
if ($inner.Count -ne 1) { throw 'Expected one inner development ZIP.' }
$expected = ((Get-Content -LiteralPath ($inner[0].FullName + '.sha256') -Raw).Trim() -split '\s+')[0]
if ((Get-FileHash -LiteralPath $inner[0].FullName -Algorithm SHA256).Hash -ne $expected) { throw 'Inner checksum mismatch. Stop.' }
$pkg = Join-Path $stage 'package'
Expand-Archive -LiteralPath $inner[0].FullName -DestinationPath $pkg
$r = Get-Content -LiteralPath (Join-Path $pkg 'SpectralForge-Chimera-AAX-development.json') -Raw | ConvertFrom-Json
if ($r.source_sha -ne 'd90511c9520af565dadc12c47793b8bdffd4c4bb' -or $r.platform -ne 'windows') { throw 'Unexpected source or platform.' }
Get-Content -LiteralPath (Join-Path $pkg 'BETA_LIFECYCLE.txt')
python (Join-Path $pkg 'aax_native.py') verify --bundle (Join-Path $pkg 'SpectralForge Chimera.aaxplugin') --platform windows
```

모든 DAW를 닫고, `package` 폴더에서 **관리자** PowerShell로:

```powershell
python .\aax_native.py install --package . --acknowledge-developer-only
# 제거:  python .\aax_native.py uninstall --platform windows --acknowledge-developer-only
```

설치 위치: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\SpectralForge Chimera.aaxplugin`

### macOS (Terminal, Python 3.9+)

```bash
set -euo pipefail
ZIP="$HOME/Downloads/<내려받은 artifact>.zip"     # 실제 경로로 바꾼다
STAGE="$HOME/chimera-aax-test-$(date +%Y%m%d%H%M)"
[ "$(shasum -a 256 "$ZIP" | cut -d' ' -f1)" = 6b20a675037449f4856dfddca893acd142f5c3397a294622565bd5e61965e269 ] || { echo 'checksum mismatch'; exit 1; }
mkdir "$STAGE" && ditto -x -k "$ZIP" "$STAGE"
INNER=$(find "$STAGE" -name '*-macos-AAX-UNSIGNED-DEVELOPMENT.zip'); [ -n "$INNER" ]
[ "$(shasum -a 256 "$INNER" | cut -d' ' -f1)" = "$(cut -d' ' -f1 "$INNER.sha256")" ] || { echo 'inner checksum mismatch'; exit 1; }
mkdir "$STAGE/package" && ditto -x -k "$INNER" "$STAGE/package"
cd "$STAGE/package"
grep -q '"source_sha": "d90511c9520af565dadc12c47793b8bdffd4c4bb"' SpectralForge-Chimera-AAX-development.json
cat BETA_LIFECYCLE.txt
python3 aax_native.py verify --bundle "SpectralForge Chimera.aaxplugin" --platform macos
sudo python3 aax_native.py install --package . --acknowledge-developer-only
# 제거:  sudo python3 aax_native.py uninstall --platform macos --acknowledge-developer-only
```

설치 위치: `/Library/Application Support/Avid/Audio/Plug-Ins/SpectralForge Chimera.aaxplugin`.
macOS 빌드는 ad-hoc 서명만 있다. Gatekeeper 격리(quarantine) 때문에 로드가 막히면 그 메시지를 그대로 기록한다. 격리 해제 같은 우회는 기록과 함께 별도로 판단한다.

## 5. Pro Tools Developer 시험 항목

결과마다 OS·Pro Tools Developer 버전·오디오 장치·샘플레이트/버퍼를 함께 기록한다.

| 번호 | 시험 | 통과 기준 |
|---|---|---|
| D1 | Developer 실행, 플러그인 스캔 | 창 제목·About에서 Developer edition 확인, Chimera가 오류 없이 목록에 나타남 |
| D2 | 모노 트랙과 스테레오 트랙에 각각 삽입·제거·재삽입 | 에디터와 파라미터 접근 가능, 충돌 없음 |
| D3 | 48 kHz / 128에서 기타·베이스 DI 60초 반복 재생 | 소리가 처리되고, bypass와 차이가 있으며, 무음·채널 소실·크래시 없음 |
| D4 | 자동화: Input gain, Output gain을 Touch/Write로 10초씩 | 곡선이 기록되고, Read로 재생할 때 노브·미터·소리가 따라감 |
| D5 | 64 / 128 / 256 samples, 44.1 / 48 / 96 kHz (장치가 허용하는 범위) | 각 조건에서 소리·bypass·자동화 정상 |
| D6 | 에디터 10회 열기/닫기, 인스턴스 2개, 앱 종료 | 충돌·멈춤 없음 |
| D7 | Transpose on/off 후 지연 보고와 PDC | 보고값과 실제 타이밍을 따로 측정 |

결과는 번호별로 PASS / FAIL / NOT_RUN과 관찰 내용을 적어 공유한다. 크래시가 나면 Pro Tools 크래시 리포트 위치와 마지막 동작을 함께 남긴다.

## 6. 그다음 (PACE 서명)

1. PACE Central에 접속해 **WCGUID**(이 플러그인의 서명 ID)를 만들고, 최신 `wraptool`과 매뉴얼을 받는다.
2. 매뉴얼에서 플랫폼별로 필요한 OS 코드서명 인증서와 `wraptool sign` / `verify` 옵션을 확인한다.
3. 인증서를 준비한 뒤, `AAX_FIRST_SIGNING_RUNBOOK_20261027.ko.md`(#171)와 `AAX_SIGNING_CI_DESIGN.md`의 순서대로 첫 서명을 한다.
4. 서명한 빌드를 **일반 Pro Tools**에서 D1–D7과 세션 저장/다시 열기까지 확인한다. 그 결과가 있어야 공개 배포 판정을 한다.
