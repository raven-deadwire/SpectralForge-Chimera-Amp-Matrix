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
macos-15 전용 CI 실행 [39470382813](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39470382813)은
2026-10-21 02:58 KST 확인 시 러너 대기(queued) 상태다. job 118792563442는 실행을 시작하지 않았다.
따라서 Apple FFT의 비트 동일성·FFT 처리 시간·CAB CPU·deadline 효과는 **미측정**이다.
워크플로 등록이나 Linux 통과를 native 성능 증거로 승계하지 않는다.

### 실제 완료한 Linux negative control

- baseline/candidate 각각 8개, 총 **16/16 실행 entry PASS**.
- FFT 180 cases / 327,040 float words에서 mismatch 0 (각 바이너리에서 동일 suite 반복).
  `apple_backend=0`; 새 API는 기존 Linux FFT fallback으로 위임된다.
- 경계/reset parity: 각 196 cases / 1,000,818 samples bit exact.
- 기존 realtime: 각 144 routes, frozen 2,645,088 + fade 388,800 floats bit exact.
- 개인 IR 원본 WAV 삭제 후 embedded bytes/metadata 및 오디오 복원, worker cancellation/reprepare PASS.
- 동일 상태 CAB replay: 12 routes × 2,000 callbacks × 2 binaries = **48,000 callbacks**.
  전체 출력 SHA256 및 callback별 state/fade 일치, 실제 IR 길이 8,161, callback new/delete 관측 0.
- 기존 integration entry 통과는 deadline miss 0을 뜻하지 않는다. 모든 miss를 그대로 보존했다.

예시: v2·96kHz·32 samples·stereo, 각 2,000 callbacks, budget 333.333333µs:

| Linux negative control | CPU p50 µs | CPU p99 µs | Wall miss | CPU over | CPU 동반 wall miss |
|---|---:|---:|---:|---:|---:|
| baseline | 48.387 | 256.792 | 15 | 15 | 15 |
| candidate 위임 경로 | 48.492 | 325.634 | 16 | 19 | 16 |

이 1회 A/B는 Apple 후보의 timing experiment가 아니다. A/A noise screening도 하지 않았으므로
어떤 수치 차이도 후보의 향상/악화로 귀속하지 않는다. 전체 24 route-summary는
`linux/callback-summary.csv`, 16개 실행 로그와 bit CSV, binary/source fingerprints, 결과 JSON을 보존했다.
원시 callback/output 파일의 fingerprint는 `linux/raw-manifest.json`에 있다.
원시 대용량 데이터 자체는 이 repository 보고서에 포함하지 않았다.

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

## 측정 코드와 공개 커밋 연결

Linux collector가 기록한 로컬 source SHA `e643466`과 공개 실험 커밋
`6abf7e251b9a78d5dcc840653cb40f984111f88f`는 동일한 tree
`a48289d32de25ea796ce98a3ab0fb0a6916e9b33`를 가리킨다.
연결된 GitHub API로 같은 tree를 게시했기 때문에 commit metadata만 다르다.
측정 binary SHA256 및 실제 컴파일 명령은 `linux/build.json` / `linux/manual-build.json`에 기록했다.
본 후속 보고서 커밋에서 수치를 새로 측정했다고 주장하지 않는다.
