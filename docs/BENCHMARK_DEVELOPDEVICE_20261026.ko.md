# 외부 벤치마크 대조 — DevelopDevice 컬렉션 (2026-10-26)

이 문서는 DevelopDevice 공개 제품 설명(개별 앱 15종·번들 1종·Manager·지원 문서) 조사에서 나온
**우선순위 6개**를 현재 `main` 코드와 대조한 결과입니다. 기능 설명은 제작사 공개 자료 기준이며,
설치·청음·CPU·지연 측정은 하지 않았습니다. 이 문서는 **제안**이고 beta.2 범위
([RELEASE_SCHEDULE_20261026.ko.md](RELEASE_SCHEDULE_20261026.ko.md))나
[DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md)를 바꾸지 않습니다.

표기: **있음** = 코드에서 동작 경로 확인 / **부분** = 일부만 있음 / **없음** = 코드에서 찾지 못함.
"없음"은 구현이 없다는 단정이 아니라 이번 검색에서 확인되지 않았다는 뜻입니다.

## 1. 우선순위별 현재 상태

### 1-1. 비교·취소·복원 공통화

| 요소 | 상태 | 근거 |
|---|---|---|
| A/B 스냅샷 (파라미터·모델·내장 IR 포함, 프로젝트에 저장) | 있음 | `Source/PluginProcessor.cpp:566-581`, `Source/PluginEditor.cpp:199-202` |
| A/B **음량 맞춤** 비교 | 없음 | gain/level/loudness match 관련 코드 없음 |
| 전체 편집 Undo | 부분 | EQ 전용 히스토리 64단계(`Source/EQEditState.h:19`), Auto Set 1회 되돌리기(`Source/AutoSetController.h:74`) 뿐. 공통 UndoManager 없음 |
| 사용자 잠금(Lock) 보존 | 없음 | Auto Set·프리셋 로드에서 개별 파라미터를 보호하는 경로 없음 |
| 재호출 후 **같은 출력** 검증 | 있음 (10/26 추가) | `Tests/StateRecallAudioTests.cpp` — 아래 1-1a |

**제안:** (a) A/B에 비교 전용 음량 보상(출력 게인과 분리, 저장하지 않음), (b) A/B·프리셋·프로젝트 복원 뒤
고정 입력으로 출력 RMS/샘플 차이를 비교하는 회귀 테스트. (b)는 "설정이 복원됐다"와 "소리가 보존됐다"를
분리하는 장치로, OBSIDIAN의 위상 모드 게인 이관 사례와 같은 문제를 잡기 위한 것입니다.

#### 1-1a. 출력 동일성 회귀 테스트 (`ChimeraStateRecallAudioTests`)

같은 Drop A 팜뮤트형 고정 입력(0.64초, 48 kHz/256)을 매번 새 `prepareToPlay` 뒤 렌더해 비교합니다.
기준은 차이 에너지 -120 dB 이하이며, 실제 결과는 대부분 비트 단위 동일입니다.

| 경로 | 결과 |
|---|---|
| 팩토리 프리셋 6종(각 계열)을 다른 프리셋·편집 위에 다시 로드 / 같은 프리셋 재로드 | -300 dB (동일) |
| A/B 전환·반복 토글·A/B 복사 | -300 dB |
| 프로젝트 저장→새 인스턴스 복원 (활성 B, 저장된 A, 2세대 재저장) | -147 ~ -163 dB (파라미터 직렬화 반올림, 최대 3e-8) |
| 재생 중이던 인스턴스에 프로젝트 복원 | -300 dB |

팩토리 프리셋은 `input`·템포·튜너 등 연주 설정과 비활성 페달 뱅크를 세션 값으로 유지하도록 설계되어 있어
(`Source/FactoryNativeVoicing.h:218-252`), 프리셋 경로에서는 이 값들을 편집하지 않습니다. 전체 상태 복원
경로(A/B·프로젝트)는 `input`까지 포함해 검사합니다.

**테스트가 찾은 결함 (수정함):** Reverb가 켜진 소리는 한 번 처리한 인스턴스를 다시 `prepareToPlay`하면
첫 약 20 ms가 새 인스턴스와 달랐습니다(-55 dB). `juce::dsp::Reverb`가 prepare 때 내부 스무더를 *직전 목표값*으로
스냅하기 때문입니다. `SpaceModule::prepare`에서 prepare 전에 기본 파라미터로 되돌려, 새 인스턴스의 첫 렌더와
같은 시작점을 갖게 했습니다(`Source/SpatialModules.h`). 새 인스턴스의 소리는 바뀌지 않고, DAW가 버퍼 크기·
샘플레이트 변경이나 오프라인 바운스로 재-prepare할 때의 결과만 새 인스턴스와 일치하게 됩니다.

### 1-2. 전환과 지연 검증

| 요소 | 상태 | 근거 |
|---|---|---|
| 채널·앰프 전환 크로스페이드, 연속 전환 대기열 | 있음 | `Source/NewAmpDSP.h:323`, `Source/NiflheimrDSP.h:70`, `Source/AmpNativeDSP.h:394` |
| PDC 보고·동적 갱신 | 있음 | `Source/PluginProcessor.cpp:153`, `:279-280` |
| 진단 출력에 보고 지연 포함 | 있음 | `Source/PluginProcessor.cpp:820` |
| 전환 **요청→반영 시간**, 전환 순간 CPU·클릭, 이전 CAB 잔존 측정 | 부분 | CAB 타이밍([CAB_REALTIME_TIMING_AUDIT.md](CAB_REALTIME_TIMING_AUDIT.md))·Transpose PDC([TRANSPOSE_T6_PDC_CONTRACT.md](TRANSPOSE_T6_PDC_CONTRACT.md))는 있으나 채널 전환 전용 지표 없음 |

**제안:** 채널 전환을 "끊김 없음"과 "응답 시간" 두 지표로 나눠 기록. SMASHPLATE의
약 100ms 준비 + 20ms 크로스페이드처럼 수치로 공개할 수 있게 측정 항목을 정의.

### 1-3. Auto Set의 설명과 통제

| 요소 | 상태 | 근거 |
|---|---|---|
| 5단계 진행, 단계별 결과 문구 | 있음 | `Source/AutoSetController.h:20-29` |
| 유효 연주량(들은 초/필요 초) 표시 | 있음 | `Source/AutoSetController.h:29`, `:270-273` |
| 무음 시 실패 처리 | 있음 | `Source/AutoSetController.h` `tick()`의 `noAudioTimeoutMs` |
| 마지막 실행 되돌리기 | 있음 | `Source/AutoSetController.h:74` (수동 수정 덮어쓰기 문제는 #156에서 수정 중) |
| 결과 **강도** 조절 | 없음 | |
| 적용 전 **미리듣기** | 없음 | 결과가 바로 파라미터에 반영됨 |
| 제외 구간·목표 미달 이유 표시 | 부분 | 진행 중 측정량은 보이지만 제외 구간·한계는 표시하지 않음 |
| 개별 설정 잠금(ContextVerb식 Lock/Hold) | 없음 | |

**제안 순서:** 잠금 → 강도 → 미리듣기. 잠금은 #156의 "수동 수정 보호"와 같은 축이라 이어서 하기 쉽고,
강도는 기존 결과값을 현재값과 보간하면 되므로 DSP 변경이 없습니다.

### 1-4. CAB 편집·내보내기

| 요소 | 상태 | 근거 |
|---|---|---|
| IR 로드·라이브러리·내장 저장 | 있음 | `Source/IRLibrary.cpp`, `Source/IRCollection.h` |
| 블렌드 결과 **WAV 내보내기** | 없음 | WAV 쓰기 경로 없음 |
| 후보 잠금·대역 보호·Tone Match | 없음 | |

**제안:** 가장 작은 단위는 "현재 CAB 블렌드를 WAV로 저장 → 다시 불러와 프리뷰와 출력 비교" 왕복 테스트.
CarveIR·IR MatchMaker식 분석은 실시간 엔진과 분리된 **CAB 분석·설계 도구**로 두는 것을 권합니다.
CAB IR은 선형 응답이므로 NAM 앰프 처리를 포함하지 않는다는 점을 UI 문구에 명시해야 합니다.

### 1-5. Deadwire MIDI 검증

Chimera는 MIDI를 받기만 하고(`acceptsMidi`), 생성하지 않습니다(`Source/PluginProcessor.h:58-60`).
현재 MIDI는 CC learn뿐이며 프로젝트에 저장됩니다(`Source/PluginProcessor.cpp:171`, `:509`, `:563`).
ShredSync·HumBeat·MODALITH 계열 비교는 **Chimera 범위 밖**이고, 별도 MIDI·편곡 도구 또는 Deadwire 제작
작업에서 다룹니다. 100 BPM 기준 파일 검증안은 그 도구의 첫 테스트로 넘깁니다.

### 1-6. 배포·지원

| 요소 | 상태 | 근거 |
|---|---|---|
| 지원 리포트 초안 (버전·OS·오디오 설정, 경로·장치명·오디오 제외) | 있음 | `Source/SupportPanel.h:42`, `Source/ReleaseSupport.h:43-46` |
| 본체·OBS·설명서 버전 조합 표시 | 확인 못함 | |
| 상태 스키마 버전·이관 | 있음 | `schemaVersion` 10 (`Source/PluginProcessor.cpp:476`), 호스트 인덱스 보존 규칙(`:444`) |

## 2. Doubler 비교 (DupTraX 3 대비)

현재 Doubler(`Source/PerformanceUtilities.h`)는 **앰프 뒤**(`Source/PluginProcessor.cpp:328`)에서 동작하는
스테레오 지연 디코릴레이션입니다. 좌 채널은 설정 시간의 0.55배, 우 채널은 1배, ±0.25ms 변조, 혼합 35% 고정,
스테레오 버스에서만 켜집니다.

DupTraX는 **앰프 앞** 클린 DI에서 테이크별 시간·피치·강도를 바꾸고 테이크마다 독립 앰프 처리를 합니다.
구조가 다르므로 기능 비교보다 아래 측정이 먼저입니다.

- 모노 합산 시 콤 필터 깊이(특히 Drop A 저역·팜뮤트 어택)
- 원신호 지연 없음 유지 여부(현재 코드는 dry 경로 지연 없음)
- 앰프 앞 다중 테이크 방식은 앰프 인스턴스 수만큼 CPU가 늘므로, 도입 시 CPU 예산을 먼저 정해야 함

## 3. 문구·주장 구분 규칙

외부 제품 조사에서 확인된 혼동을 Chimera 문서·UI에도 적용합니다.

- **끊김 없는 전환 ≠ 즉시 전환** — 응답 시간을 따로 표기
- **원음 무지연 ≠ 전체 출력 무지연** — Doubler·Transpose의 wet 지연을 따로 표기
- **CAB IR ≠ 앰프 캡처** — 내보낸 IR에는 비선형 처리가 없음
- **지원 형식과 예정 형식 분리** — Chimera 본체와 OBS 애드온 각각
- **원본 실명 비노출** — [PUBLIC_PRODUCT_NAMING.md](PUBLIC_PRODUCT_NAMING.md) 정책 유지. 외부 자산 출처·해시·라이선스는 내부 기록에만

## 4. 제안 후속 작업 (범위 미확정)

| 순서 | 작업 | 크기 | 비고 |
|---|---|---|---|
| 1 | A/B·프리셋·프로젝트 복원 후 출력 동일성 회귀 테스트 | 작음 | **완료** (1-1a) |
| 2 | Auto Set 개별 잠금 + 강도 | 중간 | #156 이후 |
| 3 | A/B 비교 전용 음량 보상 | 작음 | 저장하지 않는 모니터링 게인 |
| 4 | 채널 전환 응답 시간·클릭 측정 항목 정의 | 작음 | 측정 문서 먼저 |
| 5 | CAB 블렌드 WAV 내보내기 + 왕복 테스트 | 중간 | |
| 6 | Doubler 모노 합산 측정 | 작음 | 구조 변경 판단 근거 |

## 출처

제품 기능은 developdevice.com 공개 제품 페이지·사용 설명서(2026-10-26 열람) 기준입니다.
원 조사 보고서(13쪽 PDF)는 이 저장소에 포함하지 않았습니다.
