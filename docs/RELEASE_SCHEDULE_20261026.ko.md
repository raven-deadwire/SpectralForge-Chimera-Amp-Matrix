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
| **Easy view** | 프리셋·6개 노브·Auto Set·튜너 간단 화면 병합 (#161) | 입력별 GAIN 연결·드래그 중 파라미터 변경 수정 [#164](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/164): 이전 커밋 Windows 실패와 구분하여 `main` 대상 head `6f105d3a`의 CI 결과 확인 중 | 보완 후 포함 |
| **Auto Set** | 입력/게이트 → Tone EQ → 컴프 → 멀티밴드 → De-esser/리미터 5단계, 진행·결과 표시 병합 | 피킹 어택 과감쇠·Undo의 수동 수정 덮어쓰기 수정 [#156](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/156): main 병합·로컬 테스트 통과, CI 대기 후 머지. Gate 2/Expander 단계 [#169](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/169)는 신규 후보 | 핵심 기능으로 포함 |
| **다이내믹스·EQ** | 독립 4밴드, PRE/POST 동시 게이트, 별도 Expander, EQ 밴드 Dynamic, 프리셋 48개 조정 병합 (#163) | 실제 기타·베이스 청음, 기존 세션 복원 확인 | 포함 후보 |
| **Standalone 사용성** | 최대화/F11·Fit screen·노브 조작 통일·Windows ASIO 빌드 | 장치 진단·첫 실행 ASIO 선택 [#124](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/124): main 병합·로컬 테스트 통과, CI 대기 후 머지 | 포함 |
| **OBS 애드온** | Windows·macOS/Linux, Python 없는 설치 도우미, 상태 복원·설치 보호, macOS PKG 선택 설치 병합 (#151/#157/#166) | 설치 보호 후속 [#167](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/167)의 동일 소스 macOS PKG 검증·통합. 실제 OBS 장면 복원·다중 인스턴스·장시간 녹화. Linux는 별도 설치, DEB/TAR 통합은 후속 | 포함 후보 |
| **베타 잔여 기간 표시** | 정확한 잔여 시간·UTC/KST 표시 통합 | SETTINGS 헤더 [#126](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/126): main 병합, CI 대기 후 머지. beta.2 만료일·패키지 표시 일치 | 포함 |

**Auto Set 검증 원칙:** 악기 자동 판별에 기대지 않고 **Guitar/Bass를 명시 선택**한 상태로 검증합니다.
수정 후보에서도 합성 Drop A 기타 → 베이스, 고음역 6현 베이스 → 기타 오분류가 남아 있습니다.

## 3. 출시 필수 조건 (P0 4개 — 유지)

UI·OBS 준비 여부와 무관하게 빠지지 않습니다. [요구사항 #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97)

| P0 | 현재 | 남은 일 |
|---|---|---|
| **Transpose** | v3 연구·시험 기반 마련 (#154 병합), 제품 적용 미완 | **2.5ms는 PDC 보고값**이며 저역 실제 지연은 별개. 44.1kHz 실패, 저역·업시프트, 실제 DI·청음·DAW 검증. v4/1.25ms 절충안 [#155](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/155) 음질·CPU 승인 보류 |
| **FabFilter 참조 반영** | 화면·독자 DSP 상당 부분 진행 | 원본 응답·검출 시간·위상·미터 비교와 수용 판정. [#127](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/127)은 비교 도구일 뿐 측정 완료가 아님 |
| **베타 수명주기** | beta.1 정책 커밋됨 | beta.2 전용 만료일 결정. 실행 중 만료·재실행·업데이트 후 프로젝트/IR 복원. **beta.1의 2027-02-11 만료일 자동 승계 금지** |
| **AAX Native** | 개발 빌드 가능 | PACE 승인(10/24 신청 접수 확인, 승인 회신 미확인) → iLok 준비 → 서명 → Windows/macOS 일반 Pro Tools 검증 |

## 4. 단기 일정 (작업·판정 목표)

| 기간 · KST | 우선 작업 | 완료 산출물 |
|---|---|---|
| **10/26–10/28** | #164 CI 해결, #156 Auto Set 안전성 수정·#124 장치 진단·#126 통합(아래 진행 상황), #169 검토, 중복·오래된 PR 정리(완료) | beta.2 통합 후보 소스와 포함 목록 |
| **10/29–10/30** | 3OS 빌드·설치, 화면 전환·게이트·프리셋·개인 IR 회귀, OBS 실제 테스트 | **내부 테스트 빌드와 실패 목록** |
| **11/2–11/6** | Transpose 실제 DI/DAW 검증, EQ·다이내믹스 원본 비교, Auto Set 청음 조정 | P0별 통과/미달 판정과 수정 범위 |
| **11/9–11/13** | 통과한 변경을 최종 소스로 동결, 설치·상태 복원·성능 재검증 | **beta.2 공개 가능 여부 판정** |
| **11/16–11/27** | 미달 항목 수정, EQ 편집 후속 기능, OBS/Linux 패키징·호환성 보완 | 주간 검증 빌드와 다음 업데이트 범위 |

- AAX 트랙(PACE → iLok → 서명 → Pro Tools 검증)은 위 일정에 병행합니다. 외부 준비가 늦어지면 beta.2 공개 판단도 늦어집니다.
- 기존 10/23 목표는 지났습니다. **10/30에 자동으로 beta.3로 넘어가지 않습니다.** 공개는 P0 4개의 실제 결과로만 결정합니다.
- 매주 실질적인 개선 빌드를 만드는 원칙은 유지합니다.

### 통합 진행 상황 (2026-10-26 11:30 KST 기준)

각 PR에 최신 main을 병합했고, 충돌을 정리한 뒤 새 커밋으로 CI를 다시 돌리는 중입니다. **CI가 모두 통과하고 충돌이 없을 때만** main에 머지합니다. 아래 "로컬 확인"은 Linux에서 돌린 테스트이며, Windows·macOS와 UI 테스트는 CI 결과로 판정합니다.

| PR | 내용 | 진행 | 로컬 확인 | 남은 일 |
|---|---|---|---|---|
| [#164](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/164) | Easy view 리뷰 수정 | main 병합 커밋 `6f105d3` CI 대기 | — | 이전 커밋의 Windows 실패(`ChimeraEasyModeUITests`의 "Easy preset did not load")는 Easy view 프리셋이 비동기로 로드되던 탓. main의 동기 로드가 `6f105d3`에 들어와 같은 테스트가 #161에서 Windows 포함 통과. Windows 재실행 결과 확인 필요 |
| [#156](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/156) | Auto Set 공격음 보호·Undo 소유권 | main 병합 `a5d2460`, Draft 해제, CI 대기 | `ChimeraAutoSetTests` 전체 통과(5단계 통합 실행 포함) | CI green 시 머지. 악기 판별 오분류·실제 DI 청음·DAW 승인은 미해결로 유지 |
| [#124](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/124) | Standalone 장치 진단·첫 실행 ASIO | main 병합 `3dc17a0`, CI 대기 | 장치 진단 테스트 3회 연속 통과, Standalone 빌드 통과 | CI green 시 머지. 실제 Windows 장치·loopback 지연 측정은 미실시, #113은 닫지 않음 |
| [#126](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/126) | SETTINGS 헤더 KST·잔여 시간 | main 병합 `0141b81`(충돌 없음), CI 대기 | 변경 파일 컴파일 통과 | CI green 시 머지 |
| [#169](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/169) | Auto Set 5단계(Gate 2·Expander) | 미검토 | — | #156 머지 후 충돌 여부와 함께 검토 |

### PR 정리 결과 (10/26 확인)

아래 닫은 PR은 브랜치를 지우지 않았으므로 필요하면 다시 열 수 있습니다. 각 PR에 닫은 이유를 코멘트로 남겼습니다.

**#167은 현재 열린 설치 보호 후속 PR이며 중복 종료 목록에서 제외합니다.** 초기 #166과 겹쳤던 이력과 달리, 현재 `1c3fcd91`은 프로세스 재확인·console 사용자/home 검증·helper seal 확인·회귀 검사를 추가합니다. 현재 diff와 동일 head CI, 특히 실제 macOS PKG gate를 검토한 뒤 통합을 판단합니다.

| 분류 | 닫은 PR |
|---|---|
| 공개된 beta.1로 대체된 이력 | #68, #70, #88, #90, #94 |
| 과거 beta.3 계획 | #78, #84, #85, #87, #89, #91 |
| 오래된 Transpose 연구 | #99, #125, #133, #150 (현재 기준은 #154·#155) |

### #164 / #167 판정 근거 (2026-10-26 11:27 KST 확인)

아래는 **확인 시점의 정확한 head** 기준입니다. 새 커밋이 올라오면 그 소스의 실행 결과를 다시 확인하며, 이전 실패를 현재 실패로 옮기거나 이전 성공을 현재 PASS로 승계하지 않습니다.

| PR · 확인한 head | 확인 시점 CI | 보존할 구분 |
|---|---|---|
| #164 · `6f105d3a7b351a6f0e9cadceccfcc60e2f4f5669` | [전체 빌드 40256090473](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40256090473) · [Windows 후보 40256090391](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40256090391) 모두 queued | 이전 `1495b6c`의 Windows Easy preset 실패는 이력으로 보존. 현재 PR은 `main`을 대상으로 동기 프리셋 로딩 수정을 포함하지만, 현재 CI 통과나 실제 DAW 수용을 뜻하지 않음 |
| #167 · `1c3fcd91a83cb68c3dc379c6abf9fab61a12d467` | [전체 빌드 40256359461](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40256359461) pending, [튜너 통합 40256359377](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40256359377) queued, [설치 안전성 40256359381](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40256359381) success | #166의 성공은 이 후속 수정본의 검증이 아님. rollback 보호 범위는 사용자 OBS 필터이며 전체 PKG가 아님. GUI·실제 OBS·물리 Intel/macOS 12–14·서명/notarization 수용은 별도 |

이 정정은 문서의 분류와 검증 기준만 바로잡습니다. PR 종료·병합·공개 배포를 승인하거나 릴리즈 게이트를 완화하지 않습니다.

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
