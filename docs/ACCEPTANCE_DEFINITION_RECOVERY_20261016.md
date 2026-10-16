# beta.3 수용검사 정의 복구 조사 — 2026-10-16 KST

**원래 A-ID별 정의의 일대일 복구는 0/86이다. 대신 86개 전부에 구체적인 대체 명세·기존 증거·미충족 범위를 작성했다.**
원래 조건의 일부인 계산식, 경계값, Signature 구조, 개인 IR 무결성 의도는 찾았다.
그 조건을 어느 번호에 배정했는지 확인되지 않으므로, 아래 새 번호 연결은 **검토안이며 승인되지 않았다.**
고정 beta.3의 원본 게이트는 **21 PASS / 107 BLOCKED / 0 N/A**, 새 PASS·면제는 각각 0이다.

- [86행 상세 조건](evidence/acceptance-definitions-20261016/DEFINITIONS_KO.md)
- [CSV](evidence/acceptance-definitions-20261016/definitions.csv) / [정의·고정 소스·증거 원장 JSON](evidence/acceptance-definitions-20261016/definition-proposals.json)
- [Git 이력 조사](evidence/acceptance-definitions-20261016/history-audit.json) / [실제 재검산 결과](evidence/acceptance-definitions-20261016/audit-result.json)

## 1. 조사 범위와 원래 정의를 찾지 못한 근거

제품 증거의 기준은 PR #62 `5c858442d0ed8360703e0ce63241a567736608a9`,
이번 변경의 부모는 PR #78 `921a9aed8dc3889018a9456f19784abefd57f8b0`다.
193개 저장 ref에서 도달 가능한 608개 커밋, 현재·삭제된 텍스트 파일 변경,
원래 정책/번호 도입, 테스트·릴리스 문서 및 관련 GitHub 이슈/PR 기록을 대조했다.
ref/commit 목록과 SHA-256, 실제 검색 명령·일치 파일은 evidence 폴더에 보존했다.

| 근거 | 확인 결과 |
|---|---|
| [`c499d4c`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/c499d4cfcda82d87b57023757536e79287232d7e), 10월 1일 | 단계명만 도입. 개별 assertion 없음. |
| [`11ee241`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/11ee2419faeecab5bce919965051deef2391a88f), 10월 1일 | policy 파일 하나에서 ID 목록과 hard-gate membership 추가. 86개 ID 순서는 현재와 동일하지만 조건·owner·producer는 없음. |
| [`735eaa6`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/735eaa6a6419bc105e43f4bb7983f8ddaf21ccb1), 10월 4일 | 미소유 ID를 assertion=null, unclaimed-baseline/BLOCKED로 생성. 테스트를 정의한 커밋이 아님. |
| [Issue #21](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/21), [PR #23](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/23) | 21개 자동 producer와 나머지 86개 번호별 정의 누락을 명시. generic A2.01 fixture는 실제 Signature assertion이 아님. |
| [PR #41](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/41), `07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46` | 393커밋 선행 조사에서 같은 결론. 원래 계산식과 13개 calculator 계약을 복구했으나 미병합이며 beta.3에 포함되지 않음. |
| 10월 16일 확대 검색 | 이후 일치는 generated status, fixed-gap publication inventory, 선행 누락 감사 기록. 새 원본 ID 정의는 확인되지 않음. GitHub 이슈 검색 A1.01/A5.01/A9.01도 결과 없음. |

이는 **검색한 범위에서 미확인**이라는 결론이다. 삭제되어 도달 불가능한 커밋,
제공되지 않은 원래 외부 체크리스트, 검색하지 않은 binary-only 문서에 원본이 없다고 단정하지 않는다.
외부 원본이 나중에 발견되면 제안과 대조하고 충돌을 보존한다.

## 2. 복구 가능한 계약과 대체 제안의 구분

| 단계 | 수 | 찾은 계약 / 새 제안의 범위 |
|---|---:|---|
| A1 | 6 | 버전·패키지 일치, 구성/경로, 실제 beta.2 업그레이드, repair, uninstall, assembly. 기존 Windows raw receipt에 연결. |
| A2 | 8 | Bass Signature 3종 identity/Matrix/PRE/LOW/곡별 crossover·amp. 현재 modeled CAB 및 기존 level 한계를 명시. |
| A3 | 9 | APVTS completeness, dirty recall, binary/A-B, synthetic audio delta, host ID, legacy/expiry/IR/UI. Guitar full snapshot과 Bass performance 보존의 차이를 명시. |
| A4 | 5 | 기존 preset ID, performance 보존, 48종 완전 recall, saved state 재보이싱 방지, 음향 안전. |
| A5/A6 | 16 | 아래 결정안: modeled factory CAB와 명시적 Bass/Guitar 개인 IR import·복원·보존의 동시 검증. |
| A7/A8 | 11 | audio/archive hash, provenance, public allowlist, 상태 무결성, 원자적 실패, missing/invalid/embedded fallback, source isolation. |
| A9 | 9 | 원래 raw delta → bias → centering → RMSE → pair order/gap → weighted score 식. ordinal 연결은 새 제안. |
| A10 | 6 | H1–H4의 정확한 수치, secondary 3/4, overall 85/critical .70. 어떤 threshold도 변경하지 않음. |
| A11 | 9 | JSON 구조/숫자/결과 계약 및 누락된 strict parsing·provenance·CLI 실패 산출물 처리 제안. |
| A14 | 7 | 버전/언어/카탈로그/CAB·IR/검증 상태/패키지 문서/출시 제한의 실제 내용 일치. 파일 존재만으로 통과 불가. |

제안 근거의 분류는 **기존 동작에서 구성한 새 연결 53 / 현행 구조로의 대체 28 / 새 증거 프로토콜 보강 5**다.
53개를 원래 번호별 정의가 복구된 것으로 해석하면 안 된다. 원래 mapping/owner/producer는 모두 null이다.
전체 86행에 재현 조건, 수치 또는 비교 기준, 고정 source URL·hash, 기존 실행 증거와 남은 범위를 넣었다.

## 3. A5/A6 및 A7/A8의 충돌 해결안

### 과거 계약을 보존할 부분

과거 [`40d5e8a5`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/40d5e8a5ea466eaccc1044ccefce1bd33e6ac222/docs/VALIDATION_STATUS.md)의 Signature 기본값은 다음 실제 파일을 자동 탐색했다.

| 과거 범위 | WAV와 SHA-256 | pack SHA-256 |
|---|---|---|
| Wild Hunt MID / Hartke | `Hartke HyDrive 410 _ SM57.wav` — `caa3be009191b2cd6c2ad1a711fe0f7eddd74c9115e991ab2032159405699d0b` | `c95e39e488d293cc786723116e27c590fae8016f7a806749435b3c823a39b50e` |
| Azhi Dahaka HIGH / Marshall | `Marshall G12 1 SM57 3.wav` — `d55582bb1f6ed27e0ee03e4ea71006968c2eb36cba93b55338f86b45d42c5cb1` | `c78de95fac7d5a4874c77e2bde186f68aefbe8068f043abd7e7f2c7f1b21c9ae` |

[`ca0c976`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/ca0c976443c2dc615afe6555fd36bd36b98c1c89)는 metadata/예상 filename을,
[`0259cb7`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/0259cb7e43aa164427a4bed7aeb03a41495be233)는 hash/sidecar stamping을 검사했다.
후자는 **합성 WAV에 provenance 필드를 쓰는 시험**이며 실제 원본 pack의 성공적인 import 증거가 아니다.
`approved_pack`이라는 당시 내부 명칭만으로 재배포권을 확인한 것도 아니다.
과거 missing/hash mismatch→Filters-only 동작은 해당 과거 factory path에 한정된다.

### 현재 제품과 정합적인 대체 명세

[`4c7688d`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/4c7688d78869e113a041cb7316f4d23d5ea1a02c)에서 48개 공장 프리셋에 modeled CAB를 적용하고
private 자동 탐색·approved-pack catalog routes를 제거했다. beta.3도 이 구조다.

| 대상 | 검토할 조건 | 재사용 증거 / 남은 부분 |
|---|---|---|
| Factory Wild/Azhi | private 파일 유무와 상관없이 authored CAB/speaker/mic를 선택. empty target/hash, private User IR 자동 선택 금지. | Factory/Integrated의 기존 검사. private 이름의 정상/변조 파일을 넣은 직접 비교 사례는 추가 연결 필요. |
| 수동 개인 Bass/Guitar IR | OPEN IR/ADD FOLDER, WAV/AIFF decode, 명시적 type/Unspecified, 정확한 bytes·sidecar, rig source=3 선택. | 기존 IRCollection/IRLibrary/installed companion 검증. 모든 AIFF/경계값 coverage는 아직 미확인. |
| 기존 프로젝트·A/B | 저장된 source ordinal 0/1/2/3, personal/embedded IR·metadata·route 보존. factory revoicing을 saved state에 적용하지 않음. | lifecycle/state/source-isolation 실행은 재사용. 실제 Hartke/Marshall 오디오를 포함한 사용자 DAW 프로젝트는 별도 필요. |
| 누락/변조 | invalid 파일은 ready가 아님. 외부 경로가 없어도 유효 embedded IR로 복원. 둘 다 무효일 때의 결과를 명시하고 검증. | 앞의 두 조건에는 부분 실행 증거. 마지막 정상 상태 보존과 양쪽 무효 경로의 직접 negative fixture는 남음. |
| 배포·설치 보존 | public allowlist의 두 IR만 동봉. 개인 데이터는 upgrade/repair/remove 후 보존. | 기존 raw Windows/분포 계약. byte sentinel을 실제 IR 청음·DAW 복원으로 확대하지 않음. |

**제안 결정:** A5의 8개는 Bass 개인 IR과 Wild factory 계약, A6의 8개는 Guitar 개인 IR과 Azhi factory 계약으로 명시한다.
각 8개는 factory 독립성 / 명시적 선택 / 분류 / bytes·metadata / invalid 거부 / saved-state 호환 /
비파괴 제거·재등록 / 배포·업그레이드 경계다. 원래 16개 ID와 hard membership는 그대로 보존한다.
A7/A8도 같은 구조에 맞춰 무결성·fallback을 보강한다. 이는 아직 **승인되지 않은 명세 대체안**이다.

원래 특정 Hartke/Marshall 파일 자체의 수용을 계속 요구한다면, 위 역사적 hash의 사용자 보유 입력과
해당 old-state fixture를 별도 비공개 검증에 연결해야 한다. 현재 모델링 CAB 시험으로 그 검증을 대체하거나 N/A 처리하지 않는다.
과거 자동 탐색/다운로드 UI를 복원하거나 private 오디오를 공개 저장소에 추가할 필요는 없다.

## 4. 실제로 재사용한 증거

| 증거 | 식별·검증 | 적용 한계 |
|---|---|---|
| 기존 원본 strict gate | run `38386305674`, attempt 1, product `5c858442…`, artifact `11812195536`, archive SHA-256 `d34d6e1ac9335a5c6daf66b043f50818e9585e99d04b2bf8722482ff9e25bc14` | raw bundle/hash/정책을 다시 계산해 **21/107** 재현. 이 조사 브랜치의 새 바이너리 인증이 아님. |
| 3OS 계약·패키징 | 위 archive의 JUnit + LastTest.log + install/assembly. 필요한 case의 platform/test/status/hash 및 원시 assertion 발췌를 원장에 연결 | Windows 51/macOS 47/Linux 48 configured CTest는 이미 실행된 기록. suite PASS를 미정의 ID의 PASS로 복사하지 않음. |
| 실제 beta.2→beta.3 Windows | run `38777238017`, attempt 1, tool `921a9aed…`, product `5c858442…`, artifact `11993179895`, SHA-256 `8f000f9cd812e005a49f581a633ae571408c439318751b3fe3b873c9fedf7417` | 5개 assertion과 4회 실제 Setup 호출을 원본 ZIP으로 재사용. qualification-only·DAW=false 유지. |
| PR #41 calculator | run `38057692032`, attempt 1, source `07dbfa5…`, artifact `11672138749`, SHA-256 `77ae24e77059f103efc5690a2579912b1404fd6c29117e6fdd3a292f8d95037f` | **13 component cases PASS**. 입력 source·output/log hash 검증. **전체 Product workflow는 failure**이며 beta.3와 evaluator가 다름. |

총 47개 고정 source 파일 및 16개 증거 레코드로 연결했다. 두 작은 원본 ZIP을 보존했고 기존 원본 gate ZIP은 PR #78의 파일을 그대로 참조한다.
이미 완료된 native build·Windows 설치·calculator 실행을 반복하지 않았다.
같은 bytes의 역사적 증거는 재사용하지만, 승인된 만료일이 들어간 새 최종 바이너리에 이전 PASS를 자동 이식하지 않는다.

## 5. 추가로 드러난 미충족 사항

1. **A11.03:** beta.3 `finite()`는 `isinstance(x,(int,float))`여서 `True/False`도 숫자로 받는다.
   PR #41에는 bool을 거부하는 수정이 있지만 beta.3에 미통합이다. 이 제안 PR은 계산기 동작을 수정하지 않으며
   해당 PR의 numeric-input PASS를 beta.3로 옮기지 않는다. 별도 fix-forward 후 정확한 소스로 검증해야 한다.
2. **A11.06–09:** 현재 evaluator는 duplicate JSON keys·strict provenance를 강제하지 않는다.
   strict parsing, stale output 방지, DI/reference/render/source/run 연결은 명시적인 새 증거 프로토콜 제안이다.
3. **A14.01/05:** 고정 beta.3의 `docs/RELEASE_CHECKLIST.md`는 beta.2를 현재 후보로 쓰고
   107개 전체를 unexecuted로 기술한다. 버전 범위와 누락 분류를 정정해야 한다. 패키징 성공만으로 내용 검토를 통과시킬 수 없다.
4. 실제 DAW·청음·장치·soak 7건과 입력/기준음 연결 13건, 종합승인 1건은 그대로 남는다.

## 6. 승인 정책과 후속 적용 순서

`release-policy.json`, `ci-check-map.json`, waivers, frozen benchmark 수치,
weekly/beta publication 정책, lifecycle policy, publisher/preflight/evidence 도구 및 기존 workflow를 변경하지 않았다.
44개 보호 파일의 bytes를 PR #78과 대조한다. 기존 제품 Source/Tests/Installer/VERSION도 변경하지 않았다.
`expires_at_utc=null`, `expiry_approved=false`, `release_approved=false`, `RELEASE_REVIEW_PENDING`을 유지한다.
기존 공개 beta.2의 승인/태그/패키지와 별도 weekly publication 승인 경로도 그대로 보존한다.
새 정의를 기존 fixed-gap exception inventory에 몰래 반영하거나 strict BLOCKED를 승인으로 해석하지 않는다.

후속 적용은 다음 순서다.

1. 86개 **새 ordinal 연결**과 A5–A8의 명시적 범위 대체를 검토·승인한다. 조사 PR의 병합만으로 자동 승인하지 않는다.
2. 원장의 증거를 각 승인된 predicate와 대조해 부족한 fixture/producer만 보완한다. 원본 입력이 없는 항목은 BLOCKED다.
3. definition/policy fingerprint와 receipt 계약을 별도 변경으로 묶는다. 기존 hard gate를 삭제·면제하지 않고,
   변경된 정책에 과거 receipt를 자동 통과시키지 않는다. weekly fixed-gap 목록·publisher 계약에 영향을 주면 별도 검토한다.
4. 승인된 만료 정책이 들어간 최종 source/package의 필요한 검증, 실제 수용 및 버전·소스·패키지별 공개 승인을 진행한다.

## 7. 재현과 이번 검증

```sh
python Tools/test_acceptance_definitions.py
python Tools/audit_acceptance_definitions.py --output build/acceptance-definition-audit.json
```

16개 테스트는 누락/중복 ID, 원본 정의 날조, PASS/N/A 승격, 미승인 명세 승인,
hard gate 삭제, 알 수 없는 증거, 정책 fingerprint 변경, archive 변조,
다른 source 승계, CTest identity 바꿔치기·플랫폼 누락·발췌 날조, raw receipt 왜곡, 과거 workflow 실패 숨김을 거부한다.
정상 경로는 실제 원본 게이트를 재계산한다. CI `Acceptance definition evidence audit`도 같은 검사를 실행한다.
종료 코드 0은 **조사자료 무결성 확인**이며 제품 acceptance나 공개 승인이 아니다.
이 workflow artifact는 기존 release-evidence producer 집합에 포함되지 않는다.
