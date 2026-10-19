# 검증 결과 — 2026-10-19

기준 main: `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`.
Linux Release / JUCE 8.0.8, beta qualification 빌드. 실제 시험한 변경 코드의
SHA-256은 `source-manifest.json`에 있다. 공개 배포·설치본·청감 수용 결과가 아니다.

## 자동검증 결과

| 검사 | 결과 | 증거 |
|---|---|---|
| 48개 변경 전/후 오디오·상태·중복 압축 | PASS | `comparison.csv`, `comparison.log`, `signal-path-audit.csv` |
| 30개 자동화 경로: 5 presets × 3 rates × mono/stereo | PASS | `automation.csv` |
| 전체 IntegratedProcessor 회귀 | PASS, exit 0 | `integrated-final.log` |
| GraphicalEQ DSP / UI / Editing 및 GateProcessor | 4종 PASS | `initial-ctest.log`, `initial-ctest-results.xml` |
| CommonDynamics | PASS | `common-dynamics-final.log`, `common-dynamics-results.xml` |
| 8개 24-bit 합성 WAV 페어의 오프라인 RMS 정렬 | PASS | `wav-alignment.csv` |
| 기존 snapshot·DSP·파라미터 범위 보존 | PASS | `scope.json`, `source-manifest.json` |

- 변경 전 대비 RMS: **−0.8693~−0.0310 dB**. OUTPUT 노브는 기존 0 dB 유지.
- RMS 정렬 후 crest 변화: **−0.2552~+0.5166 dB**.
- 정렬 후 각 pluck 첫 20 ms peak 변화: **−0.2552~+0.5291 dB**.
- 48개 nominal 및 입력 +6 dB에서 최대 **−1 dBFS sample peak**.
- 리미터 OFF에서도 최대 peak는 nominal **−0.7594 dBFS**, +6 dB 입력 **−0.9100 dBFS**.
  기존 peak < 0.95, RMS > −30 dBFS 기준을 그대로 통과했다. 최저 RMS **−27.7909 dBFS**.
- 저장/복원 오디오 최대 차이 **5.96e−8**, 자동화 복원 최대 차이 **2.98e−7**.
  48 kHz 비교에서 기존 지연 42 samples 유지. 자동화 중 rate별 지연도 유지.
- 각 자동화 경로에서 begin/end gesture 각각 720회. 6개 파라미터의 출력 유한성,
  피크 및 복원 후 동일 자동화를 검사했다. 실제 DAW automation 모드 시험은 아니다.
- 정렬 WAV 8쌍 최대 RMS 오차 **4.40e−7 dB**. 페어 양쪽에 같은 headroom pad 적용.
- 베이스 저음 A/B의 EQ 계산 응답: 최대 dynamic cut까지 포함해 최저 **−0.0333 dB**.
  전체 신호의 청감 저역 보존을 이 수치만으로 주장하지 않는다.
- 공통 COMP는 7개만 활성, 기존 PRE/POST/Matrix LOW 압축과 겹치는 프리셋 **0개**.
  기존 PRE 압축 28개, POST 압축 11개, POST EQ 20개, Matrix LOW 압축 10개를 조사했다
  (각 분류는 서로 겹칠 수 있음).

전체 IntegratedProcessor 실행에는 기존 Original dispatch, 30 channel banks / 390 knobs,
구형 상태 오디오, A/B, 시그니처, gain response 및 Niflheimr 5개 회귀도 포함된다.
`levels.csv`, `hot-input.csv`, `limiter-bypassed.csv`, `dynamics.csv`, `eq-detectors.csv`,
`low-fundamental.csv`는 최종 실행 로그에서 추출했다. 판정 기준을 낮추지 않았다.

## 최초 실패와 처리

최초 integrated 검사의 새 dirty-state fixture가 bool 파라미터에 normalized `0.81`을
직접 넣었다. `finalEQ_b1_enabled`에서 저장값 true와 JUCE 내부 getValue 0.81이
달라져 복원 비교가 실패했다. `restore-diagnostic.log`에 expected 1 / actual 0.81을
남겼다. fixture를 bool 0/1 및 각 파라미터의 유효 스텝으로 왕복 변환하고, 기존 값과
반대쪽 값을 사용하도록 고쳤다. 제품 DSP/복원 코드나 허용 오차는 바꾸지 않았다.
수정한 test TU를 같은 Ninja compile/link 명령으로 별도 executable에 빌드하고
**전체 integrated suite를 다시 실행해 exit 0**을 확인했다.

최초 CTest의 CommonDynamics Not Run은 해당 binary 빌드가 끝나기 전에 검사에
도달한 실행 순서 문제였다. 6개 target의 CMake 빌드 완료(exit 0) 후 별도 CTest로
통과했다. 최초 실패 로그도 보존하며 처음부터 전체 CTest가 성공했다고 표시하지 않는다.

이 executor는 AF_UNIX 연결을 거부한다. Xvfb/Openbox를 인증된 TCP X11 연결로
실행해 실제 native UI 검사를 유지했다. `run_local_tcp_tests.py`는 이 환경만을 위한
재현 보조 도구다. 일반 CI의 desktop helper나 GUI assertion을 완화하지 않았다.

## 재현 및 남은 수용 조건

일반 Linux 환경에서 저장소의 CMake/JUCE 준비 절차 후:

```sh
cmake --build build --target ChimeraIntegratedProcessorTests ChimeraGraphicalEQTests ChimeraGraphicalEQEditingTests ChimeraGraphicalEQUITests ChimeraCommonDynamicsTests ChimeraGateProcessorTests -j2
python3 Tools/run_linux_ui_tests.py -- ctest --test-dir build -C Release -R '^(ChimeraIntegratedProcessorTests|ChimeraPresetFinishingComparison|ChimeraGraphicalEQTests|ChimeraGraphicalEQEditingTests|ChimeraGraphicalEQUITests|ChimeraCommonDynamicsTests|ChimeraGateProcessorTests)$' --output-on-failure --no-tests=error
```

비교 CSV와 합성 WAV는 `build/signature-snapshots/preset-finishing-comparison/`에 생성되며
기존 Product CI의 signature-snapshots artifact 수집 범위에 들어간다. WAV는 git에
넣지 않는다. 비교 baseline은 변경 범위 검증 후 새 finishing만 기존 기본값으로
되돌린 **같은 DSP의 이전 레시피**다. 이전 설치 바이너리를 실행한 비교는 아니다.

실제 기타/베이스 DI의 음량 정렬 청음, 팜뮤트·슬램·둠 sustain·slap의 detector 복귀,
Windows/macOS 설치본, Studio One 프로젝트 재열기 및 Read/Touch/Latch/Write는
**미실행**이다. GitHub 최종 head의 플랫폼 CI도 별도 판정한다. 이 때문에 개발 PR은
Draft로 유지하며 P0 배포 작업의 완료나 공개 배포를 주장하지 않는다.
