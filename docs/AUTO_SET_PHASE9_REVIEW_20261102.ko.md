# Auto Set Phase 9 독립 검토 — 2026-11-02

## 판정과 대상

PR #237은 2026-11-02 17:31:10 KST에 이미 병합됐다.
검토한 PR head는 `635502ca40b5faa3ee3e04ec441024dfecc146a6`,
병합 커밋과 후속 수정의 기준은 `58e193434ffe48dac8c96005e3479e84a1cd1ed4`다.

기존 합성 음향 회귀 수치와 CI 성공은 확인됐다. 그러나 사용자가 마이크
컨트롤을 편집한 경우 안전 검사를 우회하는 부분 적용 결함을 추가 재현했다.
따라서 기존 CI 성공만으로 추가 수정이 불필요하다고 판단할 수 없다.
이번 후속 수정의 원격 CI와 실제 DI·청취 검증은 별도 판단 대상이다.

## 재현된 결함과 수정

- 조건: Classic, Bass 4x10 / Vector 10 / Anchor 20, 유닛 4,
  POSITION 0.1, DISTANCE 20 cm, mic gain -2 dB, 48 kHz, Strength 50%.
- 입력: 기존 `cabRegression::target(true,48000,8)`의 결정적 합성 신호.
- 청취 중 POSITION을 0.1 → 0.2 → 0.1로 변경하고 제스처를 종료한다.
- 기존 컨트롤러는 최종 값/모델 키만 같다고 판단하고 분석을 진행한다.
  개별 파라미터 편집 보호가 POSITION 쓰기를 거부하지만 DISTANCE는
  24.3 cm, gain은 -1.4 dB로 변경한다. 안전 검사를 받은 위치·거리 쌍과
  실제 적용된 쌍이 달라진다.
- 마이크 A/B 모두 연속 모델의 국소 극값 증가 **3.67023 dB**,
  실제 생성 IR의 독립 DTFT 기준 **3.67025 dB**를 재현했다.
  3 dB 제한을 넘는 값이다. 기존 6x10 결함과 별개의 4x10 사례다.

수정은 위치·거리·레벨 세 컨트롤의 쓰기 소유권을 함께 검사한다.
하나라도 편집되었거나 제스처가 진행 중이면 자동 배치 전체를 보류한다.
적용 중 호스트의 begin/value/end 콜백이 개입하면 Auto Set이 소유한
부분 쓰기만 복구하고 호스트의 값과 편집 이력을 유지한다.
IR 생성기, 음향 검색식, 3 dB 기준, FFT 배치, 파라미터 ID/범위는 유지한다.

## 기존 PR 검증 대조

검증 문서 `AUTO_SET_PHASE9_VALIDATION_20261031.md`, 소스 및
GitHub Actions run `41380883333`의 원시 로그/CTest 결과를 대조했다.
모든 플랫폼의 근거는 PR head `635502c`에 연결되어 있다.

| 항목 | 확인 결과 |
|---|---|
| Drop A 27.5 Hz 및 저차 배음 | 90 IR 쌍, 270 레벨 사례 통과. Linux 최대 0.010091 dB, macOS 0.0100888 dB 예측 오차 |
| IR와 모델의 변화량 | 대역 최대 0.232348 dB, 저차 배음 최대 0.750016 dB |
| 기존 국소 극값 회귀 | 18개 유효 이동, 생성 IR의 최대 증가 2.9698 dB |
| A/B·Strength | 베이스 24개 이동, 25/50/75/100%, 실제 적용 위치의 레벨 보정 및 반대 마이크 보존 통과 |
| Lock·UNDO | 베이스 잠금 6개 및 전체 파라미터 복원 통과 |
| 적용 제외 | Dual, captured IR, A/B 혼합 유지 시험 통과. Matrix도 같은 mode != 0 분기로 제외 |
| 기존 상태 호환 | CAB layout/state, 프로젝트·A/B·개인 IR 복원 및 과거 beta.2 상태 복원 CI 통과 |
| Windows | Open Beta CTest 105/105 통과 |
| Linux | Open Beta CTest 94/94 통과 |
| macOS | Open Beta CTest 93/93 통과 |

조회한 PR head의 워크플로는 성공 14개, preset maintenance 건너뜀 1개였다.
Open Beta의 3개 OS 빌드, 통합 검사, beta lifecycle 및 세 OS의 historical
restore 작업이 완료됐다. 건너뜀은 실행 성공으로 집계하지 않았다.

## FFT 리뷰 판정

리뷰 `discussion_r4431101813`, `discussion_r4431101814`의
“Nyquist를 float index 1에서 읽어야 한다”는 지적은 둘 다 잘못됐다.

JUCE 8.0.8 (`d6181bde38d858c283c3b7bf699ce6340c050b5d`)의
`juce_FFT.h` 75–100행은 2*N float 버퍼와 interleaved complex 출력을
정의한다. complex bin N/2는 float N 및 N+1에 해당한다.
`AppleFFT::mirrorResult`는 내부 vDSP의 DC imaginary에 저장된 Nyquist를
complex bin N/2로 옮기고 DC imaginary를 0으로 만든다.
따라서 `frame[1]`/`bins[1]`로 치환하면 오히려 Nyquist 에너지를 잃는다.
N, N+1은 할당한 2*N 범위 안이므로 out-of-bounds라는 리뷰 요약도 틀렸다.
44.1/48/96 kHz alternating-sample 회귀가 Linux/macOS CI에서 통과한
것도 실제 백엔드가 이 계약을 지킨다는 증거다. 생산 FFT 코드는 수정하지 않았다.

## 후속 회귀

`Tests/AutoSetCabEditTests.h`를 기본 Auto Set suite와 `--cab-regression`에
연결했다. `--cab-edits`로 이 검사만 실행할 수도 있다.
마이크 A/B, 공개 Strength 4단계, 위치·거리·레벨의 반환 편집,
진행 중 제스처, begin/value/end 동기 호스트 콜백, 부분 적용 복구 및
UNDO를 확인한다. 정상 입력에서도 유효 이동이 남아야 통과한다.

원래 컨트롤러를 대입하면 새 회귀는 `Edited mic left a partial automatic
placement`로 실패한다(exit 1). 수정본의 집중 회귀는 **56/56 사례**,
정상 유효 이동 **8개**, exit 0으로 통과했다. 이후 콜백을 위치·거리·레벨
각 쓰기 지점에서 발생시키도록 확장한 최종 **92/92 사례**도 exit 0으로
통과했다(28.70 s). Linux GCC 13.3 / JUCE 8.0.8 Release의 headless
processor 시험이며 네이티브 창/DAW 검증은 아니다.

## 최종 로컬 검증

- 전체 `ChimeraAutoSetTests`: exit 0, **224.15 s**. 기존 CAB 90/270/18
  음향 검사, 24개 베이스 이동/6개 잠금, Auto Set 다른 단계와 전체 상태
  UNDO까지 통과했다. 이 실행에는 초기 56개 편집 사례가 포함됐다.
- 이후 테스트만 확장한 최종 92개 편집 사례: 같은 컴파일 옵션으로
  `AutoSetTests.cpp`를 포함한 집중 runner에서 exit 0, **28.70 s**.
  생산 코드는 전체 suite 실행 때와 같다. 기본 suite 및 `--cab-regression`의
  호출도 포함되어 있으며, 최종 PR head의 원격 CI에서는 92개를 실행한다.
- `ChimeraCabLayoutModelTests`: 1/1, **3.90 s**, exit 0. 생성기 변경 없음.
- `git diff --check`: 통과.
- 생산 `AutoSetController.h` SHA-256:
  `c3bcaf6659f28a7a79be69a95426d69541d42058f8531a9581e0e0132be2d136`.

재실행:

```sh
cmake --build build --target ChimeraAutoSetTests ChimeraCabLayoutModelTests --parallel 2
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --cab-edits
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --cab-regression
ctest --test-dir build -R '^(ChimeraAutoSetTests|ChimeraCabLayoutModelTests)$' --output-on-failure
```

최종 후속 PR의 CI 완료 전에는 원래 PR의 CI를 새 head의 성공으로
승계하지 않는다. 실제 DI/청취 및 공개 릴리스 승인은 이번 판단에 포함하지 않는다.

## 남은 검증

실제 Drop A/B0 베이스 DI, 어택·고스트 노트, 청취 비교는 수행하지 않았다.
합성 RMS 정확도는 기음/각 배음 보존이나 지각적 LUFS 일치를 보장하지 않는다.
선택된 드라이버·마이크, 닫힌 후면/트위터 OFF 시험을 모든 조합의 승인으로
확대하지 않는다. 1/96 octave 격자 사이의 모든 노치까지 증명하지 않으며,
gain -24..+12 dB 한계에서는 완전한 보정이 불가능할 수 있다.

## 근거

- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/237
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41380883333
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41380883501
- https://github.com/juce-framework/JUCE/blob/8.0.8/modules/juce_dsp/frequency/juce_FFT.h
- https://github.com/juce-framework/JUCE/blob/8.0.8/modules/juce_dsp/frequency/juce_FFT.cpp
