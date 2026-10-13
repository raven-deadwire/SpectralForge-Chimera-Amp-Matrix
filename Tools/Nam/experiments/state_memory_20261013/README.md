# Náströnd 메모리 구조 후속 검증 — 2026-10-13

**전체 재현을 위한 우선 개발안은 비선형 상태를 보존하는 Chimera 전용 하이브리드다. 현재 TONE3000용 단일 NAM의 대체품으로 승인한 것은 아니다.** 긴 receptive field는 일반 NAM Core의 별도 연구 경로이며, TONE3000 호환 경로는 기존 A2와 지원 POST EQ를 묶는 제한적 개선안으로 남는다. 음향 정확도 기준과 제품 DSP는 그대로이며 **`release_approved=false`**다.

이번 작업은 학습 횟수를 늘리지 않았다. 10월 11일 checkpoint에서 export한 기존 가중치, 캡처 배열, 10월 12일의 입력 quartet·오차·EQ 실험을 재사용했다. 새 optimizer update는 **0**이다. 새 작업은 구조/실행 비용 검사, 원본 내부 상태에 대한 통제 실험, 손실 없는 teacher 분리 시제품이다. 실제 신경망을 교체한 하이브리드의 fidelity PASS를 주장하지 않는다.

## 최신 상태와 이미 완료된 실험

- PR #22는 Draft, head `859fea9bc49c4b9981c926a314655e485c51027c`. 최신 NAM smoke run `38113606377`은 SUCCESS지만 음향 승인과 다르다. 같은 head의 Node24 계약 run `38113606380`은 FAILURE다.
- 기존 5,000 update recipe 비교, 1,000 update centered continuation 및 10월 12일 출력 head ridge/지원 EQ 적합을 다시 실행하지 않았다.
- 10월 11일 독립 입력: ESR 평균 0.099505 → 0.088075, fidelity 0/5.
- 추가로 발견한 10월 12일 연구 브랜치 `experiment/nam-alternatives-20261012`, commit `d93f4f26711912c5317fec1dfa4a6f674c988dab`: 다른 독립 입력에서 사전 선택 평균 ESR 0.101233505 → 0.098946431, 2.2592% 개선, fidelity 0/5. 서로 다른 입력의 평균을 직접 비교하지 않는다.
- 이전 refinement ZIP의 SHA-256을 재확인했고 alternatives ZIP의 307개 보존 파일은 크기/해시가 모두 일치했다. [provenance.json](provenance.json)에 source/archive/compiler/binary 식별을 기록했다.

이전 전체 보고서는 [10월 12일 연구](../alternatives_20261012/README.md)다. 현 소스는 그 브랜치에서 분리했으며 PR #22를 변경하지 않는다.

## 공식 구현과 전달 단위

확인한 trainer main은 `0072676419459f5d39e36f5b9fd4172f28d62cbf`, TONE3000 main은 `ab5e5ae4388753184687d67816815aa481f2cea9`다. 현재 플러그인이 고정한 Core는 여전히 `1f42f88535884450104b8711d7595019afa0495b`이며 기존 PR의 Core와 같다.

| 접근 | 단일 `.nam` | 현재 TONE3000 | Chimera 및 원본 보존 | 지연·CPU |
|---|---|---|---|---|
| 기존 A2의 추가 학습 | 가능 | 구조를 유지하면 가능 | 파형·어택 오차는 개선 여지. 고정 메모리 한계는 그대로 | 기존 비용/인과 구조 유지 |
| receptive field 확장 | 일반 WaveNet NAM으로 가능; 대상 Core 지원 필요 | **로컬 A2 gate 거부** | 장기 입력 이력을 제공. 가중치 재학습과 별도 NAM runtime 통합 필요 | 과거 이력만 사용하므로 lookahead 지연은 추가하지 않음. 같은 층/폭이면 MAC 수는 그대로지만 버퍼·cache 비용 증가 및 A2 fast path 상실 |
| 원본 비선형 상태 + neural fast path + native 출력부 | **현재 표준 단일 NAM으로 이 custom graph를 전달할 수 없음** | 지원 안 됨 | sag/blocking/bias/feedback·BLOOM·rail을 실제 위치에서 보존할 수 있음. 가장 직접적인 장기 구조이나 neural 대체부는 미검증 | 인과 상태 갱신 자체의 lookahead는 0. 기존 4× OS/6-sample 정렬 보존 필요. 상태 연산은 고정 크기이나 전체 CPU는 별도 실측 필요 |
| A2 뒤 지원 POST EQ | NAM에 EQ가 포함되지 않음 | **`.t3kpreset` 쌍으로 지원**; 10월 12일 engine/preset 검증 재사용 | 선형 잔향을 보완. 비선형 회복/어택 이력을 일반적으로 해결하지 못함 | IIR은 추가 lookahead 0이나 주파수별 위상/군지연 존재. 네 개 활성 bell은 고정 크기 연산; 전체 실시간 인증은 미완료 |
| A2 뒤 임의 recursive filter/원본 출력부 | 현재 TONE3000이 지원하는 단일 NAM에 담을 수 없음 | 임의 pole/topology는 지원 EQ와 구분 | Chimera custom path에서 가능. 원본을 분리해 목표를 재학습해야 하며 기존 full-output NAM 뒤에 그대로 붙이면 중복 착색 가능 | 소수 pole은 저비용 설계 가능. 출력 rail은 비선형이므로 고정 LTI EQ/IR로 정확히 대체되지 않음 |

공식 NAM Core 최신 main `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`에는 `Sequential`과 LSTM/일반 WaveNet 실행 경로가 있다. 따라서 모든 복합/재귀 구조가 NAM 형식으로 영원히 불가능하다는 주장은 틀리다. **그러나 현재 TONE3000의 고정 Core와 로컬 A2 허용 범위는 별개**다. `SlimmableContainer`는 크기 tier 선택이지 임의 직렬 체인이 아니다. 최신 Core의 `Sequential`도 임의 Chimera DSP 클래스를 자동 직렬화하는 기능은 아니다.

기존 pinned Core의 A2 fast path가 `condition_dsp`를 무시할 수 있다는 10월 12일 관찰은 중요하다. 최신 Core는 non-null conditioner를 fast path에서 거부하도록 수정돼 있다. 현재 TONE3000의 의존성을 몰래 바꾸거나 JSON 필드를 삽입해 지원되는 모델처럼 취급하지 않았다.

공식 소스:
- [TONE3000 현재 로더](https://github.com/tone-3000/tone3000-plugin/blob/ab5e5ae4388753184687d67816815aa481f2cea9/plugin/src/ProcessorModelLoader.cpp), [고정 Core gitlink](https://github.com/tone-3000/tone3000-plugin/tree/ab5e5ae4388753184687d67816815aa481f2cea9/plugin/NeuralAmpModelerCore), [지원 EQ](https://github.com/tone-3000/tone3000-plugin/blob/ab5e5ae4388753184687d67816815aa481f2cea9/plugin/src/BlockEq.cpp).
- [A2 shape 검사](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/1f42f88535884450104b8711d7595019afa0495b/NAM/wavenet/a2_fast.cpp), [최신 conditioner 검사](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842/NAM/wavenet/a2_fast.cpp), [최신 Sequential](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842/NAM/sequential.cpp).
- [공식 trainer receptive field](https://github.com/sdatkinson/neural-amp-modeler/blob/0072676419459f5d39e36f5b9fd4172f28d62cbf/nam/models/wavenet/_wavenet.py), [공식 A2 설명](https://www.tone3000.com/guides/nam-a2-the-complete-guide). Stack 6,332 + head의 추가 15 = 전체 6,347 samples다. **132ms 문맥은 132ms 처리 지연이 아니다.**

## 새 실험 1 — 이력 확장과 실제 로더

기존 Fimbulvetr export의 가중치를 전혀 바꾸지 않고 1보다 큰 dilation만 2/4/8배로 늘렸다. 1-dilation 층과 16-tap head는 유지했고 입력 support가 중간에 빠지지 않는지 계산했다. 변경 모델은 **실행 시험용 fixture이며 학습된 음향 후보가 아니다.**

| 구조 | 전체 RF | 입력 문맥 | 일반 Core 실행 | TONE3000 A2 gate | 64/256 block 출력 차이 | 기존 Fimbulvetr quiet 창들의 최악 RMS 하한 |
|---|---:|---:|---|---|---:|---:|
| 원래 A2 | 6,347 | 132.229ms | PASS | PASS | 0 | −68.0333dBFS |
| 확장 ×2 | 12,649 | 263.521ms | PASS | FAIL | 0 | −86.4674dBFS |
| 확장 ×4 | 25,253 | 526.104ms | PASS | FAIL | 0 | 아래 주석 |
| 확장 ×8 | 50,461 | 1,051.271ms | PASS | FAIL | 0 | 아래 주석 |

확장 ×4/×8에서 동일 입력의 nonzero target을 가진 zero-history witness가 사라져 이 하한으로는 blocker를 증명하지 못했다. JSON의 −200dBFS는 계산 표시 floor이지 모델의 실제 잔차가 아니다. 원래 어려운 단일 창은 ×2부터 zero-history subset이 없다. 어느 결과도 활성 구간 정확도나 충분한 학습을 보장하지 않는다. 모든 계산은 기존 4,096 창·6,347 skip·레벨 anchor를 그대로 사용했다.

**CPU 판정은 미확정이다.** 공유 Linux 환경의 wall callback p99는 64샘플에서 1.747–7.087ms로 변동했고, 모든 조건에서 deadline miss가 있었다. 256샘플에서도 일관된 순위가 나오지 않았다. 시작/예열 시간을 별도 실행해 차감한 child CPU 후속 측정도 nonpositive 차감값으로 실패했다. 이 수치로 확장 모델이 더 빠르다고 판단하지 않았으며 CPU PASS나 절감률을 주장하지 않는다. 층·가중치·MAC 수는 그대로, 시간 버퍼는 증가하며 fast path가 바뀐다는 구조적 비용까지만 확정한다. 전체 오디오 메모리 사용량과 Windows/macOS 실시간 비용은 아직 미측정이다.

근거: [rf-results.json](rf-results.json), [CPU 측정 실패](cpu-invalid-attempt.json), [실패 로그](cpu-invalid-attempt.log). 계산·로딩·block 일관성의 성공과 실시간 비용의 미확정을 구분한다.

## 새 실험 2 — 어떤 상태가 장기 반응을 만드는가

10월 12일의 high/low prefix와 probe/zero quartet 입력을 그대로 재사용했다. 150/750ms 휴지 뒤 새 음이 시작되기 직전에, 별도 소스 overlay에서 선택한 상태만 한 번 0으로 만들었다. 3채널 × 2 gaps × 7 modes × 4 renders = **168개 통제 렌더**다. 학습이나 EQ 적합은 없다.

비교량은 `(high+probe − high+zero) − (low+probe − low+zero)`의 200ms RMS이며 원래 channel anchor를 적용한다. 아래 수치는 **진단용 incremental difference**, NAM fidelity 잔차가 아니다. 일부 상태를 지운 소리는 수정된 teacher이므로 승인 reference/candidate로 사용할 수 없다.

| 채널 / 휴지 | 원본 | 출력 공진·필터 상태만 초기화 | sag+blocking+bias 초기화 | blocking만 초기화 |
|---|---:|---:|---:|---:|
| Nidhoggr / 150ms | −47.21 | −47.21 | −61.30 | −61.40 |
| Nidhoggr / 750ms | −48.31 | −48.31 | −171.58 | −144.42 |
| Fimbulvetr / 150ms | −45.22 | −45.22 | −47.42 | −45.93 |
| Fimbulvetr / 750ms | −69.35 | −69.35 | −160.05 | −74.13 |
| Ragnarök / 150ms | −38.75 | −38.75 | −41.08 | −41.06 |
| Ragnarök / 750ms | −82.73 | −82.73 | −168.09 | −168.09 |

단위는 dBFS다. 전체 native core audio memory 초기화 control은 −182.30dBFS 이하로 내려갔다. 변경하지 않은 overlay의 3채널 validation 1초 접두부는 기존 캡처와 bit-identical이고, 6개 no-op quartet 결과도 이전 수치와 일치했다. 실제 wrapper의 4× oversampling과 6-sample 지연을 유지했다.

Nidhoggr의 750ms 차이는 출력 잔향으로 설명되지 않으며 blocking memory를 직접 통제하면 크게 사라진다. Fimbulvetr의 sag만 초기화한 값은 −70.89dBFS, blocking만은 −74.13dBFS지만 함께 초기화하면 −160.05dBFS다. **상태의 효과를 독립 비율로 합산할 수 없는 상호작용**이다. 150ms에서는 느린 상태만 초기화해도 차이가 남으므로 다른 recursive 필터·feedback·envelope 상태를 버려도 된다는 근거도 없다. 이 실험을 전체 기존 ESR의 원인 비율로 환산하지 않는다.

근거: [state-results.json](state-results.json), [재현 코드](probe_states.py), [overlay 생성기](prepare_state_probe.py).

## 새 실험 3 — native 하이브리드 teacher 분리

`prepare_hybrid_teacher.py`는 고정 소스에 대해 `tickFront`와 `tickPost` 경계를 생성한다. 앞단에는 원래 비선형 회로와 모든 상태를 유지하고, 뒷단에는 BLOOM, voicing, output/DC filter, master와 최종 nonlinear rail을 유지한다. 경계는 double이며 반올림·trim·정렬을 추가하지 않는다. 제품 헤더는 편집하지 않았다.

기존 validation 입력 접두부를 사용한 **5채널 × 48/192kHz 내부 rate, 두 audio 채널, gain 변경 smoothing 및 reset: 10/10 비교에서 최대 오차 0**, 총 3,840,000 sample 비교를 확인했다. 이는 **native core를 손실 없이 분리할 수 있다는 시제품 검증**이다. 192kHz 내부 테스트를 새로운 DAW sample-rate 지원이나 전체 oversampling wrapper 검증으로 읽으면 안 된다.

원본 앞단을 그대로 실행하므로 이 시제품은 새로운 neural hybrid의 정확도·CPU 개선을 입증하지 않는다. 다음 모델이 teacher의 정답 state를 추론 시 입력받는 방식은 허용하지 않는다. 인과적으로 갱신할 native state 또는 학습된 recurrent state가 필요하다. 원래 장기 비선형 상태를 가진 앞단을 다시 bare A2로 바꾸면 확인한 문제를 재도입한다.

근거: [hybrid-split-results.json](hybrid-split-results.json), [HybridSplitProbe.cpp](HybridSplitProbe.cpp).

## 개발 결정과 다음 완료 조건

1. **우선: 원본 상태를 보존하는 Chimera 연구 경로.** 이번 teacher 경계를 실제 4× wrapper의 capture tap으로 연결하고, blocking/bias/sag/feedback 및 나머지 recursive state의 위치·초기화·계수 갱신을 명시한다. 이후 입력과 자체 상태만으로 동작하는 neural fast-path 후보를 구성한다. 이 단계 전에는 학습을 시작하지 않는다. frozen capture와 native DSP 음색을 바꾸지 않는다.
2. **비교군: 526ms/1,051ms 일반 WaveNet.** 소규모 새 구조 실험의 입력 support와 실제 Core 실행은 확인됐으므로 별도 런타임 후보로 유지한다. 기존 모델의 dilation만 바꾼 fixture를 음향 후보로 사용하지 않는다. Windows/macOS buffer 32/64/128/256, 모노/스테레오/Dual/Matrix CPU·메모리 비용과 reset/restore를 먼저 측정한다. TONE3000 단일 파일 요구의 대체로 간주하지 않는다.
3. **TONE3000 호환의 제한적 병행안: Surtr/Fimbulvetr의 고정 지원 POST EQ + chain-aware 목표.** 10월 12일의 동결 모델·EQ/selection을 재사용한다. 새로운 full-weight 실험은 기존 head 적합 반복이 아니라 전체 체인 손실과 연속 필터 상태가 가설이다. Nidhoggr의 장기 비선형 회복과 Ragnarök transient 문제를 해결했다고 주장하지 않는다. `.nam`과 `.t3kpreset` 전달 단위를 구분한다.

어느 neural 후보든 train-only 적합, validation 선택, model+state/filter hash 동결 후 새로운 독립 테스트와 실제 악기 DI 평가를 거쳐야 한다. 이미 본 10월 11·12일 테스트는 앞으로 회귀 자료일 뿐이다. 평가는 Full ESR <0.01, active median/p95/worst ≤0.005/0.01/0.02, quiet RMS/peak ≤−70/−60dBFS, RMS/peak level ≤0.5/1dB를 그대로 적용한다. 원본 채널 성격·어택·crest·대역 전력·회복을 함께 검사한다.

real DI/블라인드 청음, 전체 GUI/DAW 복원/종료, realtime CPU 모두 미승인이다. 제품에 새 모델/외부 필터를 삽입하지 않았고 공개 모델이나 릴리스를 만들지 않았다.

최종 [보존 증거 검증](verification.json)은 원래 validation 5채널과 노출 테스트 배열의 해시, RF 4종의 engine/블록 일관성, 통제 렌더 168개 파일 해시와 모든 200ms 통계 재계산, native split 10개 결과를 확인했다. 이는 새로운 학습·독립 테스트·청음 승인이 아니다.

## 재현

NumPy, soundfile 및 C++20 compiler가 필요하다. `--data`는 이전 full-run archive의 `data/`, `--refinement`는 refinement ZIP의 추출 루트, `--previous`는 alternatives ZIP의 추출 루트다. Core/JUCE pin은 `provenance.json`과 일치시킨다.

```sh
python build_probes.py --repo /path/to/chimera --core /path/to/pinned-core \
  --juce /path/to/JUCE --out /path/to/new-build
python probe_rf.py --refinement /path/to/refinement --data /path/to/data \
  --tool /path/to/new-build/core-memory-probe --out /path/to/new-rf-results
python probe_states.py --previous /path/to/alternatives --data /path/to/data \
  --tool /path/to/new-build/teacher-state-probe --out /path/to/new-state-results
python prepare_hybrid_teacher.py --repo /path/to/chimera --out /path/to/new-hybrid
g++ -std=c++20 -O2 -I/path/to/chimera/Source -I/path/to/new-hybrid \
  -I/path/to/pinned-core/Dependencies/nlohmann HybridSplitProbe.cpp -o /path/to/hybrid-probe
/path/to/hybrid-probe /path/to/new-rf-results/runtime-input.f32 \
  /path/to/data/channel-settings.json
```

재현 실행을 새로운 독립 테스트로 세지 않는다. 보존 파형의 재계산은 학습과 분리하며, CPU 측정 실패도 함께 남긴다.
