# PR #83 raw float 정규화 계약 후속 검증

상태: 검증 진행 중. Draft / release_approved=false. 실제 DAW NOT_ASSESSED.

## 원인 분석

기준은 공개 beta.2 소스 `6033236cb81820a43679cbbd34be020804f9658d`와 PR #83 `c837c7854584031bd22b0dacd4984f46c1262103`다. 두 소스는 JUCE 8.0.8(`d6181bde38d858c283c3b7bf699ce6340c050b5d`)을 사용한다. EQ parameter 정의와 오디오 계산은 동일하다. 후보의 GraphicalEQ 변경은 analyzer FIFO이며, restoreCore에 추가된 EQ RestoreScope는 점유/history metadata를 처리한다.

JUCE `AudioProcessorValueTreeState::ParameterAdapter`의 생성자는 `range.convertFrom0to1(parameter.getDefaultValue())`를 사용해 **스냅하지 않은** raw cache를 만든다. 일반 RangedAudioParameter의 `convertFrom0to1`은 결과를 스냅한다. `setDenormalisedValue`는 저장값과 현재 cache가 `approximatelyEqual`이면 대입을 생략한다. `flushToTree`도 같은 방식으로 가까운 기존 저장값을 유지한다. 따라서 저장값, DSP raw cache, host normalized 값은 별개다. `R(x)` 한 가지 표현만으로 복원 raw를 판정한 기존 검사는 이 초기화/대입 생략 경로를 포함하지 않았다.

이 소스 분석은 초기값과 실제 관측의 비트 대조 및 렌더 결과로 확인해야 한다. 임의의 1 ULP 이웃을 포괄적으로 허용하지 않는다. 제품 Source, JUCE 패치, 파라미터 범위/ID/순서, 프리셋, 버전, 스키마를 변경하지 않는다.

## 수용 계약 구분

| 대상 | 새 검증 계약 |
| --- | --- |
| 파라미터 ID/순서/default/steps/flags, 호스트 정규화 값 | 정확한 일치. 기존 7,048 wrapper / 4,967 product / 122 EQ 검증 재사용 |
| 직접 편집의 변경 대상 외 raw | 기존 비트 일치 그대로 |
| 복원 raw | 정확한 저장값 또는 기존 RangedAudioParameter 왕복값. 추가로 **실제 JUCE constructor 식과 비트가 같은 값**에 한해, 기존 JUCE 대입 생략 조건과 정규화 값 비트 일치가 모두 성립할 때 인정 |
| 같은 정규화 값을 갖는 임의의 다른 raw 값 | 불허. 음성 대조(negative control)로 검사 |
| 첫 저장 이후 반복 복원 | 새 인스턴스 8회·같은 인스턴스 8회: 전체 raw/host 비트 및 XML 의미 완전 일치. A/B 한 왕복 후 8회 고정점 별도 검사 |
| 결정적 EQ 오디오 | 네 sample rate × 두 block size, A/A 및 canonical reload의 모든 샘플 비트 일치. 허용 오차 없음 |
| 실제 VST3 오디오 | SHA-256 고정 공개 beta.2와 후보의 A/A, 후보→beta.2→후보, 구버전 숫자 lane 큐 비교. 48 kHz/256 stereo, Classic의 EQ를 분리한 테스트 입력. 오디오/호스트 값/지연/저장 상태 정확히 비교 |
| 편집 직후→최초 새 인스턴스 raw/오디오 | 관측 수치 별도 기록. canonical fresh-load와 혼동하거나 잔차를 허용 오차 PASS로 바꾸지 않음 |
| 실제 DAW/프로젝트/자동화 모드/개인 IR/청음 | NOT_ASSESSED. 자동 ABI 호스트를 Studio One/Cubase/Cakewalk 수용으로 간주하지 않음 |

“정규화”는 모든 내부 raw를 하나의 값으로 강제 변환하는 제품 변경이 아니다. 저장값의 안정성, 생성자 상태를 고정한 복원, A/B 이력을 고정한 복원을 각각 검사한다. 이력과 무관한 유일 raw 표현을 주장하지 않는다.

## 기존 증거 재사용

[PR #83 원본 보고서](../eq-beta2-compatibility-20261016/README_KO.md), 원본 세 fixture와 digest, 71회/35개 파라미터 원시 관측, 3-OS 실패 로그, 기존 Editing/Freeze와 ABI 역방향 결과를 보존한다. 이전 실패를 새 PASS로 덮어쓰지 않는다. 이번 실행 로그/소스 digest/CI는 별도 기록한다.

## 재현

```sh
python3 Tools/eq_legacy_fixture.py verify --output Tests/fixtures/eq-beta2
cmake -S . -B build-legacy -DCMAKE_BUILD_TYPE=Release -DCHIMERA_EQ_DEVELOPMENT_PREVIEW=ON
cmake --build build-legacy --config Release --target ChimeraEQLegacyStateTests ChimeraEQNormalizationTests ChimeraVST3EQStateHost ChimeraAmpMatrix_VST3 --parallel 2
ctest --test-dir build-legacy -C Release -R '^(ChimeraEQLegacy.*|ChimeraEQNormalization.*)$' --output-on-failure --no-tests=error
```

실제 공개 바이너리 다운로드/digest/ABI 렌더 명령은 `.github/workflows/eq-legacy-state.yml`에 포함한다. 빌드·검사 실패는 nonzero로 유지하고 결과물은 always 업로드한다. 병합·태그·공개 배포는 하지 않는다.
