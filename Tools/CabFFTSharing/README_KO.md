# CAB 입력 FFT 중복 조사와 격리 후보

고정 제품 소스: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
JUCE 8.0.8: `d6181bde38d858c283c3b7bf699ce6340c050b5d`.
PR #76의 수동 NEON/S8T 독립 비교는 완료됐으며 통합 보류 결정을 유지한다.
이 작업은 별도 구조 후보이며 `Source/`, 기존 테스트, IR, 릴리스 설정을 수정하지 않는다.

## 실제 호출과 입력

| 경로 | 고정 소스의 처리 순서 | 공유 판단 |
|---|---|---|
| Mic A/B | `Cabinet.h::process`: 원본을 `second`로 복사 → B 재귀 처리 → A HP → A convolution | 원본은 같지만 각 HP 계수·내부 상태가 독립. 컷 설정만 비교하면 불충분 |
| B 음소거/재활성화 | `renderB`가 false이면 처리 중단. 재개 때 `micB->reset()` | A의 HP/입력 이력은 진행하지만 B는 초기화. 재개 직후 공유를 보장할 수 없음 |
| active/fading | HP 출력에서 `alternate` 복사 → 새 active 처리 → 이전 fading 처리 → 50ms 혼합 | 현재 입력 샘플은 같지만 새 커널의 부분 블록 누적은 비어 있음. FFT 전체 입력이 일치할 때만 재사용 |
| 모노/스테레오 | 모노는 real FFT, 스테레오는 L+iR complex FFT | 서로 호환되지 않음. 스테레오를 모노 합산하지 않음 |
| Classic | `Engine::process`에서 CAB 한 개 | 그 CAB의 마이크·페이드만 대상 |
| Dual full range | 원본 복사 후 각각 다른 Amp 상태를 통과 | 같은 원본이 동일 CAB 입력을 뜻하지 않음. rig 간 공유 제외 |
| Dual crossover/Matrix | 독립 크로스오버 대역 → band tone/Amp. Matrix 저역은 별도 compressor/DI 혼합 | rig 간 공유 제외 |
| 개인/레거시 IR | `Kernel`의 JUCE Convolution fallback | 내부 FFT에 접근하지 않음. 기존 처리 그대로 |

`PluginProcessor.cpp`는 A의 low cut을 lane에서, B low cut을 별도 파라미터에서 갱신한다.
LP·마이크 gain/polarity/delay·A/B blend·CAB enable dry/wet는 convolution 뒤이므로
현재 FFT 입력을 직접 바꾸지는 않는다. B 실행 여부는 blend 등에 의해 달라진다.

`Prepared`의 B는 최대 host block의 다음 2의 거듭제곱이다. B<=128에서는 N=4B,
IR partition=3B, input ring step=3이고 그보다 크면 N=2B, partition=B, step=1이다.
실제 callback 길이가 B보다 작거나 경계를 가로지르면 누적된 time buffer 전체를 매번 변환한다.
같은 callback 길이나 같은 cursor만으로 FFT 입력 일치를 보장할 수 없다.
특히 불규칙 host block 도중 새 커널이 설치되면 새 커널은 position=0에서 시작하고 기존 커널은
부분 위치를 유지한다. 이후에도 내부 FFT 블록 경계가 어긋날 수 있어 같은 현재 신호와 HP만으로
재사용이 복구된다고 가정할 수 없다. 전체 time buffer 비교는 이 경우도 안전하게 miss 처리한다.
IR 길이와 partition 개수는 마이크마다 다를 수 있다. 순방향 FFT 결과 재사용에는 IR 일치가 필요 없지만
지연 partition 합산·스펙트럼 링·역 FFT·overlap은 각 커널에서 계속 독립 처리해야 한다.

## 후보의 정확한 범위

`prepare.py`는 Git의 고정 객체에서 A/B shadow tree를 생성한다. A는 원본 그대로이며,
B의 `ModeledCabConvolution.h`/`Cabinet.h`에만 다음 실험을 적용한다.

- CAB parent가 prepare 시 고정 12-entry scratch를 할당한다. 자식 Mic B와 해당 CAB의
  active/fading 커널만 같은 scratch를 사용한다. 콜백 시작 때 모든 entry를 무효화한다.
- FFT 종류, 입력 float 수, bin 수를 비교한 뒤 **FFT time buffer 전체 바이트를 memcmp**한다.
  signed zero도 같아야 한다. 같으면 planar spectrum을 각 커널의 **개별 ring 슬롯**에 복사한다.
- 누적 input time, FFT 객체, ring 크기/커서, `pastSum`, inverse FFT, overlap, fade, HP/LP,
  gain/delay/blend 상태, Prepared 소유권과 worker publication 프로토콜은 기존 그대로다.
- miss/용량 초과 시 원래 FFT를 실행한다. capacity/배열 크기는 고정이며 eviction·잠금·shared_ptr
  복사·할당·해제·대기·다음 블록 지연은 cache path에 없다. entry는 callback 밖에서 재사용하지 않는다.
- scratch의 가변 상태는 해당 audio owner에만 있다. worker는 참조하지 않는다. 서로 다른
  processor/rig 사이에 전역·thread-local 변환 캐시를 두지 않는다. 같은 processor의 동시 process는
  기존 클래스와 마찬가지로 지원 범위가 아니다.

ring 자체를 공유하지 않으므로 서로 다른 IR 길이, 새 커널의 빈 ring, B reset 이후 이력을
다른 마이크의 이력으로 바꾸는 문제가 없다. 정확 입력 검사·복사 비용 때문에 실제 FFT 호출 감소가
callback CPU 개선을 보장하지는 않는다. 완전 공유 front-end 설계보다 보수적인 첫 후보다.

## 검증과 성능 프로토콜

1. A/B 모두 원래 `CabRealtimeRegressionTests.cpp`를 수정 없이 실행한다.
2. 3개 authored 모델 계열 × 17/32/64/128/256/512 maximum block × mono/stereo × steady/stress,
   총 72개 경로에서 각각 2,600 callbacks의 모든 출력 float를 비교한다.
   stress는 불규칙 chunk, B mute/wake, B HP 변경/복귀, gain/delay/polarity, 부분 위치 reset,
   A/B publication과 50ms fade, enable 복귀 및 silence tail을 포함한다.
3. 개인 IR fallback, 무음 우측 채널, 서로 다른 processor의 병렬 실행을 추가한다.
   C++ new/delete/정렬 new를 callback thread에서 감시한다. 직접 C malloc 호출을 가로채는
   시험은 아니므로 전체 allocator interception이나 TSAN race-free 증명으로 해석하지 않는다.
4. 모든 빌드와 정확성 시험이 끝난 뒤 96kHz·32/64 samples·mono/stereo·3계열, 12경로의
   steady matched-HP workload에서 control/profile별 6개 ABBA/BAAB quartet와 교차 A/A를 측정한다.
   A/A의 두 논리 arm은 동일 A 바이너리다. 각 trial 1,600회, 앞 200회 warmup을 별도 보존한다.
   총 raw callback 1,843,200개이며 세 rig/여섯 mic를 포함한 **CAB 전체** thread CPU/wall이다.
   임의로 trial 재실행·outlier 삭제·우선순위 변경을 하지 않는다.
5. CPU mean의 quartet log-ratio를 10,000회 bootstrap한다. 95% 하한이 2%와 최대 A/A 효과를
   모두 넘고 ABBA/BAAB 각 순서에서 개선돼야 통과다. p99 회귀도 A/A envelope와 비교한다.
   profile은 실제 forward FFT 호출 감소와 MAC 호출 일정 보존을 확인한다. B에서 pack이
   forward 계측 범위로 이동했으므로 stage 시간 차이를 순수 FFT/MAC 성능 차이로 주장하지 않는다.

측정 workload는 의도적으로 동일 HP가 계속 실행되는 공유 가능 경로다. stress CSV의 CPU는
설명 자료이며 반복 paired 성능 판정을 대체하지 않는다. 서로 다른 HP/재활성화 직후의 cache miss
비용, 비동기 worker 노출, 전체 PluginProcessor/DAW 성능은 별도 후속 범위다.
단일 runner와 12경로의 통계는 모든 Apple Silicon 장치나 다중비교 보정 보장을 의미하지 않는다.

## 재현

```sh
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git ../juce
python Tools/CabFFTSharing/prepare.py ../variants
cmake -S Tools/CabFFTSharing -B ../build -DVARIANTS="$PWD/../variants" -DJUCE_SOURCE="$PWD/../juce" -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --parallel 3
python Tools/CabFFTSharing/run.py --build ../build --output ../evidence
```

macOS arm64 CI는 Xcode 16.4 환경·하드웨어·컴파일 명령·source manifest·후보 patch·성공/실패
raw population을 artifact로 보존한다. 원시 출력/CSV와 JSON은 같은 population에서 생성한다.
Linux 결과는 설계/예비 성능 확인이며 Apple Silicon 성능의 근거로 대체하지 않는다.

**제품 통합/병합/릴리스는 하지 않는다. `release_approved=false`.**
모든 경로가 빨라져도 실제 worker/DAW/청음 수용 및 제품 전체 검증 전에는 통합 승인을 뜻하지 않는다.
