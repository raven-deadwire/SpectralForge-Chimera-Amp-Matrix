# 재현 절차

이 문서는 수동 재현용 명령 순서다. 문서 작성 중 학습·평가·CI를 다시 실행하지 않았다. 기존 결과는 evidence archive로 먼저 확인하며, 완료된 5,000-step 비교나 1,000-step continuation을 호출하지 않는다.

## 1. 소스·입력·환경

실험 체크아웃은 새 evidence archive의 `manifest.json.source_commit`에 기록된 게시 commit을 사용한다. 부모 PR 소스는 `859fea9bc49c4b9981c926a314655e485c51027c`다.

| 구성 요소 | 고정 값 |
|---|---|
| Native teacher | `d93560445c78552768a0ed592e5ec62c3c648fb6` |
| Trainer | `0072676419459f5d39e36f5b9fd4172f28d62cbf` |
| TONE3000 plugin | `ef6f178ae1ac6412b55fec6f058d86e640e5aaaf` |
| Plugin NAM Core | `1f42f88535884450104b8711d7595019afa0495b` |
| JUCE 8.0.8 | `d6181bde38d858c283c3b7bf699ce6340c050b5d` |
| Python 환경 | Python 3.12, PyTorch 2.8.0 CPU, [requirements-ci.txt](../../requirements-ci.txt) |

원래 입력은 **Nastrond-NAM-Refinement-Evidence-2026-10-11.zip**, SHA-256 **`4b3705e3bb515462250f6f59d8d05a96917467efd5b2bd6fdacb78a4615fb6ce`**다. 새 **Nastrond-NAM-Alternatives-Evidence-2026-10-12.zip**에는 `fit/`, `evaluation/`, `selected-supplement/`와 파일별 해시가 있고 원래 입력은 중복하지 않는다.

아래 절대 경로를 자신의 환경으로 지정한다. 의존성 clone/pin, recursive submodule, Python 설치와 두 도구 빌드는 [기존 Local build 문서](../../README.md#local-build), [고정 CI의 설치·빌드 명령](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/.github/workflows/nam-validation.yml), [이번 빌드 기록](tone3000-support.md#local-build-verification-in-this-experiment)을 따른다. 워크플로 dispatch나 PR 생성은 필요하지 않다.

```bash
set -euo pipefail
export NAM_REPO=/absolute/path/to/experiment-checkout
export NAM_WORK=/absolute/path/to/new-reproduction
export NAM_INPUT=/absolute/path/to/restored-original-archive
export NAM_PY=/absolute/path/to/python312-venv/bin/python
export NAM_TRAINER=/absolute/path/to/pinned-trainer
export NAM_RENDERER=/absolute/path/to/ChimeraNamRender
export NAM_TOOL=/absolute/path/to/ChimeraTone3000Tool
export NAM_BUILD_RECORD=/absolute/path/to/verified-native-tool-build.json
export PYTHONPATH="$NAM_TRAINER"
export NAM_EXP="$NAM_REPO/Tools/Nam/experiments/alternatives_20261012"
export NAM_DATA="$NAM_INPUT/failed-full/data"
export NAM_MODELS="$NAM_INPUT/refinement/legacy"
export NAM_OLD_TEST="$NAM_INPUT/fresh-test"
export NAM_FIT="$NAM_WORK/fit"
export NAM_EVAL="$NAM_WORK/evaluation"
export NAM_SUPPLEMENT="$NAM_WORK/selected-supplement"
mkdir -p "$NAM_WORK"
```

입력을 아직 복원하지 않았다면 archive SHA-256을 위 값과 확인하고 다음 순서로 **바깥 zip과 안쪽 zip**을 각각 푼다. 이미 복원했다면 재추출하지 않는다.

```bash
export NAM_ARCHIVE=/absolute/path/to/Nastrond-NAM-Refinement-Evidence-2026-10-11.zip
sha256sum "$NAM_ARCHIVE"
"$NAM_PY" -m zipfile -e "$NAM_ARCHIVE" "$NAM_INPUT"
"$NAM_PY" -m zipfile -e "$NAM_INPUT/failed-full-37947011057.zip" "$NAM_INPUT/failed-full"
```

이번 TRAIN은 `failed-full/data/train/`의 **96초**다. baseline은 추가 학습 후 선택된 `refinement/legacy/Nastrond-*.nam`이며, `legacy`라는 폴더명이 A1 구조를 뜻하지 않는다. `official-a2/`, v0.2 export 또는 다른 TRAIN으로 바꾸지 않는다. 적합 도구가 데이터 매니페스트의 해시를 확인한다.

**새 빌드 기록:** evaluator는 역사적 바이너리 해시를 검사한다. 동일 바이너리를 재사용하면 기존 `native-tool-build.json`을 지정한다. 새 바이너리라면 [원래 검증 절차](tone3000-support.md#local-build-verification-in-this-experiment)대로 의존성 핀과 8개 DSP header를 확인하고, 원래 설정으로 TRAIN의 첫 **192,000샘플**을 다섯 채널 렌더해 archived target 접두부와 bit-exact인지 확인한 별도 로컬 기록을 만든다. 기록에는 `status`, `capture_source`, `dependencies`, 실제 `binaries.*.sha256`과 다섯 `channels[].bit_exact`가 필요하다. 실패하면 원인을 조사한다. 역사적 기록의 해시만 새 값으로 바꾸거나 원래 봉인 파일을 수정하지 않는다. 새 기록은 `--build-record`로 전달한다.

## 2. 동결 후보를 준비한다

게시된 `memory-factorization/*-factor-post-eq.json`을 그대로 사용한다. 같은 필터 적합을 반복할 필요가 없다. **새 archive의 `fit/`을 재사용하면** `NAM_FIT`을 해당 복원 경로로 지정하고 아래 적합 명령을 생략한다.

head 적합 자체를 재현할 필요가 있을 때만 새 출력 디렉터리에서 실행한다.

```bash
"$NAM_PY" "$NAM_EXP/head_refit.py" --data "$NAM_DATA" --models "$NAM_MODELS" \
  --eq-jobs "$NAM_EXP/memory-factorization" --out "$NAM_FIT"
```

완료된 `selection.json`이 있어야 평가한다. 이는 TRAIN으로 30개 head를 적합하고 validation에서 선택하는 실험이며 backbone 업데이트는 없다. 컴파일러·수치 라이브러리가 다른 재현에서 해시나 선택이 달라질 수 있으므로 원래 결과에 맞추도록 후보를 바꾸지 않는다.

## 3. 역사적 세 arm 평가 → 누락 후보 보충

```bash
"$NAM_PY" "$NAM_EXP/evaluate_candidates.py" \
  --data "$NAM_DATA" --experiment "$NAM_FIT" \
  --renderer "$NAM_RENDERER" --tool "$NAM_TOOL" --build-record "$NAM_BUILD_RECORD" \
  --arm "baseline=$NAM_FIT/baseline" --arm "raw=$NAM_FIT/best-esr-raw" \
  --arm "eq-chain=$NAM_FIT/best-esr-eq-chain" \
  --old-test "$NAM_OLD_TEST" --seed 2610122201 --out "$NAM_EVAL"
"$NAM_PY" "$NAM_EXP/supplement_selected.py" \
  --evaluation "$NAM_EVAL" --experiment "$NAM_FIT" --tool "$NAM_TOOL" --out "$NAM_SUPPLEMENT"
```

보충 도구는 원래 비교의 `complete=true`와 동일 선택 봉인을 요구한다. 선택 네 채널을 정확한 모델+job 해시로 재사용하고, 첫 실행에서 빠진 **사전 선택 Nidhoggr raw λ=0.01 하나만** 같은 입력·참조로 측정한다. 다른 누락 집합이면 중단한다. 보충은 별도 독립 테스트나 테스트 이후 후보 선택이 아니다.

**정상적인 다음 실행에서는** 첫 evaluator의 arm 목록에 `--arm "selected=$NAM_FIT/selected"`를 추가해 사전 선택 후보를 처음부터 포함할 수 있다. 결과는 `comparison.json`의 `arms.selected`에 직접 있다. 이 경우 역사적 단일 누락 전용 보충 도구는 실행하지 않는다.

seed `2610122201`도 이제 노출된 데이터이므로 같은 실행은 재현/회귀다. 다음 개발에서는 가설·후보·예산·선택 규칙과 아직 노출되지 않은 시드를 입력 생성 전에 별도로 고정한다. 현재 `head_refit.py`는 역사적 시드를 계획에 고정하므로 evaluator의 `--seed`만 바꿔 우회할 수 없다.

## 4. 저장된 결과만으로 집계·그림 생성

`summarize_results.py`는 자신과 같은 디렉터리의 두 고정 파일명을 읽는다. 아래는 역사적 세 arm+보충 결과 또는 archive에서 복원한 해당 결과에 적용한다.

```bash
mkdir "$NAM_WORK/summary"
cp "$NAM_EXP/summarize_results.py" "$NAM_WORK/summary/summarize_results.py"
cp "$NAM_EVAL/comparison.json" "$NAM_WORK/summary/independent-comparison.json"
cp "$NAM_SUPPLEMENT/selected-summary.json" "$NAM_WORK/summary/selected-summary.json"
"$NAM_PY" "$NAM_WORK/summary/summarize_results.py"
```

`results-summary.json`, `results-figure.png`, `character-bands.png`를 만들며 학습·선택·렌더는 하지 않는다. 미래의 네 arm 비교는 `arms.selected`를 사용해 집계하고 역사적 보충 결과로 표시하지 않는다. 원본의 절대 scratch 경로는 실행 당시 출처다. 읽기 전용 결과 확인에는 archive의 해시로 복원 파일을 연결하며, 실행 검사를 우회하려고 봉인된 경로를 치환하지 않는다.

## 추가 진단과 판정 범위

필터 생성·메모리 진단 자체를 조사할 때는 [memory_factorization.py](memory_factorization.py), [memory_eq_parity.py](memory_eq_parity.py), [nonlinear_memory_probe.py](nonlinear_memory_probe.py), [nonlinear_memory_summary.py](nonlinear_memory_summary.py)의 명시적 CLI를 사용해 별도 디렉터리에 기록한다. 기본 재현에 이 완료된 진단을 다시 실행할 필요는 없다. archive는 입력·50개 차분 곡선·출력 해시·요약을 보존하지만 150개 조건의 전체 원래 native trace를 모두 보존하지는 않는다.

`complete=true`는 측정 완료다. 고정 source-output anchor와 원래 무음·레벨·ESR 임계값을 유지하며 현재 다섯 채널의 전체 정확도는 실패다. 모델·EQ·프리셋을 함께 관리한다. `.nam`만으로 외부 POST 보정이 실행되지 않는다. 실제 `NamEngine`/`BlockEq`/`PresetFile` 콘솔 검증은 전체 `ProcessorModelLoader`, GUI/DAW, 실시간 또는 실제 악기 청음 승인이 아니다.

IR은 **소스만 확인한 다음 단계**다. 현재 `render-preset`은 단일 NAM+EQ를 검사한다. IR cap/trim, `cab`/`space` 태그와 길이 fallback, pad/mix, 에너지 정규화·시간 정렬을 실제 IR 로더·컨볼버로 먼저 검증해야 한다. 현재 도구의 성공을 A2+IR 검증으로 확장하지 않는다.
