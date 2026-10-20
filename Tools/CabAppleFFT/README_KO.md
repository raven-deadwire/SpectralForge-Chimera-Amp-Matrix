# Apple FFT complex-forward 항등 정규화 — PR #109 독립 실험

## 상태와 범위

기준 제품/연구 소스: PR #109 `cb9d2f3500b210d9556cbe4045dec5eac42ab900`.
JUCE 8.0.8: `d6181bde38d858c283c3b7bf699ce6340c050b5d`.
`Source/`, 기존 `Tests/`, 제품 CMake, 버전, 배포 파일은 변경하지 않는다.
`integration_eligible=false`, `release_approved=false`, `aax_approved=false`.
실제 native 결과가 없으면 **미채택 / macOS 측정 필요**, 개선율은 미확인이다.

## 확인한 구현과 격리 방식

JUCE `modules/juce_dsp/frequency/juce_FFT.cpp`의 AppleFFT는
`vDSP_fft_zop` 후 `forwardNormalisation(0.5f) * 2.0f`를 `vDSP_vsmul`로 적용한다.
complex inverse는 1/N, real forward는 0.5, real inverse는 1/N을 사용한다.
배율 1의 생략이 특수 부동소수점 값에서도 비트 동일한지는 수학만으로 판정하지 않는다.

`generate.py`는 원본 FFT cpp/h와 CAB 헤더의 SHA256을 고정하고 빌드 디렉터리에만
JUCE/Source 사본을 만든다. 변경은 다음 세 부분이다.

1. 기존 FFT API/AppleFFT 메서드를 그대로 두고 `performChimeraCabForwardNoUnity`를 추가한다.
   Apple 구현만 동일한 split layout/stride/order의 `vDSP_fft_zop`을 실행하고 unity pass를 생략한다.
   다른 backend는 기존 `perform(...,false)`로 위임한다. backend 식별 API로 실제 Apple 경로를 확인한다.
2. candidate CAB의 **실시간 stereo complex forward 호출 단 한 곳**을 새 API로 바꾼다.
   Prepared IR 스펙트럼 생성도 기존 함수 그대로다. mono, inverse, 개인 IR의 JUCE convolution,
   다른 FFT 사용자, 50ms fade, 8,161샘플 모델 IR, partition/누산 순서/지연은 바뀌지 않는다.
3. baseline은 같은 연구용 JUCE 라이브러리의 변경되지 않은 기존 API를 사용한다.
   두 바이너리의 JUCE dependency와 컴파일 옵션을 공유한다. 추가 API를 지우면 원본 전체가
   byte-exact로 복원되는지 생성기에서 확인한다. 이는 기존 메서드의 동작 검증을 대신하지 않는다.

새 FFT 객체·scratch·allocation·mutex·worker·callback guard를 추가하지 않는다.
세 patch 파일은 검토용 diff다. 적용은 source drift를 거부하는 생성기로만 한다.
전역 `vDSP_vsmul` 제거, 다른 후보와 결합, 제품 backend 교체는 이 실험 범위 밖이다.

## 검증 및 미리 정한 판정

- FFTCheck: N=4..1024, caller FP 모드/ScopedNoDenormals 각각 10종 입력 = 180 cases.
  정상 신호, signed zero, subnormal, 작은 정상값, 큰 유한값, Inf, qNaN, sNaN을 전체 비트 비교한다.
  candidate 호출 전후의 기존 inverse 및 real forward/inverse도 비교한다. 특수값 실패도 숨기지 않는다.
- PR109 Parity, 144-route realtime, 기존 integration 4 entry, 개인 IR 파일 삭제 후 embedded 복원 검사를
  baseline/candidate 각각 실행한다. gate 실패 로그를 유지하며 기준을 완화하지 않는다.
- 기존 `CabZeroWork/Probe.cpp`를 수정 없이 재사용한다. 96kHz, 32/64, mono/stereo,
  v1/v2/v3, 3 CAB/6 mic의 12 routes, route당 2,000 callbacks. IR 준비/publication을 측정 밖에 고정한다.
  전체 출력 SHA256, callback별 출력/slot/fade state, IR identity, new/delete 관측을 대조한다.
- CAB timing은 6 ABBA/BAAB + 교차 동일 바이너리 A/A = 48 trials / 1,152,000 callbacks.
  32샘플 **333.333333µs**, 64샘플 666.666667µs. wall miss, CPU 동반 초과, wall-only 초과를 보존한다.
  새 중첩 stage 계측이나 scheduler 원인 추정은 하지 않는다. #103/#105 연구를 대체하지 않는다.
- FFT 단독 timing은 CAB와 별도 순차 population이다. N=128/256/512/1024, 20 warmups,
  200 batches × 128 transforms. 동일한 6 quartet A/B·A/A를 사용한다.
- 기존 analyzer의 기본값은 유지하고 candidate 이름만 인자로 전달한다.
  paired CPU p50 개선 중앙값 >5%, A/A 최대 절대 변동 초과, 6 quartet 모두 개선,
  route/phase별 wall miss·CPU over-budget 비증가, 출력/state/gate 통과를 그대로 적용한다.
  fade_active는 publication과 겹치므로 독립 callback 총수로 합산하지 않는다.
- 개별 route의 유리한 결과만으로 채택하지 않는다. FFT 4규격과 stereo 30 route-phase 전부의
  screening 및 mono를 포함한 deadline 비증가가 필요하다. 통과해도 별도 제품 검토 대상으로만
  기록한다. numerical mismatch는 현재 후보 기각, 이득이 noise를 넘지 못하면 미채택이다.

new/delete 관측은 C++ allocation hook 범위이며 모든 시스템 allocator를 감시한다는 뜻은 아니다.
이 실험은 CAB-only이며 전체 processor, 실제 DAW/interface dropout, 청음, 배포 승인을 대신하지 않는다.

## 재현

macOS에서 repository 밖의 빌드 디렉터리를 지정한다. JUCE 원본은 바꾸지 않는다.

```sh
python Tools/CabAppleFFT/test_contract.py --juce ../JUCE-8.0.8
cmake -S Tools/CabAppleFFT -B ../apple-build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DJUCE_UPSTREAM=/absolute/path/JUCE-8.0.8
cmake --build ../apple-build -j 3
python Tools/CabAppleFFT/run.py --build ../apple-build --output ../apple-evidence
```

`cab-apple-fft-research.yml`은 macos-15에서 동일 절차를 실행하고 실패한 로그도 artifact로 보존한다.
workflow 존재/성공과 효과 입증은 다르다. 최종 판정은 `decision.json` 및 원시 증거를 읽는다.
외부 CI를 사용할 수 없으면 미실행 상태를 유지한다.

Linux는 AppleFFT가 없으므로 성능 mode를 거부한다. 빌드/음향/상태 대조만 수행한다.

```sh
python Tools/CabAppleFFT/build_linux.py --juce ../JUCE-8.0.8 --build ../apple-linux
python Tools/CabAppleFFT/run.py --build ../apple-linux --output ../apple-linux-evidence --correctness-only
```

이는 수동 GCC 빌드이며 공식 CMake/CTest 성공으로 보고하지 않는다. 출력 디렉터리는 신규만 허용하며
실패 population 덮어쓰기, timing retry, outlier 삭제를 하지 않는다.
