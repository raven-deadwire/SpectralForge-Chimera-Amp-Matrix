# Auto Set 오탐·설정 보존 검토 — 2026-10-25 KST

대상 기준선: main `95dbd8b96a5b952c2a96c6ea95ff9bbc43c05408`.
PR #153의 병합 커밋은 `45de2782d77fa074fa3b9e13626ff7275b7ddff5`.
이 보고서와 같은 커밋의 Source/AutoSet.h, Source/AutoSetController.h 및
Tests/AutoSetTests.cpp가 수정 후보와 재현 절차다. 실제 DI 청감 품질 승인은 **보류**한다.

## 재현 결과와 적용 범위

| 항목 | 확인 결과 |
|---|---|
| Drop A 기타 모델 입력 | 55 / 65.406 / 82.407 Hz 배음 신호를 Auto가 베이스로 분류 |
| 5현 Low B 베이스 모델 입력 | 30.868 / 41.203 / 55 Hz는 베이스로 분류 |
| 6현 베이스 고음역 모델 입력 | 130.813 / 164.814 / 196 Hz를 Auto가 기타로 분류 |
| 보존 대상으로 정의한 음의 시작 어택 | 기존 판정은 36/36에서 활성화, 설정 range 9 dB, 실제 DeEsserCore 최대 감쇠 8.21976~8.95649 dB |
| 지속적인 고역 과다 / 독립 고역 클릭 | 수정 후에도 검출되는 양성 대조군으로 별도 검사 |
| 수동 설정 / UNDO | 미변경 항목까지 복원하던 전체 스냅샷 문제를 수정. 실행 중·이후 바뀐 값은 보존 |
| CI | Graphical EQ workflow에 누락된 ChimeraAutoSetTests 빌드·실행을 추가 |

36개 A/B 입력은 44.1/48/96 kHz × 보통/빠른 감쇠 × tanh 클리핑 유무 ×
Rock/Clean/Metal이다. 입력은 Rock 목표 곡선의 위상 난수 합성 신호에 250 ms마다
8 ms 이하의 감쇠하는 6 kHz 어택을 더했다. seed=417, 기본 감쇠율 12/s,
팜뮤트 대용 감쇠율 45/s, 클리핑은 `0.25*tanh(12*x)`다.
`--risk-probe`가 동일 입력을 재생성하며, 실제 제품 DeEsserCore에 샘플 단위로
통과시켜 최대 감쇠를 기록한다. 이 입력은 실물 악기/피킹의 물리 모델이나 청감 정답이 아니다.

별도 회귀 검사는 각 스타일 자체의 목표 신호, seed 417/991, 두 번째 seed의
3.7 ms 시작 오프셋을 사용한다. 72개 어택 보호, 18개 지속 고역 과다,
36개 명시적 악기 선택, 9개 자동 모드 목표 충돌 조건을 포함한다.
기존 입력 게인·게이트·컴프레서·Tone EQ·고역 클릭·캡처 검사도 유지한다.

## 원인 및 변경

1. **분류의 구조적 한계**: 분석 위치는 raw DI가 아니라 앰프/캐비넷 이후다.
   35~71 Hz 대 71 Hz~1.4 kHz 에너지 비율 −12 dB 하나로 결정하므로,
   저음 기타·베이스 고음역·캐비넷 필터·디스토션 배음에 따라 결과가 바뀐다.
   실제 악기 식별 정확도가 개선됐다고 주장하지 않는다. Tone EQ의 기존 추정과
   스타일 곡선은 유지하고, 결과 표시를 estimate로 명시한다.
   De-esser Auto 모드는 기타/베이스 양쪽 정렬 목표를 모두 넘는 고역만
   스펙트럼 초과로 인정한다. Tone EQ의 추정값을 명시적 선택으로 전달하지 않는다.
   수동 Guitar/Bass는 기존 선택 곡선을 그대로 사용한다.
2. **어택과 거친 고역 혼동**: 기존은 고역·전체의 서로 다른 95백분위 값을 빼며,
   어택을 포함한 평균 스펙트럼만으로도 활성화했다. 수정 후 같은 시간창의 상승량을
   먼저 비교한다. 전체와 1.5 kHz 이하 몸통이 함께 상승한 음의 시작 주변을 보호하고,
   보호 구간과 겹치지 않는 원래 연속 FFT 프레임으로 지속 고역을 평가한다.
   충분한 프레임이 없으면 해당 스펙트럼 판정을 보류한다. 오디오를 잘라 이어 붙이지 않는다.
3. **수동 설정 보존**: 고역 과다가 없을 때 실제로는 기존 enabled/값을 유지하면서
   상태 문구만 off라고 하던 문제를 unchanged로 수정했다. 스냅샷은 실제 자동 쓰기와
   마지막 값을 추적한다. 시작 이후 값이 달라진 항목은 쓰지 않고, UNDO도 자동 쓰기
   이후 값이 그대로인 항목만 복원한다. 정상적인 자동 쓰기/복원은 기존 host gesture와
   setValueNotifyingHost 경로를 사용한다.

스타일 목표 수치, spectral 2/3/1.5 dB 및 burst 4 dB 한계, range 3~9 dB,
ratio 4:1, attack 1 ms, 스타일별 release, limiter −1 dBFS는 유지했다.
최대 range는 감쇠 상한이며 실제 감쇠 측정과 구별한다. 파라미터 ID·범위·버전,
실시간 DeEsserCore 및 Capture의 오디오 처리 경로는 변경하지 않았다.

## 검증 실행

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build --target ChimeraAutoSetTests ChimeraCommonDynamicsTests ChimeraMultibandDynamicsCoreTests --parallel 2
ctest --test-dir build -C Release -R '^(ChimeraAutoSetTests|ChimeraCommonDynamicsTests|ChimeraMultibandDynamicsCoreTests)$' --output-on-failure
# Linux 환경에 따라 xvfb-run 또는 저장소 Tools/run_linux_ui_tests.py 사용
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --risk-probe
```

환경: Linux x86_64, GCC 13, JUCE 8.0.8, Release/qualification-only 빌드.
기준선 분석의 native probe는 원본 AutoSet.h와 제품 DeEsserCore를 사용했다.
Python DSP 번역이나 FFT 대체품을 사용하지 않았다. `before-risk-probe.txt`가
원래 36개 조건의 측정값이다. `standalone-risk-probe.cpp`는 기준선과 후보
양쪽의 실제 AutoSet.h/DeEsserCore에 대해 빌드할 수 있는 당시 네이티브 harness다.
최종 후보의 검사 로그와 after 측정값을 함께 보관한다.

### 로컬 실행 결과

- 최종 CMake 빌드 및 ChimeraAutoSetTests CTest **1/1 PASS (16.85 s)**.
- Native Auto Set 전체 검사 PASS: 분석 135개 추가 조건, 실제 processBlock 4-pass,
  캡처 무할당, Guitar/Bass × 3 styles, 수동 설정 보존, UNDO/host gesture.
- CommonDynamics 전체 계약 PASS: 컴프레서·Dynamic EQ·익스팬더·리미터와
  파라미터 순서/자동화 알림/프로젝트·A/B 복원 계약.
- MultibandDynamicsCore 전체 계약 PASS: De-esser 대역 선택성·범위·스테레오·listen,
  빠른 파라미터 변경 및 오디오 콜백 무할당.
- 36개 동일 입력 A/B: 기준선 36개 활성화, 후보 0개 활성화 / 최대 감쇠 0 dB.
- 실행 바이너리는 CMake가 생성한 실제 테스트 컴파일 설정과 제품 정적 라이브러리를
  사용한 native 링크였으며, 최종 CMake 실행 파일의 Auto Set CTest도 확인했다.
  저장소 전체 CTest·실제 DAW 시험으로 확대하지 않는다.
  소스 SHA-256, 환경과 판정은 `verification.json`에 기록했다.

추가 1초 간격 탐색(`standalone-risk-probe.cpp`의 `std::fmod(t,.25)`만
`std::fmod(t,1.0)`으로 변경)도 보관했다. Rock 목표 신호를 Metal로 분석한
96 kHz의 두 조건에서는 약 1.65 dB가 기존 1.5 dB 한계를 넘어 최대 약 4 dB를
줄였다. 나머지 34개는 비활성이다. 스타일 목표·임계값의 청감 타당성을 뜻하지 않으며,
이 결과를 없애려고 기존 Metal 한계를 완화하지 않았다.

## 미확인·잔여 위험

- 기존 합성 DI의 실제 전체 체인은 후보에서도 지속 고역 초과 9.88041 dB,
  range 상한 9 dB를 제안한다. 이것이 적절한 음색인지는 미확인이다.
- 실제 기타·5/6현 베이스 raw DI, 외부 페달 클리핑 녹음 및 사람의 A/B 청취 없음.
  저장소에서 찾은 WAV는 IR 또는 기존 합성 시험 결과여서 실연 DI로 사용하지 않았다.
- 빠른 팜뮤트처럼 안정 구간이 부족하면 과도한 고역도 놓칠 수 있다. 의도적인 보수적
  보류이며, 거친 고역이 없다는 판정이나 청감 개선의 입증이 아니다.
- raw DI를 식별하지 않는 자동 분류의 오분류는 남는다. Drop A 기타, 5/6현 베이스는
  Guitar/Bass를 직접 선택해야 Tone EQ 목표까지 확정된다.
- tanh 검사는 클리핑 대용이다. 모든 앰프/캐비넷/외부 디스토션 조합을 검증하지 않았다.
- 캡처의 스테레오 평균은 역상 성분을 상쇄할 수 있다. 이 작업에서 캡처 구조는 바꾸지 않았다.
- 호스트 자동화 검사는 native processor parameter listener의 알림/gesture 계약이다.
  실제 DAW의 write/read/touch automation, 플러그인 포맷별 호스트 인수 시험은 별도다.
- UNDO의 충돌 판정은 마지막 값 비교다. 같은 값을 다시 기록한 자동화나 값이 바뀌었다가
  정확히 원래 값으로 돌아오는 경우의 작성 주체까지 식별하지는 않는다.

청감 승인에는 동일 연주의 레벨 정렬된 Auto 전/후와 수동 설정 대조가 필요하다.
기타 Drop A/Drop B, 베이스 Low B/Low A 및 High C 고음역에서 약한 피킹·강한 피킹·
슬랩·빠른 팜뮤트·서스테인을 clean/내부 gain/외부 distortion으로 나누어 확인해야 한다.
이번 변경은 검토용 후보이며 릴리스·main 병합·청감 승인에 해당하지 않는다.
