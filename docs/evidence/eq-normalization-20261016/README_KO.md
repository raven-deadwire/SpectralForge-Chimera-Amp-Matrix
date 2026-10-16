# PR #83 raw float 정규화 계약 후속 검증

**판정: 로컬 정규화·멱등성·활성 EQ 바이너리 비교 PASS. 편집 직후→최초 복원은 NOT_BIT_IDENTICAL. 최종 3-OS CI 미완료 / 실제 DAW NOT_ASSESSED. Draft / release_approved=false.**

## 원인 분석

기준은 공개 beta.2 소스 `6033236cb81820a43679cbbd34be020804f9658d`와 PR #83 `c837c7854584031bd22b0dacd4984f46c1262103`다. 두 소스는 JUCE 8.0.8(`d6181bde38d858c283c3b7bf699ce6340c050b5d`)을 사용한다. EQ parameter 정의와 오디오 계산은 동일하다. 후보의 GraphicalEQ 변경은 analyzer FIFO이며, restoreCore에 추가된 EQ RestoreScope는 점유/history metadata를 처리한다.

JUCE `AudioProcessorValueTreeState::ParameterAdapter`의 생성자는 `range.convertFrom0to1(parameter.getDefaultValue())`를 사용해 **스냅하지 않은** raw cache를 만든다. 일반 RangedAudioParameter의 `convertFrom0to1`은 결과를 스냅한다. `setDenormalisedValue`는 저장값과 현재 cache가 `approximatelyEqual`이면 대입을 생략한다. `flushToTree`도 같은 방식으로 가까운 기존 저장값을 유지한다. 따라서 저장값, DSP raw cache, host normalized 값은 별개다. `R(x)` 한 가지 표현만으로 복원 raw를 판정한 기존 검사는 이 초기화/대입 생략 경로를 포함하지 않았다.

원본 71회 관측은 모두 이 생성자 값과 비트가 같고 기존 대입 생략 조건을 만족했다(`constructor-formula.log`, mismatch 0). 실제 후보의 기존 상태 검사에서도 동일한 71회를 `RETAINED_CONSTRUCTOR`로 재현했고 실패는 0이었다. 임의의 1 ULP 이웃을 포괄적으로 허용하지 않는다. 제품 Source, JUCE 패치, 파라미터 범위/ID/순서, 프리셋, 버전, 스키마를 변경하지 않는다.

## 수용 계약 구분

| 대상 | 새 검증 계약 |
| --- | --- |
| 파라미터 ID/순서/default/steps/flags, 호스트 정규화 값 | 정확한 일치. 기존 7,048 wrapper / 4,967 product / 122 EQ 검증 재사용 |
| 직접 편집의 변경 대상 외 raw | 기존 비트 일치 그대로 |
| 복원 raw | 정확한 저장값 또는 기존 RangedAudioParameter 왕복값. 추가로 EQ frequency/Q 및 delaymix에서 **실제 JUCE constructor 식과 비트가 같은 값**에 한해, 초기 PARAM tree 생성과 복원 양쪽의 기존 대입 생략 조건 및 정규화 값 비트 일치가 모두 성립할 때 인정 |
| 같은 정규화 값을 갖는 임의의 다른 raw 값 | 불허. 음성 대조(negative control)로 검사 |
| 첫 저장 이후 반복 복원 | 새 인스턴스 8회·같은 인스턴스 8회: 전체 raw/host 비트 및 XML 의미 완전 일치. A/B 한 왕복 후 8회 고정점 별도 검사 |
| 결정적 EQ 오디오 | 네 sample rate × 두 block size, A/A 및 canonical reload의 모든 샘플 비트 일치. 허용 오차 없음 |
| 실제 VST3 오디오 | SHA-256 고정 공개 beta.2와 후보의 A/A, 후보→beta.2→후보, 구버전 숫자 lane 큐 비교. 48 kHz/256 stereo, Classic의 EQ를 분리한 테스트 입력. 오디오/호스트 값/지연/저장 상태 정확히 비교 |
| 편집 직후→최초 새 인스턴스 raw/오디오 | 관측 수치 별도 기록. canonical fresh-load와 혼동하거나 잔차를 허용 오차 PASS로 바꾸지 않음 |
| 실제 DAW/프로젝트/자동화 모드/개인 IR/청음 | NOT_ASSESSED. 자동 ABI 호스트를 Studio One/Cubase/Cakewalk 수용으로 간주하지 않음 |

초기 PARAM tree 생성에서도 snapped default를 쓴다. 그 값과 constructor cache가 충분히 가깝지 않으면 실제 대입이 일어나므로 constructor 표현을 예외로 인정하지 않는다. 예: GCC 13의 30 Hz 초기 계산 `29.9999961853`은 최종 fresh raw `30`으로 바뀐다. 이 조건을 추가해 도달하지 않는 초기 표현을 승인하지 않는다.

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

## 실제 로컬 실행 및 검사기 보강

Ubuntu 24.04 x86_64 / GCC 13.3 / JUCE 8.0.8. 제품 빌드 revision 표시는 `c837c78545`이며 Source tree는 PR #83 그대로다. 이번 검사기 소스의 SHA-256과 실제 바이너리 digest는 별도 execution-summary에 기록한다. 로컬 결과를 미완료 3-OS CI의 결과로 대체하지 않는다.

- 기존 공개 fixture 무결성 및 기존 Python controls를 재사용했다. 기존 상태 검사 3/3 PASS, 71회 constructor 유지 관측 재현.
- 공개 deb와 VST3 SHA-256은 원본과 일치. PR #83의 `roundtrip-states.zip`에 보존된 삭제/재사용/자동화 상태를 재사용했다.
- **활성 EQ 실제 VST3 비교 15/15 PASS**: 세 상태 각각 candidate A/A, beta.2 A/A, candidate→beta.2, candidate→beta.2→candidate, 기존 숫자 lane 큐. 21회 실제 모듈 실행, 비교당 stereo 32,768개 float 샘플. 오디오 비트·7,048 normalized 항목·지연·전체 XML 상태 의미가 동일하다. 중간 블록 큐가 재사용 밴드의 오디오를 실제로 바꾼다는 positive control도 PASS.
- 초기 ABI fixture는 원래 A 상태의 EQ bypass=true를 그대로 유지했다. 초기 15회 결과는 `initial-bypassed-render-summary.json`으로 보존하며 **활성 EQ 오디오 근거로 사용하지 않는다**. 후속 시험용 복사본에서 양쪽 EQ bypass를 끄고 재실행했다. 최종 결과는 `abi-render-summary.json`이다. 원본 fixture의 band 값·메타데이터를 수정하지 않았다.
- constructor 공식이 모든 파라미터의 최종 fresh cache와 같다는 초기 검사기 가정은 성립하지 않았다. 초기 tree 대입 조건을 분리하고, 예외 범위는 관측된 EQ frequency/Q와 delaymix 계열로 한정했다. 원래 저장값/왕복값의 정확한 검사는 유지한다. 초기 검증 실패는 `initial-constructor-domain-failure.log`에 보존한다.
- 로컬 빌드에서 artwork 중간 객체 3개가 0-byte로 생성돼 첫 링크가 실패했다. 그 세 중간 객체만 재생성했다. 제품 자산·소스·컴파일 옵션을 변경하지 않았다.

[ABI 결과·바이너리 해시](abi-render-summary.json) · [기존 상태 검사](local-legacy-results.xml) · [초기값 71회 대조](constructor-formula.log)

실제 Studio One/Cubase/Cakewalk 수용은 기존 [DAW 세션 명세](../../EQ_LEGACY_COMPATIBILITY_20261016.md)를 그대로 적용한다. 기술적 정규화 계약 통과는 공개 릴리스 승인과 별개다.

## 최종 로컬 결과

- **CTest 5/5 PASS, 120.88초.** 원본 fixture·기존 상태·기존 evidence·신규 normalization·신규 render controls 검사. [JUnit](local-final-results.xml), [CTest 요약](local-final-ctest.log), [전체 출력](local-final-test-output.log).
- **71회 / 35개 파라미터** 모두 좁은 constructor-retention 조건으로 설명됐다. 변경 대상 외 직접 편집 raw 비트 검사는 그대로다.
- **12개 상태 × fresh/same-instance/A/B 각각 8회** 고정점 검증 PASS. 저장 XML 의미 전체, 4,967 raw/host 값은 이후 반복에서 안정적이다.
- **96개 EQ 렌더 경로**(12상태 × 44.1/48/96/192 kHz × 32/256 samples), 각 경로 A/A 및 canonical fresh restore 비교: **192쌍 × 32,768샘플 비트 일치**. 임의 허용 오차를 사용하지 않는다.
- **실제 공개 beta.2↔후보 활성 EQ ABI 15/15 PASS**, 앞 절의 범위에 한한다.

### 최초 복원의 잔차는 0이 아니다

| sample rate | 서로 다른 샘플 / 32,768 | 최대 절대 잔차 | 판정 |
| --- | ---: | ---: | --- |
| 44.1 kHz | 7,596 | 1.4901161193847656e-8 | NOT_BIT_IDENTICAL |
| 48 kHz | 7,238 | 1.4901161193847656e-8 | NOT_BIT_IDENTICAL |
| 96 kHz | 6,136 | 1.4901161193847656e-8 | NOT_BIT_IDENTICAL |
| 192 kHz | 5,154 | 1.4901161193847656e-8 | NOT_BIT_IDENTICAL |

이는 테스트용 1234 Hz / +6 dB / Q 0.70710678의 실제 EQ DSP에서, 호스트 편집으로 얻은 snapped raw와 최초 새 processor 복원의 retained raw를 비교한 결과다. 호스트 normalized 값은 정확히 같다. **내부 1 ULP가 DSP 출력 비트에 영향을 줄 수 있음을 확인했으므로 “영향 없음”이나 “모든 상태가 비트 동일”로 결론내리지 않는다.** beta.2와 후보의 해당 parameter/APVTS/필터 계산 경로는 동일하고, 같은 저장 상태를 양쪽 실제 바이너리에서 읽어 얻은 출력은 비트 동일했다. 신규 EQ의 추가 회귀로 판단할 증거는 없다. 청감은 측정하지 않았다.

기술적으로 수용할 범위는 **기존 serialized/normalized 계약 + 정의된 fresh-load/반복복원 결정성**이다. 편집 이력을 포함한 모든 내부 raw/오디오 비트의 영구 보존을 요구하려면 별도 제품 변경과 beta.2 호환성 검토가 필요하며, 이 PR에서 그런 변경을 하지 않는다.

[소스·실행 바이너리 SHA-256 및 결과 JSON](execution-summary.json) · [beta.2/후보 관련 소스 차이](beta2-candidate-source.patch)

3-OS 최종 CI는 별도로 추적하며 실행이 끝나지 않은 job은 PASS로 기록하지 않는다. 로컬 기술 검증의 성공으로 실제 DAW 수용이나 공개 배포를 승인하지 않는다.
