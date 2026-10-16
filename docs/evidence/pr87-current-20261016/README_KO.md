# PR #87 현재 소스 수용 증거 복구 — 2026-10-16 KST

검사 대상 제품은 **`72ecf35fa90293418da8635225435095dbca1135`**다. 이 문서와 검사 도구가 들어 있는 PR #89의 HEAD를 시험한 제품으로 바꿔 표시하지 않는다.

- Product CI: [38887964838](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38887964838), attempt 1, 성공. Windows **54/54**, Linux **51/51**, macOS **50/50** CTest.
- CAB·IR 집중 CI: [38887964889](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38887964889), 성공. 수용 근거에는 이 별도 component artifact를 승격하지 않고 **동일 Product run 안에서 다시 실행된 3개 suite**의 원시 로그를 사용했다.
- 현재 제품의 beta.2 → beta.3 네이티브 업그레이드: [38905833729](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38905833729), attempt 1, 성공. 도구 SHA `584caf48efdc0de9c7ab24f592fc3a556270424e`와 제품 SHA를 별도 기록했다.
- 대상 39건: **35건은 이미 완료된 현재 소스 실행에서 복구**, **4건(A1.01/A1.03/A1.04/A1.05)은 새 Windows 실행 후 복구**. 39/39 근거 확보.
- 원본 Product 게이트: **49 PASS / 79 BLOCKED**. 원본은 그대로 보존했다.
- 현재 증거를 반영한 **검토용 재계산: 88 PASS / 40 BLOCKED, 전체 BLOCKED**. `review-release-gate.json`은 `review_only=true`, `publisher_input=false`다. 공식 CI artifact를 덮어쓰거나 publisher에 주입하지 않는다.
- `release_approved=false`, `expiry_approved=false`, 공개 승인 없음. 실제 DAW·청음·사유 IR 권리/정품 인증을 합성 검사나 설치 파일 sentinel로 대신하지 않는다.

## 소스와 원본 보존

기존 승인 원장과 SHA-256 `457260441cd5000c2b82abaa3b4c75d7262c5a5d9187b1615ad6e34a8938dda5`는 불변이다. 기존 `accepted_baseline.fingerprint_changes`는 지금도 `CMakeLists.txt`, `Tests/CabIRAcceptanceTests.cpp`의 차이를 탐지하며 component 원본은 **22 PASS / 64 BLOCKED**를 유지한다. 새 consumer는 과거 PASS를 읽어 승계하지 않는다. 승인된 ID→시험 매핑만 재사용하고, 현재 소스의 전체 native 입력·CTest inventory/JUnit/완료 로그/installer·assembly receipt를 다시 검증한다.

GitHub archive digest, source/run/attempt, 원시 파일별 SHA-256을 확인한다. 원본 게이트 49/79를 기존 consolidator로 재생성해 원본 결과와 일치하는지 확인한 다음, 증거를 확보한 39개 ID만 검토용 결과에 연결한다. 제품·시험·빌드 입력 **613개**를 PR #87과 바이트 단위로 비교하며, 새 native 입력이 생기거나 기존 입력이 달라지면 차단한다. 승인 조건·hard gate·waiver·만료/공개 정책을 수정하지 않았다.

패키징 CTest의 내부 22개 unittest에는 플랫폼 제한 스킵이 있다. Windows에서 Debian 비교 1개, Linux에서 Windows 전용 3개, macOS에서 이 4개가 스킵된다. 스킵을 실행 PASS로 취급하지 않고, **Linux 전용 1개는 Linux에서, Windows 전용 3개는 Windows에서 실제 `ok`**인지 검증한다. 나머지 18개는 세 플랫폼에서 모두 실행됐다. 원시 상태는 `recovery.json`의 `package_subcases`에 보존했다.

## 추가 Windows 검증

기존 승인된 업그레이드 하네스의 단언을 유지하고 제품 source/run/바이트 해시만 현재 대상으로 고정했다. 임시 Windows x64 GitHub runner에서만 실행하며 기존 설치가 있으면 중단한다.

- Setup SHA-256: `ea8c8b90be574a5a0204ecd30afb7cf17ecff4856b97d3227d04cb985e12998e`
- Portable SHA-256: `c13914acfdd420198a1f4f4f3c59326a13cce0ca7a4d8e9e2f27351cca825300`
- 실제 공개 beta.2 설치 후 같은 숫자 버전 beta.3으로 업그레이드, Standalone 고의 손상 후 복구, 제거까지 4회 installer 호출.
- 설치 바이너리/메타데이터 교체, Unicode 경로, 개인 preset/IR/project **바이트 sentinel**과 수정 매뉴얼 보존 확인. 이 sentinel은 유효한 오디오/DAW 프로젝트가 아니므로 실제 세션 호환성 주장을 하지 않는다.
- 설치/업그레이드/복구/제거 원시 로그 4개를 해시와 함께 보존. 이전 PR78의 다른 제품 SHA 결과는 사용하지 않았다.

## 복구한 수용 기준 39개

전체 정의와 source/evidence 연결은 [recovered-39.csv](recovered-39.csv), 각 실제 실행의 원본 해시와 완료 assertion은 [recovery.json](recovery.json)에 있다. `E.*`는 승인 원장의 시험 매핑이며 이 보고서의 값은 **현재 소스에서 재실행된 결과**다.

| ID | 수용 기준 | 사용한 현재 실행 |
|---|---|---|
| A1.01 | 버전·소스·패키지 일치 | 새 Windows 업그레이드 + 현재 Product |
| A1.02 | 구성요소·경로 선택 | 현재 Product native/설치/assembly |
| A1.03 | 공개 beta.2 실제 업그레이드 | 새 Windows 업그레이드 + 현재 Product |
| A1.04 | 손상 복구 | 새 Windows 업그레이드 + 현재 Product |
| A1.05 | 제거 시 소유권·보존 | 새 Windows 업그레이드 + 현재 Product |
| A1.06 | 배포 묶음 무결성 | 현재 Product native/설치/assembly |
| A2.01 | Signature 식별·메타데이터 | 현재 Product native/설치/assembly |
| A2.02 | Matrix·5슬롯 PRE | 현재 Product native/설치/assembly |
| A2.03 | LOW 역할 | 현재 Product native/설치/assembly |
| A2.04 | Crom 구조 | 현재 Product native/설치/assembly |
| A2.05 | Wild 구조 | 현재 Product native/설치/assembly |
| A2.06 | Azhi 구조 | 현재 Product native/설치/assembly |
| A2.08 | 출력·소리 유효성 | 현재 Product native/설치/assembly |
| A3.01 | APVTS 파라미터 완전성 | 현재 Product native/설치/assembly |
| A3.02 | dirty→clean recall | 현재 Product native/설치/assembly |
| A3.05 | 합성 오디오 왕복 | 현재 Product native/설치/assembly |
| A3.06 | 레거시 host ID·순서 | 현재 Product native/설치/assembly |
| A3.07 | 과거 상태·누락 필드 | 현재 Product native/설치/assembly |
| A3.08 | 만료 전후 자산 보존 | 현재 Product native/설치/assembly |
| A3.09 | UI·host 편집 일치 | 현재 Product native/설치/assembly |
| A4.01 | 공장 ID·순서 호환 | 현재 Product native/설치/assembly |
| A4.02 | 성능 컨트롤 보존 범위 | 현재 Product native/설치/assembly |
| A4.03 | 이전 소리 잔류 제거 | 현재 Product native/설치/assembly |
| A4.04 | 저장 프로젝트에 재보이싱 금지 | 현재 Product native/설치/assembly |
| A4.05 | 공장48 음향 안전 | 현재 Product native/설치/assembly |
| A5.03 | 유형 분류·동명 파일 격리 | 현재 Product native/설치/assembly |
| A5.04 | 원본 bytes·메타데이터 보존 | 현재 Product native/설치/assembly |
| A5.07 | 라이브러리 제거·재등록 | 현재 Product native/설치/assembly |
| A6.03 | 유형 분류·동명 파일 격리 | 현재 Product native/설치/assembly |
| A6.04 | 원본 bytes·메타데이터 보존 | 현재 Product native/설치/assembly |
| A7.01 | 원본 오디오 hash | 현재 Product native/설치/assembly |
| A7.02 | archive와 audio 추적 분리 | 현재 Product native/설치/assembly |
| A7.03 | 출처·라이선스 무추정 | 현재 Product native/설치/assembly |
| A7.04 | 공개 factory 2종 hash·고지 | 현재 Product native/설치/assembly + 현재 distribution negative |
| A7.05 | 공개 payload 제외 규칙 | 현재 Product native/설치/assembly + 현재 distribution negative |
| A7.06 | IR 자산 상태 무결성 | 현재 Product native/설치/assembly |
| A7.07 | 쓰기 실패의 원자성 | 현재 Product native/설치/assembly |
| A8.01 | 외부 파일 없는 공장 recall | 현재 Product native/설치/assembly |
| A8.04 | CAB/IR 전환 격리 | 현재 Product native/설치/assembly |

## 남은 40개 — 모두 BLOCKED

기존 86개 정의 중 추가 검증이 필요한 19개와 외부 수용검사 21개를 구분했다. CAB·IR 직접 검사 6개(A2.07/A5.01/A6.01/A5.05/A6.05/A8.02)는 이번 39개에 중복 계산하지 않는다.

| ID | 분류 | 필요한 근거 |
|---|---|---|
| A3.03 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | Guitar4 및 lifecycle 사례 재사용; Bass3 개별 저장 상태를 수용 증거에 연결한다. |
| A3.04 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | 기존 A/B 및 expiry processor 증거 재사용; 각 Bass Signature의 구체 슬롯 값 목록을 연결한다. |
| A5.02 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | 기존 WAV/UI 및 installed companion 시험은 부분 증거. AIFF 및 각 악기 유형·경로의 실행 사례를 추가 연결한다. |
| A5.06 | REAL_DAW_AND_PRIVATE_ASSET | 기존 lifecycle/IR isolation 증거는 부분 재사용. Hartke/Marshall 실물 개인 파일 및 실제 DAW 프로젝트의 bit-identical fixture가 없으므로 호환 완료로 주장하지 않는다. |
| A5.08 | PRIVATE_ASSET_RIGHTS_AND_LISTENING | 분포 allowlist·설치 파일 보존 증거 재사용. private 캡처 청음·권리·정품 hash 인증은 별도 입력이 없으면 BLOCKED. |
| A6.02 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | 기존 WAV/UI 및 installed companion 시험은 부분 증거. AIFF 및 각 악기 유형·경로의 실행 사례를 추가 연결한다. |
| A6.06 | REAL_DAW_AND_PRIVATE_ASSET | 기존 lifecycle/IR isolation 증거는 부분 재사용. Hartke/Marshall 실물 개인 파일 및 실제 DAW 프로젝트의 bit-identical fixture가 없으므로 호환 완료로 주장하지 않는다. |
| A6.07 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | 파일 보존/재등록 실행 결과를 재사용. 저장 상태와 결합한 후속 열기 사례는 별도 연결한다. |
| A6.08 | PRIVATE_ASSET_RIGHTS_AND_LISTENING | 분포 allowlist·설치 파일 보존 증거 재사용. private 캡처 청음·권리·정품 hash 인증은 별도 입력이 없으면 BLOCKED. |
| A8.03 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | embedded-only 복원은 기존 증거. 양쪽 모두 무효인 정책·UI 결과를 명시하고 negative fixture를 추가 연결해야 한다. |
| A11.06 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | PR41 output roundtrip은 재사용. 현재 load는 일반 json.loads이므로 duplicate/nonstandard constant 정책 보강 필요. |
| A11.07 | ADDITIONAL_NATIVE_OR_PROTOCOL_CASES | 원래 exit contract는 코드에서 복구. stale-output 방지 producer와 CLI subprocess negative receipt 추가 필요. |
| A11.08 | MEASUREMENT_AND_REVIEW_PROVENANCE | 현재 evaluator JSON 자체는 provenance를 보증하지 않는다. 실제 외부 입력·측정 producer가 필요하다. |
| A11.09 | MEASUREMENT_AND_REVIEW_PROVENANCE | PR41은 component receipt 예시일 뿐 release receipt가 아니다. 실제 acceptance producer/검토자는 별도 승인·연결이 필요하다. |
| A14.02 | PACKAGE_OR_LANGUAGE_REVIEW | 파일 존재/embedded packaging 증거는 부분 지원. 3개 언어 전체 의미 검토 receipt가 없다. |
| A14.03 | PACKAGE_OR_LANGUAGE_REVIEW | catalog contract/public naming의 source 및 native 일부 증거 재사용. 문서 전체 의미 일치는 별도 검토. |
| A14.04 | PACKAGE_OR_LANGUAGE_REVIEW | docs/IR_DISTRIBUTION 및 EXTERNAL_BASS_IRS의 현재 설명을 기준으로 3개 언어·체크리스트를 대조. |
| A14.06 | PACKAGE_OR_LANGUAGE_REVIEW | 기존 assembly 및 PR78 Windows receipt 재사용. macOS/Linux 실제 출시 패키지 설치 보존은 fixture와 구분하여 연결한다. |
| A14.07 | PACKAGE_OR_LANGUAGE_REVIEW | 현재 설명·패키지 metadata와 비교한 reviewer receipt가 필요하다. 공개 일정은 승인으로 사용하지 않는다. |
| B1.RESULT | 별도 DI/레퍼런스 | 동일 DI 파일·전처리·레벨·해시를 제품 SHA에 연결 |
| B10.RESULT | 실제 오디오·프로젝트 | 정의된 실제 오디오 저장/복원 왕복 fixture와 원시 비교 |
| B11.RESULT | 실제 DAW/호스트 | Windows 호스트의 로드·재생·편집·제거·종료 관찰 |
| B12.RESULT | 실제 DAW/호스트 | Studio One 8 프로젝트/플러그인 수용검사 |
| B13.RESULT | 실환경 지속시험 | 정의된 30분 soak와 장애/CPU/오디오 기록 |
| B14.RESULT | 최종 종합 검토 | 모든 필수 근거와 별도 공개 승인; 날짜/CI 성공으로 대체 금지 |
| B2.RESULT | 별도 DI/레퍼런스 | Signature 실제 렌더·프리셋/CAB·레이트/블록 및 해시 |
| B3.RESULT | 실제 오디오 측정 | 실제 렌더에서 재현 가능한 원시 측정값·측정구간 |
| B4.H1 | 실제 오디오 측정 | 실측 저역 RMS 순서와 간격; 계산기 fixture로 대체 금지 |
| B4.H2 | 실제 오디오 측정 | 실측 저역 Side 조건 |
| B4.H3 | 실제 오디오 측정 | 실측 고역 Side 조건 |
| B4.H4 | 실제 오디오 측정 | 실측 crest 조건 |
| B5.RESULT | 실제 오디오 측정 | 실측 secondary 3/4 조건 |
| B6.RESULT | 실제 오디오 측정 | 실측 가중 점수·critical·hard gate 전체 결과 |
| B8.RESULT | 청음 | 제품 SHA/DI/레퍼런스에 연결한 Listening A/B 관찰·검토자 |
| E2.CALIBRATION | 별도 레퍼런스/캘리브레이션 | E670FE 레퍼런스·레벨·캘리브레이션 인증 |
| E2.SAME_DI | 별도 DI/레퍼런스 | E670FE same-DI 실제 비교 렌더 |
| E2.TD_EQ | 실측·청음 | E670FE 시간영역/EQ/음질 수용 근거 |
| I2.GATE_DI | 실제 입력 | 실제 DI에 대한 gate 수용 기록 |
| I2.LIVE_REMOVE_DAW | 실제 DAW/호스트 | 재생 중 플러그인 제거·종료 안정성 |
| I2.UI_AUDIO_TIMING | 실제 호스트 계측 | UI/오디오 timing 실환경 측정과 관찰 |

[remaining-40.csv](remaining-40.csv)에 전체 ID와 상태를 제공한다. 미실행 40개를 이번 작업에서 PASS로 바꾸지 않았다.

## 재현과 회귀 방어

```sh
python Tools/test_pr87_recovery.py
python Tools/test_accepted_baseline.py
python Tools/recover_pr87_evidence.py \
  --upgrade docs/evidence/pr87-current-20261016/upgrade-38905833729.zip \
  --upgrade-metadata docs/evidence/pr87-current-20261016/upgrade-metadata.json \
  --output /tmp/pr87-recovery.json
```

출력 경로는 새 파일이어야 한다. 다른 제품 source/run/attempt, raw/ZIP 변조, 누락 upgrade, skip/중복/불완전 CTest, JUnit과 전체 로그의 불일치, 잘못된 플랫폼 소유권, 다른 upgrade run, false 승인·오류 receipt를 거부한다. 로컬과 [독립 증거 재검증 CI 38906474306](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906474306)에서 **새 회귀검사 12/12 + 기존 baseline 보호검사 17/17** 통과. 이 CI의 도구 SHA는 `e78bbe4b8cbe8425287a07a44443c10b6a18a9c0`이며 검사 대상 제품 SHA는 계속 `72ecf35`다. Windows 네이티브 검사는 별도 실제 CI로 확인했다.

원본 ZIP과 metadata는 저장소에 고정되어 CI artifact 보존기간 이후에도 검토 가능하다. 새 도구 CI는 저장된 원시 결과를 재검증하며 Windows/macOS/Linux 제품을 재빌드한 것으로 주장하지 않는다.
