# 버전 배정 대조표 — 2026-10-29 (KST)

이 문서는 통합 브랜치 `claude/integration-1-10`에 구현된 기능을 **main의 개발 로드맵**
([DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md)) 버전과 대조합니다.
어떤 게이트도 PASS로 올리거나 출시를 승인하지 않습니다.

## 결정 (소유자, 2026-10-29)

- **버전 번호는 main 로드맵을 기준으로 유지합니다.** 1.3.3 Room Core부터 1.10 SYNTH까지 기존 번호를 그대로 씁니다.
- 미승인 Draft #93(10/22) 재편성안의 번호는 따르지 않습니다. #93은 1.3.3을 공통 Original FX로 바꾸고, Náströnd·Niflheimr 독립 VST3를 끼워 넣으며, Room Core를 1.3.4로, 1.4–1.10을 약 3개월 뒤로 옮깁니다.
  - #93이 나중에 승인되면 이 대조표를 다시 맞춥니다.
  - 기능 코드는 버전 번호를 담고 있지 않습니다. 파라미터 version hint 14–28은 저장 순서용이고 출시 버전이 아닙니다. 따라서 번호가 바뀌어도 바뀌는 것은 문서와 계획입니다.
- 이 대조표의 어떤 기능도 1.3.2-beta.2 범위가 아닙니다. 공개 순서와 시기는 로드맵의 작업창과 각 버전의 수용 검증으로 정합니다.

## 버전별 대조

| 버전 | 로드맵 범위 | 통합 브랜치에 구현됨 | 빠진 것 | 로드맵보다 더 들어간 것 |
|---|---|---|---|---|
| **1.3.3** | Room Core: Size·Absorption·Bypass, 리그별 Send, 실제 초기반사·잔향, 상태·자동화, LOW DI 분리 | 전부 ([ROOM_CORE_1_3_3.md](ROOM_CORE_1_3_3.md)) | 실제 DAW 자동화·복원, Windows 테스트 설치본, 청음 | — |
| **1.4** | 독립 4밴드 다이내믹스, PRE A 6종, 첫 제한형 신호 순서 편집기 | 4밴드(main #163에 이미 있음), PRE A 6종 ([PRE_EXPANSION_A_1_4.md](PRE_EXPANSION_A_1_4.md)), 제한형 신호 순서 편집기 ([SIGNAL_ORDER_1_4.md](SIGNAL_ORDER_1_4.md), `claude/signal-order-editor`, 10/29 추가) | — | — |
| **1.5** | PRE B 6종, 드래그 순서 UX, 별도 게이트로 M/S·외부 사이드체인·6밴드 확장 | PRE B 6종 ([PRE_EXPANSION_B_1_5.md](PRE_EXPANSION_B_1_5.md)), M/S·스테레오 링크 ([MULTIBAND_STEREO_1_5.md](MULTIBAND_STEREO_1_5.md)), 외부 사이드체인 ([SIDECHAIN_SYNTH_BUS.md](SIDECHAIN_SYNTH_BUS.md)), 드래그 순서 UX ([SIGNAL_ORDER_1_4.md](SIGNAL_ORDER_1_4.md), `claude/drag-order-ux`, 10/29 추가) | **6밴드 다이내믹스 확장** | SYNTH 전용 출력 버스 (1.10 신스용, 사이드체인과 같은 버스 작업에 포함) |
| **1.6** | 레퍼런스 앰프 5종, 공통 3밴드 새추레이션 | 전부 ([REFERENCE_AMPS_1_6.md](REFERENCE_AMPS_1_6.md), [SATURATION_1_6.md](SATURATION_1_6.md)) | 매뉴얼 대조, 하드웨어 음색 일치, 사진 헤드 아트워크 | — |
| **1.7** | POST Dynamics/Tone 9종, 공통 2필터 모듈레이션 | 전부 ([POST_EXPANSION_1_7.md](POST_EXPANSION_1_7.md), [FILTER_MODULATION_1_7.md](FILTER_MODULATION_1_7.md)) | Curve Bender Multiply 스위치, 2500/2500+ 차이 | — |
| **1.8** | Motion/Space 9종, 고급 딜레이·리버브, Room Advanced 조건·공간 모델 | 9종 ([POST_MOTION_SPACE_1_8.md](POST_MOTION_SPACE_1_8.md)), Room Advanced ([ROOM_ADVANCED_1_8.md](ROOM_ADVANCED_1_8.md)), 공간 단계: 치수·재질·배치·룸 마이크 ([ROOM_SPACE_1_8.md](ROOM_SPACE_1_8.md)) | 모델별 세부 컨트롤(UltraTap Slurm/Chop, Blackhole Gravity 등), 룸 모드·지향성 | — |
| **1.9** | Creative 9종, 확장 모듈레이션, 라우팅 다듬기 | 9종과 CREATIVE 슬롯 ([CREATIVE_1_9.md](CREATIVE_1_9.md)), 랙 페이스 아트워크 | **확장 모듈레이션**(필터 LFO 추가·엔벨로프 팔로워 등), **라우팅 다듬기** | — |
| **1.10** | Dvergr Analog·Seidr Dynamic·Mimir Impact 신스 계층, 검증된 트래킹 통합 | 엔진 타입 3종, 트래킹, MIDI ([SYNTH_1_10.md](SYNTH_1_10.md), [SYNTH_PAGE.md](SYNTH_PAGE.md), [SYNTH_V2.md](SYNTH_V2.md)) | **실제 녹음 악기 트래킹 검증**(현재는 합성 신호만) | 전용 SYNTH 페이지, 오실레이터 3개, 샘플 오실레이터, 모듈레이션 매트릭스, 4개 삽입 위치 (소유자 요청) |

### 표기 차이

- **1.9 모델 이름:** 로드맵은 Slicer·HarPeggiator·Time Warp…를 쓰고, 구현은 소유자가 승인한 작업 이름 Gate Slicer·Step Harmonizer·Time Bend…를 씁니다. HarPeggiator는 공개 금지 용어로 등록돼 있습니다.

## 카탈로그 수

[Validation/catalog-count-contract.json](../Validation/catalog-count-contract.json)과 `Tools/check_catalog_contract.py`를 이번에 맞췄습니다.

- **문제:** 계약에는 PRE A/B 12종이 반영되지 않았습니다. 계약은 108(PRE 39)인데 실제 소스는 120(PRE 51)이어서 CI의 `build.yml` 검사가 실패할 상태였습니다.
- **수정:** 검사기가 CREATIVE와 SYNTH 엔진 타입도 셉니다. 계약은 **132 = AMP 30 + PRE 51 + POST 39 + CREATIVE 9 + SYNTH 3**이며, 이는 로드맵의 1.10 목표 132와 같습니다.

| 버전 | 로드맵 누적 | 통합 브랜치 |
|---|---|---|
| 1.3.3 | 85 | 85 |
| 1.4 | 91 | 91 |
| 1.5 | 97 | 97 |
| 1.6 | 102 | 102 |
| 1.7 | 111 | 111 |
| 1.8 | 120 | 120 |
| 1.9 | 129 | 129 |
| 1.10 | 132 | 132 |

## 남은 결정과 후속 작업

- **로드맵 범위에서 빠진 항목** (위 표의 굵은 글씨): 1.5 6밴드 확장, 1.9 확장 모듈레이션과 라우팅 다듬기, 1.10 실제 악기 트래킹 검증. 1.4 신호 순서 편집기와 1.5 드래그 순서 UX는 10/29에 구현했습니다.
- **공통 미완료:** 모든 버전에서 청음, DAW CPU, 설치본, 실제 호스트 확인이 남아 있습니다.
- **로드맵 갱신 여부:** 1.10에 더 들어간 SYNTH 범위와 1.5의 SYNTH 버스를 로드맵 문서에 반영할지는 소유자가 정합니다.
