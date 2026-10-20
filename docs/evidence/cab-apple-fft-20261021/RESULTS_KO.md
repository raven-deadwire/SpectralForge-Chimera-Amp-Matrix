# Apple FFT 항등 정규화 제거 후보 검증 — 2026-10-21 KST

## 판정

**미채택: native Apple FFT의 수치 동등성과 CAB 비용 개선이 아직 입증되지 않았다.**
실험용 패치와 실행 경로만 PR #109에 추가한다. 제품 소스 통합·병합·배포는 하지 않는다.
`integration_eligible=false / release_approved=false / aax_approved=false`.

## 완료한 소스 검증

- 기준: #109 `cb9d2f3500b210d9556cbe4045dec5eac42ab900`.
- JUCE 8.0.8 `d6181bde38d858c283c3b7bf699ce6340c050b5d` 실제 소스를 확인했다.
- complex forward의 `0.5 * 2.0 = 1.0` vDSP_vsmul pass를 확인했다.
- 빌드 사본에 명시적 CAB 연구 API만 추가한다. 기존 AppleFFT 메서드와 다른 backend의 기존
  API, inverse 1/N, real forward 0.5/real inverse 1/N 정규화 코드는 그대로 남는다.
- CAB realtime stereo forward 1곳만 opt-in; IR preparation, mono, 50ms fade, 모델 IR 길이,
  개인 IR 경로, FFT/partition geometry, 누산 순서와 latency는 유지된다.
- source drift 거부 및 기존 코드 전체 복원 검사를 통과했다. 상세 SHA256은
  `static-verification.json`, 정적 계약 결과는 `contract-tests.json` 참조.

## 실행 상태

로컬 환경은 Linux x86_64/GCC 13.3이며 Apple vDSP가 없다.
Linux 실행은 비Apple fallback의 negative control이며 Apple 후보 성능 입증으로 사용하지 않는다.
macos-15 전용 CI는 실험을 실행하도록 준비했다. 실제 실행 결과와 로그를 확보한 후 이 절을 갱신한다.
작성 시점에 FFT 처리 시간 감소율, CAB CPU 감소율, deadline miss 개선을 주장하지 않는다.

Linux 공식 CMake configure는 PkgConfig 부재로 중단됐다. 별도 수동 GCC 경로를 제공하며 이를
공식 CMake/CTest 성공으로 보고하지 않는다. 최초 FFT microprobe 빌드에서 timer namespace 오타를
확인하여 수정했고, 별도 신규 디렉터리에서 다시 빌드했다. timing population 재시도는 아니다.

## 재현과 채택 기준

`Tools/CabAppleFFT/README_KO.md`에 격리 설계·명령·정규화 보존·한계를 정리했다.
기존 동일 상태 CAB probe, 6 ABBA/BAAB 및 6 A/A quartet, >5%와 A/A envelope 초과,
6회 동일 개선 방향, wall miss/CPU 초과 비증가 기준을 유지한다.
96kHz·32 samples 예산은 333.333333µs다. FFT microbenchmark는 CAB callback과 별도 측정한다.
수치 mismatch는 현재 후보 기각, 이득이 noise 범위를 넘지 못하면 미채택이다.
#103 전체 DSP 계측 및 #105 CPU/실행 지연 분리 결과를 대체하거나 이전 실패를 해제하지 않는다.
