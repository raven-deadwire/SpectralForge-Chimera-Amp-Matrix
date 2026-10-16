# 스칼라 CAB 메모리 접근 최적화 — 독립 실험

2026-10-16 KST. PR #76의 S8T 개선 미입증 결론을 유지한다. 수동 NEON·unroll 조합을 재시도하지 않는다. 이 브랜치에서 추가하는 것은 Tools와 전용 CI뿐이다. 실험 헤더는 별도 detached checkout에서만 생성한다. `integration_eligible=false`, `release_approved=false`를 항상 유지한다.

## 출발점과 비용 해석

검증된 스칼라 A는 `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`; 헤더 SHA-256은 `1b0fcc12ed4b20364d94bf044d075cbdc6fe681cfe10121a9b5904e67093788f`다. 현재 main, 미병합 NEON 브랜치, 별도 버전의 전체 제품 성능을 이 기준 결과로 대체하지 않는다. 제품 후보로 이전하려면 그 소스에서 다시 검증해야 한다.

- 작은 블록은 FFT 4B, IR 파티션 3B다. residue bank는 지연 파티션의 입력 스펙트럼을 이미 연속 배치한다. 새 링 전치를 제안하는 것만으로는 신규 최적화가 아니다.
- 첫 청크는 p=1…P−1 순으로 `pastSum`을 만든다. 부분 청크는 이를 재사용하고 p=0만 마지막에 더한다. 이 순서와 캐시 유효기간을 바꾸지 않는다.
- 파티션 내부 planar real/imag는 이미 연속이며 AppleClang 자동 벡터화 대상이다. PR #75는 어셈블리 비용 차이를 확인했으나 캐시/DRAM 병목은 측정으로 확정하지 않았다.
- 32/64 블록에서 mono 출력 누산은 각각 130/258 floats, stereo는 256/512 floats다. 단일 출력 버퍼는 작으므로 타일링의 캐시 이득을 전제할 수 없다. 여러 mic/active/fading kernel의 working set과 공유 IR 재사용이 실제 CAB 콜백에서 중요하다.

## 후보 비교 — 우선순위는 가설이며 성능 순위가 아님

| 우선순위/후보 | 최소 변경과 기대 효과 | 위험·한계 | 독립 구현 |
|---|---|---|---|
| 1 / split | 링 wrap 전후 두 연속 구간. 같은 p 순서로 포인터를 stride만큼 전진; partition별 wrap 분기/곱셈 감소 | 컴파일러가 이미 일부 hoist할 수 있고 MAC 대비 주소 비용이 작음. 분할 루프 코드 증가 가능 | 생성기 `split` |
| 1 / fusedlead | `pastSum`→output 복사 후 leading MAC을 한 루프로 합침. cached delayed sum을 직접 읽고 output에 기록 | 기존 copy/SIMD와 컴파일러 FMA 결과가 달라질 가능성은 exact-bit 게이트로 차단. 이득은 leading 한 파티션에 한정 | 생성기 `fusedlead` |
| 2 / clearhead | FFT 입력에서 쓰이는 B 샘플만 +0 초기화. 초기 생성/reset으로 tail은 +0이며 처리 중 tail을 쓰지 않음 | FFT 호출 수/크기/pack/copy는 그대로. 대역폭 감소가 총 콜백 개선으로 이어지는지는 미확인 | 생성기 `clearhead` |
| 3 / tile32 | 주파수 32-bin 타일 밖으로 순회를 옮겨 출력 누산 재사용 거리를 축소; bin마다 p 순서 유지 | 원본 출력도 L1에 들어갈 크기. 입력/IR의 긴 stride·동일 cache line 재접근·추가 partition 순회로 더 느려질 수 있음 | 생성기 `tile32` |
| 보류 / 스펙트럼 정렬·padding | mono의 홀수 bins로 생기는 plane/partition misalignment 해소 | 메모리 증가, pack/unpack·Prepared 공유/cache key·주소 geometry 변경 필요. unaligned 접근이 병목이라는 근거 없음 | 미구현 |
| 보류 / frequency-major 또는 AoSoA | bin tile의 IR/history를 더 가까이 배치 | ring write transpose·복사 추가, 공유 스펙트럼 계약 변경. 한 번에 여러 원인이 달라지므로 우선하지 않음 | 미구현 |
| 보류 / FFT 대체·실수 FFT 확대 적용 | transform 자체나 pack/unpack 비용 축소 | mono는 이미 플랫폼 real FFT. stereo complex는 L+iR 두 채널 독립 처리. 두 real FFT, 새 backend, 스케줄/정규화 변경은 비트 일치·지연 위험 | 미구현 |
| 보류 / prefetch·파티션 병렬화 | stall을 숨길 가능성 | 하드웨어 counter 근거 없음. prefetch 오염 가능; 병렬화는 합산 순서·worker/실시간 계약 위험 | 미구현 |

`clearhead`의 초기화 저장량 감소(호출 중 내부 B 블록 완료마다)는 mono 384/768 bytes, stereo 768/1536 bytes(각 32/64 samples)다. 이는 정적 저장량 계산이며 측정된 DRAM 트래픽이나 CPU 개선율이 아니다. `fusedlead`도 별도 복사 2*bins floats를 없애지만, 캐시 writeback 감소를 보장하지 않는다. 어떤 후보도 FFT backend/호출 일정/latency/부동소수점 식·fast-math 설정/IR/프리셋을 바꾸지 않는다. 조합 후보는 개별 이득 입증 후 별도 대조가 필요하다.

## 자동 검증과 사전 판정

전용 workflow `cab-scalar-memory.yml`는 각 후보와 스칼라를 **같은 arm64 runner 할당 내**에서 비교한다. 후보 간 별도 runner 수치를 직접 나누어 순위를 매기지 않는다. Xcode 16.4 / AppleClang 17, JUCE commit과 PR #74 프로토콜을 고정했다. 모든 A/B 빌드가 끝난 후 순차 측정하며 A/A는 정확히 같은 A 바이너리다.

- 96kHz / 32·64 samples × mono·stereo × 3 CAB engine = 12경로, 3 rigs/6 mics.
- control: stage clock OFF 전체 CAB 콜백 thread CPU·wall p99·deadline miss·CPU over-budget. profile: 별도 실행 spectral MAC. 둘을 합치거나 빼서 추정하지 않는다.
- 후보마다 control/profile 각 6 ABBA/BAAB + 교차 배치 6 A/A = 96 trials / 1,843,200 raw callbacks. worker/automation 및 고정 publication 구간과 모든 실패·이상치를 보존한다.
- 고정 publication을 주 인과 비교로 사용. quartet 단위 bootstrap 10,000회, 95% 개선 하한이 2%와 A/A 최대 변동폭 모두를 넘고 양쪽 순서 효과가 양수여야 한다. CPU와 MAC 모두 12/12 개선 및 control CPU/wall p99 확정 회귀 없음이 기존 성능 기준이다. 기준을 낮추지 않는다.
- 추가 보수적 review 조건: 고정 구간 각 control 경로의 wall miss와 CPU over-budget 수가 A보다 증가하지 않아야 한다. 희소 miss 감소 자체로 인과적 안정성 개선을 주장하지 않는다. 비동기 구간 miss는 노출 차이를 포함하는 보조 관찰이다.
- 양쪽 모두 원래 CTest 5개·deadline 66행·144 realtime 경로·2,645,088 frozen + 388,800 fade float exact bits·immutable spectra·추가 지연 0이 필요하다. 원래 allocation/deletion, worker scheduling, latency 계약을 수정하지 않는다.
- native `check_kernels.py`: 제품 헤더에서 실제 메서드를 추출하여 bin 1…1024(홀수 포함), P=1…64, 모든 링 위치, step=1/3, 첫/부분 청크, 부분 입력 변경, signed zero/subnormal을 비교한다. FFT·제품·실시간 검사를 대체하지 않는다.
- `audit.py`는 manifest 모든 바이트와 probe·컴파일 명령을 확인하고, 96 trial/294 command 순서, 3,456 원시 요약·72 통계행·최종 결정을 재계산한다. 후보 헤더를 pinned scalar와 생성기로 재검증한다.
- 전체 PluginProcessor/DAW/물리 오디오 인터페이스/청음은 이 측정 범위 밖이다. 개선 입증 후에도 통합은 별도 검토이며 이 도구는 제품 파일 반영·merge·release를 실행하지 않는다.

## 재현

```bash
python Tools/CabNeonAB/test_analysis.py
python Tools/CabScalarMemory/test_contracts.py
python Tools/CabScalarMemory/check_kernels.py --compiler clang++ --output /tmp/new-kernel-evidence
# scalar parent와 candidate는 각각 pinned A의 clean detached checkout이어야 한다.
python Tools/CabScalarMemory/prepare.py --candidate /path/candidate --kind split
python Tools/CabScalarMemory/run.py --parent /path/parent --candidate /path/candidate --kind split --build /path/new-build --output /path/new-evidence
# evidence 디렉터리 내부 파일을 ZIP 최상위에 담은 후:
python Tools/CabScalarMemory/audit.py evidence.zip audit.json
```

기존 수집 결과: [PR #76](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/76), [PR #75 구조 진단](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/75). 해당 결과를 새 후보의 측정값으로 재사용하지 않는다.
