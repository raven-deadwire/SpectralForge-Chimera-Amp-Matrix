# EQ 구버전 바이너리 상태 호환성 — 2026-10-16

대상: PR #59 `7ef99333ce14315538ec4aed350d802ca0d78e23`, Issue #53. 10월 23일은 조건부 개발 목표다. 이 변경은 **테스트·증거 수집·문서만** 추가하며 제품 Source, 호스트 파라미터, DSP, 프리셋, 상태 스키마 및 배포 계약을 변경하지 않는다. `release_approved=false`.

## 재사용한 기준

`GraphicalEQEditingTests.cpp`의 기존 4,967/122 파라미터 계약, metadata가 없는 상태의 12슬롯 보존, enabled와 occupancy 구분, 삭제·재사용, A/B 분리, history boundary 및 정규화 왕복 비교를 그대로 유지한다. `EQParameterAssertions.h`의 canonical float bit 비교를 재사용한다. 기존 synthetic legacy/malformed 검사를 실제 바이너리 검사로 재분류하지 않는다. Snapshot Freeze의 기존 `ChimeraEQSnapshotFreezeTests`도 함께 실행한다.

## 확보한 상태의 출처와 한계

공개 `v1.3.1-beta.2` Linux x86_64 deb를 다운로드하고 릴리스 digest와 payload manifest의 VST3 digest를 모두 확인했다.

- 배포 소스: `6033236cb81820a43679cbbd34be020804f9658d`
- deb SHA-256: `5f7b98efbae81715b40bc572ae6bbaf02ad40ea1f91163110a9ed6469823a7ab`
- VST3 SHA-256: `a7be4d98614897b9dd98254f39ba919f5520fdde4649cc73e98a56d12feef486`
- 릴리스: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.1-beta.2

`ChimeraVST3EQStateHost`는 제품/JUCE에 링크하지 않는 작은 Linux VST3 호스트다. 기존 `VST3LifecycleTests.cpp`의 stream/message/parameter queue 계약을 재사용한다. 실제 배포 모듈의 public ABI를 열고, processor/controller 연결, 48 kHz/256 stereo process queue, `IComponent::getState`를 사용한다. DAW 이름을 사칭하지 않는다.

| Fixture | 실제 생성 경로 | 구분 |
| --- | --- | --- |
| `default.chimera` | 공개 바이너리 초기화 → 실제 getState | 실제 바이너리 기본 상태 |
| `pattern.chimera` | 122 EQ 필드의 실제 VST3 parameter queue와 output/gate/delay/reverb 값 변경 → getState | 실제 바이너리 자동화 입력 상태. 기본값 대비 EQ 108개·비EQ 4개 값이 다름 |
| `ab-seeded.chimera` | 위 두 실제 상태를 A/B 입력으로 구성 → 공개 바이너리 setState → getState | **호스트가 구성한 A/B 입력**. DAW UI에서 A/B를 클릭해 만든 세션은 아님 |

`.chimera`는 실제 VST3 component state에서 JUCE private wrapper trailer만 제거한 processor 원본 bytes다. XML과 값은 재작성하지 않는다. `PluginEditor.cpp`의 `.chimera` 저장도 동일한 processor serializer를 쓴다. 그러나 이 수집은 **파일 대화상자를 통한 export/import 검사 또는 DAW 네이티브 프로젝트 저장 검사가 아니다**. 원본 component bytes와 host-seeded A/B 입력은 실행 artifact에 보존하며, 반복 회귀에 사용하는 세 파일·파라미터 표·lane ID·해시는 `Tests/fixtures/eq-beta2`에 보관한다. 실제 기타/베이스 IR 파일을 삽입하지 않은 fixture이므로 개인 IR payload 수용을 추가 주장하지 않는다.

## 새 자동 계약

- 실제 fixture의 4,967개 값 전체를 원본 저장값 또는 기존 정규화 왕복값과 정확한 float bits로 비교하고, 원래 값과의 수치 의도도 확인한다. 구버전 데이터의 float를 임의로 보정하지 않는다.
- Tone/Final 각각 12슬롯 모두 점유 상태로 복원하고, disabled 슬롯을 빈 슬롯으로 재사용하지 않으며 13번째 생성을 거부한다.
- Tone 4번·Final 8번을 삭제하고 Undo/Redo한 뒤 다른 4,965개 값을 그대로 유지한다. 삭제된 상태를 파일 저장하고 processor를 파괴한 뒤 새 processor로 재연다.
- 삭제된 슬롯의 frequency automation은 점유를 되살리지 않으며 enabled=true는 동일 슬롯을 되살린다. 다시 삭제 상태로 복원하고 같은 빈 슬롯을 재사용한다. 번호 압축과 다른 슬롯 이동을 금지한다.
- 재사용 상태를 저장·파괴·재열기하고 구버전 lane을 같은 ID에 재생한다. 변경 허용 대상 외 값은 bit-identical이어야 한다. 실제 VST3 wrapper 검사에서는 beta.2가 노출한 **숫자 ParamID**를 그대로 큐에 넣는다.
- 전역 A/B의 비활성 B와 그 DSP 값·점유 상태를 보존하고, A/B 전환·프로젝트 복원에서 지역 이력을 비운다. USER_IRS/MIDI_MAP의 저장 내용을 비교한다.
- candidate → 공개 beta.2 → 재저장에서 파라미터·점유 메타데이터·A/B·보조 상태 보존을 별도로 비교한다. 구버전의 UI는 모든 12슬롯을 표시하므로 UI 표시 동일성을 주장하지 않는다.
- VST3 wrapper가 노출한 전체 7,048개 항목의 ID·순서·기본값·step·flags를 비교한다. 이 수는 제품 4,967개에 JUCE MIDI/래퍼 항목이 더해진 것이며 제품 파라미터 추가가 아니다.

실행 기록: [한국어 결과와 증거](evidence/eq-beta2-compatibility-20261016/README_KO.md). 로컬/CI 결과, 기존 synthetic PASS, 실제 바이너리 PASS, 실제 DAW NOT_ASSESSED를 합산하지 않는다.

## 실제 DAW 수용 세션 — 모두 별도 실행 필요

원본 beta.2 설치본과 후보 설치본을 **격리된 플러그인 경로/세션 복사본**에서 사용한다. 동일 ID의 두 버전을 동시에 스캔하게 두지 않는다. 원본 프로젝트를 덮어쓰지 않는다. 각 세션에 OS/build, DAW 정확한 버전, 형식(VST3/AU), binary/package SHA, source SHA, 인터페이스/드라이버, sample rate, buffer, UTC 시각, 녹화·로그·프로젝트·파일의 해시를 남긴다.

| 세션 | 준비와 실행 | 통과 조건 / 필요한 결과물 |
| --- | --- | --- |
| LEGACY-PROJECT | **공개 beta.2 자체**로 Studio One 프로젝트와 `.chimera`를 저장. Tone/Final 12슬롯에 서로 다른 값, enabled/disabled 혼합, A/B별 다른 EQ·앰프·게이트·라우팅·PRE/POST와 개인 IR 사용. 원본 보관 후 후보에서 load→수정→다른 이름 저장→호스트 종료→재열기 | EQ 122개 값·고정 슬롯·삭제 상태·A/B·비EQ·개인 IR 해시 보존. 원본/수정/재열기 세션 및 `.chimera`, parameter dump와 화면 기록 |
| READ | beta.2에서 Tone 4/Final 8의 frequency/gain/enabled lane을 기록. 후보에서 재생·루프 중 삭제→tuning만 재생→enabled 재생→삭제→같은 슬롯 재사용 | tuning만으로 삭제 슬롯이 재표시되지 않음. enabled로 동일 슬롯 복구. 재사용 후 기존 lane은 같은 번호를 제어하고 주변 슬롯 불변. 재생과 재열기 후 동일 |
| TOUCH | 자동화 중 그래프/수치 드래그, 밴드 삭제·재생성, 지역 Undo/Redo, 손을 떼고 기존 lane 복귀 | begin/end gesture 균형, 호스트 복귀 동작, 원하지 않은 lane 기록 없음. 외부 EQ 알림으로 지역 Undo가 무효화되는 기존 계약을 확인 |
| LATCH | 재생 중 수치/드래그로 값 latch, 삭제·재사용, 정지·재생·루프 | DAW가 지원하는 latch 종료/재생 정책과 실제 lane 기록 확인. 다른 슬롯과 비EQ 상태 불변 |
| WRITE | 세션 복사본에서 Write로 생성·삭제·여러 파라미터 편집 후 모드 해제·재생·저장·재열기 | 실제 작성된 envelope와 gesture 종료 확인. 자동화 트랙을 손실하거나 다른 ID로 이동하지 않음 |
| HOST-UNDO | 동일 변경에 플러그인 Undo와 DAW Undo를 각각 실행. 호스트의 parameter별 기록과 EQ의 단일 편집 작업을 구분 | DAW timeline 손실·stuck gesture·외부 값을 덮어쓰는 동작 없음. multi-parameter sample-exact atomicity는 요구하지 않음 |
| FREEZE-SESSION | 재생 중 freeze→EQ 변경→저장, 정지 중 thaw→재개, offline render, editor close/reopen | 오디오 계속 처리, 이전 FIFO 재사용 없음, 새 전체 프레임까지 대기. Freeze 캡처/지역 Undo는 저장되지 않으며 DSP/A-B에는 영향 없음 |
| HOST-LIFETIME | 다중 인스턴스·A/B·프리셋 변경·창 닫기/열기·프로젝트 닫기·플러그인 제거·DAW 종료 | Studio One 우선. 기존 신고가 있는 Cakewalk Sonar/Cubase 10.5 종료도 별도 실행. 실제 지원 OS/형식에서 반복 |

이 환경에는 실제 Studio One/Cakewalk/Cubase 세션과 오디오 인터페이스가 없다. Read/Touch/Latch/Write, 호스트 Undo, 네이티브 프로젝트 호환성, 실제 파일 대화상자·청음·설치본 수용은 **NOT_ASSESSED**다. 특정 호스트에 모드가 없으면 대체 모드를 임의로 PASS 처리하지 말고 `UNSUPPORTED`와 해당 호스트의 실제 지원 모드를 기록한다. 위 자동 검사는 호스트 timeline/UI/운영체제별 설치 수용을 대체하지 않는다.

최종 수용 전 Draft 유지. merge, tag, 공개 release, installer 배포는 수행하지 않는다.
