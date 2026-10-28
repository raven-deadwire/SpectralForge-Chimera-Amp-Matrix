# PR #189 CAB IR 내보내기 검증 — 2026-10-27 KST

수정 전 PR head는 `d1804e301754584545ac530e2769f6a72e84611a`이다.
이 문서와 함께 커밋된 수정 소스에 대해 Linux 네이티브 CAB 시험 **27/27**을 통과했다.
기존 구현에서는 실제 엔진과 오프라인 사본의 응답 불일치, WAV 재불러오기 시 음량·필터의
중복 적용, 긴 꼬리의 위상 오차를 재현했다. 아래 수정 후 정해진 허용오차 안에서 일치한다.
비트 단위 동일성이나 실제 DAW/외부 로더 호환성을 뜻하지 않는다.

**병합 판단: 보류.** Linux의 기능 결함 수정과 회귀 시험은 통과했지만, 새 코드의
Windows/macOS 네이티브 빌드·CAB 시험 및 필수 통합 CI가 완료되어야 한다.
기존 Windows 시간 초과는 컴파일/DSP 실패의 증거도, 통과의 증거도 아니다.
병합은 실행하지 않았다. 실제 DAW·외부 IR 로더·네이티브 저장 대화상자의 수동 검증은
별도 출시 수용 항목으로 남는다.

## 발견한 결함과 수정

| 문제 | 재현 / 영향 | 수정 |
|---|---|---|
| 오프라인 Mic B 컷 초기화 | `Cab::prepare()`가 미리 적용한 B 컷을 70/9000 Hz로 초기화. 44.1 kHz 모델 A/B에서 실제 CAB 대비 최대 5.95124 dB, 124.578° 차이 | prepare 후 생산 코드의 파라미터 매핑을 재적용 |
| 재불러오기 정규화·이중 처리 | 일반 IR 경로의 정규화 및 컷/게인/딜레이/블렌드 적용. 원본 모델 시나리오의 RMS 레벨 차이 8.96134 dB, CAB off 왕복은 21.9161 dB | BWF description으로 완성된 CAB 응답을 식별, 원래 레벨 유지. Mic A 로드 시 A 단독·CAB on·게인/딜레이/극성 중립·컷 bypass 설정 |
| 조용한 꼬리의 누적 오차 | 앞의 두 수정만으로는 44.1 kHz에서 최대 1.57° 위상 차이가 남음. 샘플별 −120 dB 절단은 전체 꼬리의 주파수 오차를 제한하지 못함 | 버리는 꼬리의 절댓값 합을 `peak * 1e-7` 이하로 제한. 최소 10 ms / 최대 2 s는 유지 |
| 내보내기와 가져오기 길이/무음 규칙 불일치 | 최대 2 s export가 기존 1 s loader/browser 제한에 걸림. 완전한 A/B 상쇄 파일도 거부 | 식별된 CAB export만 최대 2 s와 의도적 무음을 허용. 일반 IR의 기존 제한 유지 |
| 저장 실패 시 교체 위험 | buffered flush 상태 누락, JUCE 교체 함수의 복사 fallback이 기존 파일 보호 보장을 약화 | 메모리 WAV 완성 → 형제 임시 파일 write/flush 상태 확인 → 복사 fallback 없는 같은 디렉터리 rename |
| 확장자 변경 후 다른 파일 덮어쓰기 | 저장 창이 확인한 파일과 `.wav` 적용 후 실제 대상이 다를 수 있음 | 실제 최종 경로가 다른 기존 파일이면 Replace/Cancel 확인 추가 |

각 마이크의 **CUTS OFF** 버튼을 두 CAB 화면에 표시했다. bypass 중 해당 low/high 컷
슬라이더는 비활성화된다. 새 bool 6개는 호스트 파라미터의 마지막에 version 15로 추가했으며,
기존 파라미터 순서를 유지한다. 일반 IR을 다시 가져오면 해당 마이크의 컷을 활성화한다.
Mic B로 완성 CAB를 가져올 때는 B의 게인/딜레이/극성/컷을 중립화하고 기존 블렌드는 유지한다.
WAV 메타데이터가 제거되면 이 자동 왕복 처리도 사라진다.

원본 재현의 선택 측정값은 [original-head-measurements.log](evidence/cab-export-20261027/original-head-measurements.log)에 있다.
원본 시험은 긴 IR 거부 지점까지의 불완전한 실행이다. 개발 중 수정한 모델 CAB의 모노 가정을
제품 결함으로 세지 않았으며, 원본의 전체 실패 건수를 주장하지 않는다.

## 시험 기준과 결과

실제 `ChimeraProcessor`를 준비하고 `processBlock()`으로 생산 파라미터 매핑을 실행한 뒤,
그 프로세서가 소유한 `engine.cabinet(lane)`에 직접 입력했다. 오프라인 사본을 정답으로 삼지
않는다. 임펄스와 좌우가 독립적인 고정 시드 노이즈를 사용했다. CAB 외의 앰프·이펙트·EQ·
다이내믹스·밴드 분할은 이 비교에 포함하지 않는다.

아래 응답/위상은 **실제 CAB와 WAV를 Chimera에 다시 불러온 CAB**의 채널별 최대 차이다.
음량은 임펄스·노이즈의 최대 절대 RMS 레벨 차이이며, null은 모든 오디오 비교 중 가장 큰
상대 잔차다. 시간/게인/위상을 맞춰주는 보정은 하지 않았다.

| 세션 표본률 | 시나리오 | 재불러오기 응답 차이 | 재불러오기 위상 차이 | 최대 RMS 레벨 차이 | 임펄스 피크 이동 | 최악 null |
|---|---:|---:|---:|---:|---:|---:|
| 44.1 kHz | 9/9 | 0.00132132 dB | 0.00785772° | 0.0000189845 dB | 0 samples | −91.4022 dB |
| 48 kHz | 9/9 | 0.00551817 dB | 0.0311329° | 0.0000484044 dB | 0 samples | −89.2159 dB |
| 96 kHz | 9/9 | 0.00855176 dB | 0.234426° | 0.000153017 dB | 0 samples | −78.0346 dB |

통과 기준: null < −75 dB, RMS 레벨 차이 < 0.005 dB, 응답 차이 < 0.1 dB,
위상 차이 < 0.5°, 임펄스 피크 이동 0 samples. 스펙트럼 비교 범위는 20 Hz–0.45 Fs이며,
기준 응답의 스펙트럼 peak보다 60 dB 이상 작은 bin은 제외한다. 영점/완전 무음에서 위상은
정의하지 않는다. 피크 이동만으로 전체 군지연을 증명하는 것은 아니며 위상 비교를 함께 적용했다.
96 kHz 노이즈 null의 가장 작은 통과 여유는 약 3 dB이므로 다른 플랫폼에서도 같은 기준을
실행해야 한다. 플랫폼 차이가 나오면 원인을 분석하고 허용치를 임의로 완화하지 않는다.

| 직접 비교 | 44.1 kHz 최대 응답 / 위상 | 48 kHz 최대 응답 / 위상 | 96 kHz 최대 응답 / 위상 |
|---|---|---|---|
| 실제 CAB → 오프라인 사본 | 0.000942198 dB / 0.0058899° | 0.00536502 dB / 0.0311643° | 0.0085502 dB / 0.234172° |
| 실제 CAB → 저장 WAV | 0.000996035 dB / 0.00607287° | 0.00533087 dB / 0.0312305° | 0.00854813 dB / 0.234151° |

각 표본률에서 다음 9개 구성을 실행했다. 레인 0/1/2와 블록 64/257/1024 samples를 분산해
검증했으며 모든 구성×모든 레인×모든 블록의 완전한 조합 시험은 아니다.

| Case | 구성 |
|---|---|
| 0 | 모델 A/B, 64-sample block |
| 1 | 캡처 IR A-only |
| 2 | 캡처 IR B-only |
| 3 | 모노 사용자 IR A + 캡처 B |
| 4 | 좌우 파형·극성·탭 위치가 다른 스테레오 사용자 IR A/B |
| 5 | CAB 비활성화, unity impulse와 재불러오기 |
| 6 | 1초 스테레오 사용자 IR + 마이크 딜레이/필터 꼬리 |
| 7 | 동일 A/B, 반대 극성, 50% 블렌드의 완전 상쇄 |
| 8 | 모델 A/B, 1024-sample block의 fallback 경로 |

기본 조합은 A/B gain −4/−7 dB, delay 1.37/3.73 ms, 반대 극성, blend 37%,
A 컷 143/6100 Hz, B 컷 337/3200 Hz다. A-only/B-only/상쇄 case에서 관련 값을 변경한다.
사용자 IR 원본은 48 kHz이므로 44.1/96 kHz에서 입력 IR 리샘플링도 포함한다.
WAV export와 reimport는 같은 세션 표본률에서 비교했다. 서로 다른 세션 표본률 사이의
모든 export/import 조합은 검증하지 않았다.

세 표본률 모두 32-bit IEEE float, 해당 session rate, 모노 및 다른 L/R 스테레오 채널 수를
확인했다. 모델 CAB의 미세한 L/R 부동소수점 차이는 그대로 보존하므로 이 경우 2채널도 허용한다.
스테레오 export를 프로젝트에 저장한 다음 원본 WAV를 지워도 복원 응답이 일치했다.
노이즈 처리 두 구간 사이에 export용 IR 렌더링을 끼워 넣어도 실제 엔진의 필터/컨볼루션 이력이 유지됐다.
이는 동시 오디오 콜백의 deadline, 실시간 자동화 중 snapshot 일관성, 커널 전환 중 응답을
인증하는 시험은 아니다. export 대상은 안정된 선형 CAB 상태다.

## 저장·상태·UI 보조 검증

- 기존 파일의 정상 교체와 실패한 디렉터리/부모 경로에서 sentinel 보존: 통과.
- 주입한 short-write / flush 실패에서 기존 바이트 보존, 임시 파일 정리: 통과.
  실제 디스크 용량 고갈·전원 손실·네트워크 파일시스템 장애를 재현한 것은 아니다.
- 최종 `.wav` 경로가 다른 기존 파일일 때 추가 확인 조건: 통과.
  Windows/macOS 네이티브 저장 창의 Replace/Cancel 클릭은 아직 미검증이다.
- 기존 `IRLibraryTests`: 통과. 원본 삭제/embedded IR 복원 및 모델 worker 정리 포함.
- `IRCollectionTests`: 통과. 태그된 2초/무음 파일의 파일·embedded 검증 허용,
  일반 파일의 기존 1초/무음 정책 유지 포함.
- `NativeStateTests::run`: 통과. 기존 파라미터 ID/순서와 새 마지막 6개 순서 검사 포함.
- `GateProcessorTests::parameterContract`만 추출한 집중 검사: 통과. 고정된 기존 3686개
  파라미터와 전체 추가 파라미터 수를 확인했다. Gate 전체 DSP/UI suite 실행을 뜻하지 않는다.
- 실제 JUCE `CabPanel`의 두 화면에서 CUTS OFF attachment/범위 및 스냅샷 검사: 통과.
  [CAB 화면](evidence/cab-export-20261027/cabinet.png),
  [IR Loader 화면](evidence/cab-export-20261027/ir-loader.png).

## 플랫폼과 빌드 증거의 범위

| 항목 | 상태 |
|---|---|
| Ubuntu 24.04.3 / GCC 13.3 / x86_64 / JUCE 8.0.8 / Xvfb | 위 27개 CAB 시험 및 보조 시험 통과 |
| 새 CMake configure | 로컬 JUCE/juceaide를 사용해 성공 |
| 전체 제품 Release 빌드 및 전체 CTest | 이번 수정본으로 실행하지 않음. 작성자의 원본 65/65 주장을 수정본에 승계하지 않음 |
| Windows / macOS 새 코드 | 전용 CI 추가, 네이티브 성공 결과 필요 |
| 실제 DAW / VST3·AU·AAX 호스트 / 외부 IR 로더 / 물리 오디오 장치 | 실행하지 않음 |
| 실시간 자동화·동시 export 부하·오디오 deadline | 이번 시험 범위 밖 |

로컬 검증은 수정 작업 트리의 제품 `.cpp` 4개와 CAB 시험을 다시 컴파일한 **집중 네이티브
하네스**다. 제품/시험은 `-O1`, 변경 없는 JUCE 모듈은 기존 `-O0` 객체를 재사용했다.
embedded asset 라이브러리와 생성된 JUCE/버전/beta 헤더도 재사용했다. 생성된 버전 헤더의
`b46dab...` 값은 이 수정본의 소스 ID가 아니다. 변경 소스 SHA-256과 실제 플래그는
[build-provenance.json](evidence/cab-export-20261027/build-provenance.json)에 기록했다.
마지막 에디터 전용 수정은 별도로 재컴파일/링크했으며 실행 중이던 행렬의 DSP 객체는 바꾸지 않았다.
배포 바이너리나 패키징 검증으로 해석하면 안 된다.

## Windows 시간 초과 분석

원본 head의 [Windows job 121572168240](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40322920228/job/121572168240),
run `40322920228`, attempt 1에서 다음을 확인했다.

- 전체 build 단계: 2026-10-26 17:06:01–18:06:13 UTC, 약 3612초.
- 로그 오류: `Build product and regression checks`의 60분 제한 도달.
- `ChimeraCabExportTests.exe`는 17:19:38 UTC에 이미 링크됨.
- 확인한 전체 job 로그에서 별도의 compiler/linker error를 찾지 못함.
- 이후 CTest는 skipped. 따라서 Windows DSP 통과로 볼 수 없음.

[windows-timeout.json](evidence/cab-export-20261027/windows-timeout.json)에 발췌와 job 식별자를 남겼다.
빌드가 시간 제한에 걸렸다는 사실로 DSP 결함을 추론하지 않았다. 새 `Cabinet WAV round trip`
workflow는 Linux/Windows/macOS에서 CAB target을 먼저 빌드하고 즉시 실행해 전체 제품 빌드의
시간 초과가 CAB 결과까지 가리는 문제를 줄인다. 이 workflow 자체의 원격 성공은 아직 필요하다.

## 재실행과 근거

일반 개발 환경에서 다음 명령을 사용한다. Linux에서는 CTest 앞에 `xvfb-run -a`를 붙인다.

```sh
cmake -S . -B build-cab-export -DCHIMERA_BETA_QUALIFICATION_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-cab-export --config Release --target ChimeraCabExportTests --parallel 2
ctest --test-dir build-cab-export -C Release -R '^ChimeraCabExportTests$' --output-on-failure --no-tests=error
```

이는 CI 재실행 명령이며 이번 로컬 실행을 fresh Release build로 표현한 것이 아니다.
원시 결과: [44.1 kHz](evidence/cab-export-20261027/linux-44100.log),
[48 kHz](evidence/cab-export-20261027/linux-48000.log),
[96 kHz](evidence/cab-export-20261027/linux-96000.log),
[전체 측정 JSON](evidence/cab-export-20261027/measurements.json).
보조 결과와 재현용 UI 하네스는 같은 evidence 디렉터리에 있다.
