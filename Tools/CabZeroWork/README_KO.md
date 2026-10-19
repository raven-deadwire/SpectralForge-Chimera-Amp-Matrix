# CAB 영 기여 연산 후보 — 2026-10-20 KST

PR #105 `50ed1fd50ffb735d8f6581197116ac4184187d91` 위의 독립 연구다.
`Source/`, 기존 Tests, 버전, AAX, release policy를 수정하지 않는다.
생성기는 baseline 헤더 SHA256을 고정하며 후보를 build overlay에만 만든다.
현행 main `ec6fd639…`과 AAX `8ff4dbf5…`의 CAB/IRLibrary 제품 구현은 서로
동일함을 비교했다. #105는 여기에 compile-time 진단 hook을 더한 기준이다.

## 소스 검토 및 선정

| 경로 | 확인 | 이번 판단 |
|---|---|---|
| 모델 생성 | `ceil(rate*.085)` 전체 IR 보존. 실제 96k 결과는 8,161샘플 | 8,160으로 반올림하거나 tail을 자르지 않는다 |
| 준비 | block 32: FFT128, segment96, 86 partitions, stereo128/mono65 bins | FFT·partition·입력 ring 크기 그대로 |
| FFT | mono real FFT, stereo L+iR complex FFT; 두 채널 독립 출력 | Linux fallback과 Apple vDSP 성능을 구분. 기존 forward reuse 반복 제외 |
| MAC | delayed 1..85 순차 누산 후 partition0. partial call에는 delayed sum 재사용 | reset 직후 아직 입력이 없는 delayed partition을 검증 대상으로 선정 |
| convolution | 개인 IR/stereo IR은 JUCE fallback; 모델만 prepared path | 개인 IR 경로와 정규화·resample 유지 |
| fade | 6 steady + 최대3 fading, rig당 A/B 직렬 50ms | 동일 callback에서 동일 pending/active/fading 상태 비교 |
| 캐시/복원 | worker의 immutable spectra 공유, mic별 private history, prepare에서 cache 삭제 | 신규 필드는 저장 상태가 아니며 reset에서 복원. ownership 변경 없음 |

### 후보 A: empty_partition

`ceil(length/segmentSize)`까지만 delayed MAC을 수행한다. 기존 IR 스펙트럼,
ring 크기와 위치는 그대로 둔다. **현행 96k 모델은 8,161샘플이므로 제거할
빈 partition이 없다.** 8,160샘플이라는 초기 가설은 실제 부동소수점 길이
확인으로 기각했다. 합성 exact-multiple 길이에서 안전성을 검사하지만
이를 제품 성능 향상으로 주장하지 않는다. 타이밍에서는 효과가 없을 것으로 예상되는 대조 후보로 남긴다.

### 후보 B: zero_history

reset 후 완전히 소비된 내부 입력 block 수 `completedBlocks`를 유한 상한까지 센다.
delayed partition p는 p × inputSegmentStep 이전 입력을 참조하므로,
`p <= completedBlocks/inputSegmentStep`인 구간만 계산한다. partial host block은
기존 inputPosition/pastSum 계약을 유지한다. 입력 신호가 조용하다는 이유로
건너뛰지 않는다. reset으로 0임을 아는 미기록 history만 대상으로 한다.

32샘플에서 최초 callback은 delayed MAC 85회를 생략하고, 255개 내부 block
(85ms) 이후 원래 85회를 수행한다. FFT 호출, leading MAC, overlap-add, IR tail은
그대로다. 새 IR의 초기 50ms fade와 reset 후에 효과를 기대하지만, 장기 steady
구간의 비용 감소를 보장하는 안은 아니다. 콜백에 추가되는 것은 정수 계산과
상한이 있는 counter뿐이며 allocation, destruction, mutex, I/O, 새로운 worker는 없다.

다음 이전 후보는 재실행/조합하지 않는다: #76 수동 NEON/S8T, #79 ring span·tail copy
fusion·input-head clear·tile32, #81 forward FFT reuse, #96 bounded prefetch.
비균일 partition/비동기 convolution은 tail burst·지연·동등 출력 위험으로 보류한다.

### 미실행 후보 C: Apple complex-forward unity scaling

고정 JUCE 8.0.8 `juce_FFT.cpp`의 `AppleFFT::perform`은 complex forward 뒤에
`forwardNormalisation * 2.0f = 1.0f`를 `vDSP_vsmul`로 전체 buffer에 적용한다.
이 unity scaling pass만 forward에서 생략하는 별도 backend 후보를 검토할 수 있다.
기존 CAB forward-FFT 공유와 다른 변경이다. 다만 실제 vDSP의 비용·특수값/부호 zero
동등성·다른 FFT 이용자 영향은 macOS에서 검증해야 한다. 이 환경의 Linux FFT fallback은
해당 pass를 사용하지 않는다. 코드 생성/실행·성능 검증은 하지 않았으며 적용 후보로
승격하지 않는다. inverse의 1/N scaling이나 real FFT의 0.5 scaling은 제거 대상이 아니다.

## 동일 상태 A/B

`Probe.cpp`: 96kHz, 32/64 samples, mono/stereo, v1/v2/v3, 3 CAB/6 mic의 12 routes.
각 route는 cold400 → steady400 → publication800 → steady400 = 2,000 callbacks.
두 세대 IR을 측정 전에 준비하고 publication은 800번 callback 직전에만 수행한다.
worker의 비동기 도착 순서를 제거한 계산 비용 실험이며 원래 worker acceptance를
대체하지 않는다. 별도로 원래 integration 4 entry / 66 timing routes를 실행한다.

모든 callback의 출력 전체 float32 SHA256, IR 식별자·길이·내용 hash, slot 상태,
fade 전후 수, callback별 출력 hash를 비교한다. 동일 raw output 하나를 gzip 보관하고
동일한 반복 output은 SHA256과 byte identity로 연결한다. 다른 output은 삭제하지 않는다.
재현 입력은 서로 다른 좌우·lane 신호다. clocks 밖에서 상태/hash/출력을 수집한다.

후보별 6 ABBA/BAAB quartet + 교차한 동일 baseline 바이너리 A/A 6 quartet.
콜백을 독립 통계 표본으로 간주하지 않는다. 소스에서 미리 정한 local screening:
paired CPU p50 개선 중앙값 >5%, A/A 최대 절대 변동 초과, 6회 모두 같은 개선 방향,
route/phase별 wall miss 및 CPU over-budget 비증가, 출력/상태 일치, 기존 native 검사 PASS.
이 조건도 macOS 제품 적용이나 출시 승인은 아니다. 모든 outlier와 실패를 보존한다.

32샘플 예산은 **333.333333µs**, 64샘플은 666.666667µs다. wall/CPU endpoints는
별도다. CPU가 wall을 소폭 넘는 표본도 삭제/보정하지 않는다. CPU에는 clock/커널
회계 등이 포함될 수 있고, wall-only miss를 특정 OS 스케줄링 원인으로 단정하지 않는다.
stage clocks는 꺼져 있다. 중첩 stage 합산이나 독립 p99 차감은 하지 않는다.

## 실행

Linux 전용 수동 빌드(JUCE 공식 CMake/CTest 대체 아님):

```sh
python Tools/CabZeroWork/build_linux.py --juce /path/to/JUCE-8.0.8 --build /tmp/cab-zero-build
python Tools/CabZeroWork/extra_checks.py --juce /path/to/JUCE-8.0.8 --build /tmp/cab-zero-build --output /tmp/cab-zero-extra
# 모든 빌드와 sanitizer 실행이 끝난 후 순차 실행. 다른 benchmark와 겹치지 않는다.
python Tools/CabZeroWork/run.py --build /tmp/cab-zero-build --output /tmp/cab-zero-evidence
python Tools/CabZeroWork/run.py --analyze-only --output /tmp/cab-zero-evidence
```

경계/부분 block parity 196 cases, 기존 144 realtime routes, frozen bits/fade,
개인 IR 원본 파일 제거 후 embedded WAV/음향 복원, modeled worker cancellation·reprepare도 검사한다.
`extra_checks.py`의 ASan/UBSan은 candidate header와 test translation unit 범위다.
공유한 JUCE Release objects 전체를 sanitizer로 계측했다고 주장하지 않는다.

`CMakeLists.txt`는 후속 macOS/Linux native build용 준비 경로다. 이 작업에서 실제
성공한 빌드와 구별하며, Windows의 probe는 CPU 시간 대신 cycles API이므로 이 Linux/macOS
CPU-us 분석에 그대로 사용할 수 없다. 별도 Windows 프로토콜이 필요하다.

## 적용 경계

실행 결과는 `docs/evidence/cab-zero-work-20261020/RESULTS_KO.md`에서 source/hash와 연결한다.
Linux CAB-only 측정은 Apple vDSP/macOS, 전체 PluginProcessor, 실제 DAW/interface dropout,
Pro Tools/AAX, 청음, 통합·공개 승인을 대체하지 않는다. `integration_eligible=false`,
`release_approved=false`, `aax_approved=false`를 유지한다.
