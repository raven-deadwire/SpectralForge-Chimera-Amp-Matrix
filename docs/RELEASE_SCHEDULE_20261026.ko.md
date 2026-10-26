# 릴리즈 스케쥴 정리 — 2026-10-26 (KST)

이 문서는 2026-10-26 기준 저장소·CI·메일 점검 결과를 반영한 **작업·판정 목표**입니다.
공개 확정일이 아니며, 어떤 게이트도 PASS로 올리거나 출시를 승인하지 않습니다.
버전 작업창의 권한 문서는 계속 [DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md)이고,
중장기 재편성안은 Draft [#93](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/93)이 승인될 때까지 *제안* 상태입니다.

## 1. 현재 위치

| 항목 | 상태 |
|---|---|
| 공개판 | **1.3.2-beta.1** (`VERSION` 1.3.2 / `RELEASE_CHANNEL` beta.1), 만료 2027-02-11 23:59 KST |
| 별도 배포 | 팩토리 프리셋 유지보수판 `presets-2026.10.19.1` (엔진 바이트는 beta.1 고정) |
| 다음 목표 | **1.3.2-beta.2** — 유지 |
| 출시 시점 결정 요인 | **Transpose 검증**과 **AAX 서명·Pro Tools 테스트** |

main에 병합된 아래 항목은 **아직 공개판에 포함되지 않았습니다.** [공개 이력](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases)

## 2. beta.2 포함 범위

| 항목 | main 상태 | 남은 일 | 판단 |
|---|---|---|---|
| **Easy view** | 프리셋·6개 노브·Auto Set·튜너 간단 화면 병합 (#161) | 입력별 GAIN 연결·드래그 중 파라미터 변경 수정 [#164](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/164): 이전 커밋 Windows 실패 원인 확인, main 병합 커밋 CI 진행 중(실패 없음) | 보완 후 포함 |
| **Auto Set** | 입력/게이트 → Tone EQ → 컴프 → 멀티밴드 → De-esser/리미터 5단계, 진행·결과 표시 병합 | 피킹 어택 과감쇠·Undo의 수동 수정 덮어쓰기 수정 [#156](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/156): main 병합·로컬 테스트 통과, CI 대기 후 머지. Gate 2/Expander 단계 [#169](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/169)는 #156을 합쳐 테스트 통과, #156 다음에 머지 | 핵심 기능으로 포함 |
| **다이내믹스·EQ** | 독립 4밴드, PRE/POST 동시 게이트, 별도 Expander, EQ 밴드 Dynamic, 프리셋 48개 조정 병합 (#163) | 실제 기타·베이스 청음, 기존 세션 복원 확인([청음 체크리스트](LISTENING_CHECKLIST_DYNAMICS_163.ko.md)) | 포함 후보 |
| **Standalone 사용성** | 최대화/F11·Fit screen·노브 조작 통일·Windows ASIO 빌드 | 장치 진단·첫 실행 ASIO 선택 [#124](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/124): main 병합·로컬 테스트 통과, CI 대기 후 머지 | 포함 |
| **OBS 애드온** | Windows·macOS/Linux, Python 없는 설치 도우미, 상태 복원·설치 보호, macOS PKG 선택 설치 병합 (#151/#157/#166) | 실제 OBS 장면 복원·다중 인스턴스·장시간 녹화. Linux는 별도 설치, DEB/TAR 통합은 후속 | 포함 후보 |
| **베타 잔여 기간 표시** | 정확한 잔여 시간·UTC/KST 표시 통합 | SETTINGS 헤더 [#126](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/126): main 병합, CI 대기 후 머지. beta.2 만료일·패키지 표시 일치 | 포함 |

**Auto Set 검증 원칙:** 악기 자동 판별에 기대지 않고 **Guitar/Bass를 명시 선택**한 상태로 검증합니다.
수정 후보에서도 합성 Drop A 기타 → 베이스, 고음역 6현 베이스 → 기타 오분류가 남아 있습니다.

## 3. 출시 필수 조건 (P0 4개 — 유지)

UI·OBS 준비 여부와 무관하게 빠지지 않습니다. [요구사항 #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97)

| P0 | 현재 | 남은 일 |
|---|---|---|
| **Transpose** | v3 연구·시험 기반 마련 (#154 병합), 제품 적용 미완 | **2.5ms는 PDC 보고값**이며 저역 실제 지연은 별개. 44.1kHz 실패, 저역·업시프트, 실제 DI·청음·DAW 검증. v4/1.25ms 절충안 [#155](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/155)는 음질·CPU 승인 보류이며, 연구 자료로만 main에 보관(제품 미적용). 11/2–11/6 [실제 DI 검증 계획](TRANSPOSE_V3_DI_VALIDATION_PLAN_20261026.ko.md) |
| **FabFilter 참조 반영** | 화면·독자 DSP 상당 부분 진행 | 원본 응답·검출 시간·위상·미터 비교와 수용 판정. [#127](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/127)은 비교 도구일 뿐 측정 완료가 아님(main 병합·자체 테스트 통과, CI 후 머지 예정). 원본 플러그인 렌더와 비교는 미실시 |
| **베타 수명주기** | beta.2 만료일 **2027-03-31 23:59 KST 승인(10/26)**, 정책 파일·`RELEASE_CHANNEL` 반영([결정안](BETA2_EXPIRY_DECISION_PROPOSAL_20261026.ko.md)) | 실행 중 만료·재실행·beta.1에서 업데이트 후 프로젝트/IR 복원 검증. beta.1 만료일(2027-02-11)은 beta.1에만 적용. 베타테스터용 무기한 연장 코드(테스터별 서명, 이후 모든 베타 적용) 구현([발급 안내](BETA_TESTER_CODES.ko.md)), CI·실제 DAW 입력 확인 필요 |
| **AAX Native** | 개발 빌드 가능 | PACE 승인(10/24 신청 접수 확인, 승인 회신 미확인, 보통 7영업일 → 11/3 전후 예상. iLok 계정 활성화·Developer 라이선스 재신청은 소유자 조치 필요) → iLok 준비 → 서명 → Windows/macOS 일반 Pro Tools 검증. [서명 준비 체크리스트](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md) |

## 4. 단기 일정 (작업·판정 목표)

| 기간 · KST | 우선 작업 | 완료 산출물 |
|---|---|---|
| **10/26–10/28** | #164·#156·#169·#124·#126·#127·#155 통합(main 병합·로컬 확인 완료, CI 대기. 아래 진행 상황), 중복·오래된 PR 정리(완료), 청음·DI·AAX 준비 문서(완료) | beta.2 통합 후보 소스와 포함 목록 |
| **10/29–10/30** | 3OS 빌드·설치, 화면 전환·게이트·프리셋·개인 IR 회귀, OBS 실제 테스트 | **내부 테스트 빌드와 실패 목록** |
| **11/2–11/6** | Transpose 실제 DI/DAW 검증, EQ·다이내믹스 원본 비교, Auto Set 청음 조정 | P0별 통과/미달 판정과 수정 범위 |
| **11/9–11/13** | 통과한 변경을 최종 소스로 동결, 설치·상태 복원·성능 재검증 | **beta.2 공개 가능 여부 판정** |
| **11/16–11/27** | 미달 항목 수정, EQ 편집 후속 기능, OBS/Linux 패키징·호환성 보완 | 주간 검증 빌드와 다음 업데이트 범위 |

- AAX 트랙(PACE → iLok → 서명 → Pro Tools 검증)은 위 일정에 병행합니다. 외부 준비가 늦어지면 beta.2 공개 판단도 늦어집니다.
- 기존 10/23 목표는 지났습니다. **10/30에 자동으로 beta.3로 넘어가지 않습니다.** 공개는 P0 4개의 실제 결과로만 결정합니다.
- 매주 실질적인 개선 빌드를 만드는 원칙은 유지합니다.

### 통합 진행 상황 (2026-10-26 13:27 KST 기준)

통합 요청을 받은 PR에는 모두 최신 main을 병합했고, 충돌을 정리한 새 커밋으로 CI를 다시 돌리는 중입니다. **CI가 모두 통과하고 충돌이 없을 때만** main에 머지합니다. 아직 머지된 코드 PR은 없습니다. #164·#156은 체크를 직접 확인했고 실패가 없으며, 나머지 PR도 구독 중 **실패 알림이 없습니다**(러너 대기·진행 중). "로컬 확인"은 Linux에서 돌린 테스트이며, Windows·macOS와 UI 테스트는 CI 결과로 판정합니다.

| PR | 내용 | 진행 | 로컬 확인 | 남은 일 |
|---|---|---|---|---|
| [#164](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/164) | Easy view 리뷰 수정 | main 병합 `6f105d3`. 55개 체크 중 완료분 전부 통과(Linux 전체 빌드·테스트 포함), Windows 전체 빌드·`windows-candidate`·macOS 일부 진행 중 | — | 이전 커밋의 Windows 실패(`ChimeraEasyModeUITests` "Easy preset did not load")는 Easy view 프리셋 비동기 로드 탓이며 `6f105d3`의 동기 로드로 해결 예상. Windows 결과 확인 후 머지 |
| [#156](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/156) | Auto Set 공격음 보호·Undo 소유권 | main 병합 `a5d2460`, Draft 해제. 39개 체크 중 완료분 전부 통과(`windows-candidate` 포함), 3OS 전체 빌드 대기 | `ChimeraAutoSetTests` 전체 통과(5단계 통합 실행 포함) | CI green 시 머지. 악기 판별 오분류·실제 DI 청음·DAW 승인은 미해결 |
| [#169](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/169) | Auto Set 5단계(Gate 2·Expander) | **#156을 합친** `87e68b4`, CI 진행 중 | `ChimeraAutoSetTests` 전체 통과(#156·#169 테스트 함께) | **#156 머지 후** 머지. 히스 기준(−66 dBFS)·여유(24 dB) 등은 청음 전 출발값 |
| [#124](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/124) | Standalone 장치 진단·첫 실행 ASIO | main 병합 `3dc17a0`, CI 진행 중 | 장치 진단 테스트 3회 연속 통과, Standalone 빌드 통과 | CI green 시 머지. 실제 Windows 장치·loopback 지연 측정은 미실시, #113은 닫지 않음 |
| [#126](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/126) | SETTINGS 헤더 KST·잔여 시간 | main 병합 `0141b81`(충돌 없음), CI 진행 중 | 변경 파일 컴파일 통과 | CI green 시 머지 |
| [#127](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/127) | FabFilter 블랙박스 비교 도구 | main 병합 `891e457`(충돌 없음), PR 본문에 범위 기록, CI 진행 중 | `ChimeraKitProbe` 빌드, 자체 테스트 17/17 통과 | CI green 시 머지. 원본 렌더 측정 전이므로 P0 판정은 바뀌지 않음 |
| [#155](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/155) | Transpose v4 연구 자료(#150 포함) | 연구 자료 보관으로 결정, base를 main으로 변경, main 병합 `710c012`, CI 진행 중 | 연구 플러그인·호스트 체크 문법 검사, Python 컴파일 통과 | CI green 시 머지. 제품 Source·T6 기본값(v3) 불변, 승인 아님 |

CI는 구독해 두었고, 50분 간격 재확인에서 통과한 PR부터 머지하고 이 표를 갱신합니다.

### 오늘 main에 들어간 문서

| PR | 내용 |
|---|---|
| [#170](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/170) · [#172](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/172) | 이 릴리즈 스케쥴 작성과 통합 진행 상황 |
| [#175](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/175) | [#163 다이내믹스·EQ 청음 체크리스트](LISTENING_CHECKLIST_DYNAMICS_163.ko.md) |
| [#176](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/176) | [Transpose v3 실제 DI 검증 계획 (11/2–11/6)](TRANSPOSE_V3_DI_VALIDATION_PLAN_20261026.ko.md) |
| [#177](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/177) | [AAX 서명 준비 체크리스트](AAX_SIGNING_PREP_CHECKLIST_20261026.ko.md) |

### PR 정리 결과 (10/26 완료)

닫은 PR은 브랜치를 지우지 않았으므로 필요하면 다시 열 수 있습니다. 각 PR에 닫은 이유를 코멘트로 남겼습니다.

| 분류 | 닫은 PR |
|---|---|
| main에 이미 반영된 중복 | #167 (변경 파일 8개가 main과 동일) |
| 공개된 beta.1로 대체된 이력 | #68, #70, #88, #90, #94 |
| 과거 beta.3 계획 | #78, #84, #85, #87, #89, #91 |
| 오래된 Transpose 연구 | #99, #125, #133, #150 (현재 기준은 #154·#155) |

## 5. 중장기 작업창 (Draft #93 재편성안 — 미승인)

현재 main의 로드맵에는 예전 달력(1.3.3 Room Core 11/30–12/25, 1.4 독립 4밴드 등)이 남아 있습니다.
아래는 독립 Náströnd/Niflheimr 제품 시간을 확보한 **기존 수정안의 작업창**이며, #93 승인 시 로드맵과 카탈로그 JSON을 함께 갱신합니다.

| 작업창 · KST | 범위 |
|---|---|
| **2026/11/30–12/25** | **1.3.3 공통 Original FX** — 기타 Boost, 베이스 Drive/Fuzz, 공통 PRE·상태 기반 |
| **2026/12/28–2027/1/22** | **Náströnd 독립 VST3** 첫 베타 후보 |
| **2027/1/25–2/19** | **Niflheimr 독립 VST3** 첫 베타 후보·LOW/Blend 보존 |
| **2027/2/22–3/19** | **1.3.4 Room Core** — Size·Send·Absorption, 실제 초기반사·잔향 |
| **2027/3/22–5/28** | **1.4/1.5** — 제한된 신호 순서 변경·저장/Undo → 드래그 편집, PRE 확장 |
| **2027/5/31–7/30** | **1.6/1.7** — 앰프 확장, 멀티밴드 새추레이션, POST·필터 확장 |
| **2027/8/2–11/19** | **1.8–1.10** — Room Advanced·공간계·Creative·신스 계층 |
| **2027/11/22–12/17** | **2.0 통합/RC**, 12/20–31 수정 여유 |

정합성 원칙:

- 이미 앞당겨 구현한 **독립 4밴드·De-esser를 2027년 신규 개발로 다시 계산하지 않습니다.**
- 앰프 42종 목표와 Niflheimr 5채널 NAM은 각각 검증·납품 계획을 연결합니다. NAM 비교 도구 개선을 모델 완성으로 계산하지 않습니다.

## 6. 이번 주 우선순위

1. **Easy view·Auto Set 수정 통합** → 쓸 수 있는 내부 테스트 빌드 확보 (10/30)
2. **Transpose 실제 검증**과 **AAX 서명 준비** 진전
