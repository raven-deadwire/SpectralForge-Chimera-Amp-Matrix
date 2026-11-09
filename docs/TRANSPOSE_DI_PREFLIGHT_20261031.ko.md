# Transpose v3 DI 사전 점검 — 2026-10-31 KST

대상은 11월 2일 녹음용 **corr40-split-v3 연구 환경**이다. 기준 소스는 main
`ad622ceffdd7404b12de4e473349b1104200048f`이며, 이 변경은 입력 검사·재현 기록·비교 보고를 보강한다.
제품 `Source/`, 연구 DSP, T1–T4 수식과 `Validation/transpose-latency-targets.json`은 변경하지 않는다.

**실제 DI 입력 0/9. 녹음·음질·장비·실제 DAW 검증은 완료되지 않았다.**
템플릿의 `TEMPLATE_NO_RECORDING`, WAV/라벨 null, `labels_reviewed: false`를 유지한다.
자동 검사는 Linux 작업 환경에서만 실행하며 사용자의 Windows 장비 준비 여부를 추정하지 않는다.

## 1. 이번에 실행한 검사

최종 실행 결과와 소스/도구 해시는
[`evidence/transpose-di-preflight-20261031/`](evidence/transpose-di-preflight-20261031/)에 기록한다.
합성 파일과 가상 청취자는 실연·사람의 청감·제품 승인 증거가 아니다.

| 검사 | 결과·범위 |
|---|---|
| 입력 QC 회귀 | 27개 합성 시험 통과. PCM16 음수 풀스케일, PCM24/32, float 비유한값, 빈 WAV, 채널·표본률·레벨·DC·경계·뮤트·라벨·상대 경로·9테이크·해시 보존·호스트 실패 처리 |
| DI selftest | 세 저음 각각 정답 61/61, 수정 전후 라벨러와 T1–T4 결과 일치. T1 5.583 ms, T2 짧은 음 6.280 / 긴 음 9.338 ms, T4 오검출·누락 0 |
| 청취 파이프라인 selftest | ABX 72회, 쌍 비교 36회, 가상 청취자 채점 PASS |
| 연구 probe | JUCE 8.0.8 고정 커밋으로 GCC 및 CMake probe 빌드. 두 실행 파일 selftest의 T1–T4 일치, SHA-256 기록 |
| VST3/헤드리스 호스트 | Linux 직접 실행 144/144 통과, 엔진별 72/72에서 11개 필수 검사 통과. 큰 블록 비교 432개 차이 0. xvfb 래퍼 시도는 144개 비정상 종료로 별도 보존. 실제 Windows/macOS DAW 수용과 구별 |
| 블록 분할 진단 | 3저음 × 3표본률의 긴 음, v3 −2반음: 9실행/117비교에서 차이 0. 별도 CorrelationTests 324그룹 PASS |
| 과거 PDC 재집계 | 기존 합성 원자료와 요약 일치. 44.1 kHz 잔차 최대 53.237 ms 실패 기록 보존 |
| 미입력 템플릿 | 9개 WAV 미입력 → `MISSING_INPUT`, `--strict` 종료 코드 1이 정상 |

## 2. 빌드 및 자가시험 명령

Python 3, NumPy, SciPy가 필요하다. 이번 실행 버전은 증거의 환경 기록을 따른다.
JUCE는 태그 이름뿐 아니라 아래 커밋을 확인한다. 네트워크 다운로드는 녹음 전에 완료한다.

```sh
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git ../juce-8.0.8
git -C ../juce-8.0.8 rev-parse HEAD
# d6181bde38d858c283c3b7bf699ce6340c050b5d
python -c "import sys, numpy, scipy; print(sys.version); print(numpy.__version__, scipy.__version__)"
git rev-parse HEAD
```

`build.py`는 g++/C++20 및 Linux 링크 옵션을 사용하는 기존 경로다. 플러그인을 빌드하지 않는다.
`--out`은 필수다. 현재 GCC 빌드의 명령·컴파일러·JUCE·소스 해시는 `build.json`에 남는다.

```sh
python Tools/TransposeNext/build.py --juce ../juce-8.0.8 --out build/transpose-next
python Tools/TransposeNext/di_validation.py selftest --renderer build/transpose-next/TransposeProbe --out build/di-preflight/selftest
python Tools/TransposeNext/t6_listening.py selftest --renderer build/transpose-next/TransposeProbe --out build/di-preflight/listening
python -m unittest discover -s Tools/TransposeNext -p test_di_preflight.py -v
```

연구 VST3와 호스트는 CMake 3.22 이상, C++20 컴파일러, JUCE의 플랫폼별 빌드 의존성이 필요하다.
Linux는 ALSA/X11/Freetype/Fontconfig/GL 개발 패키지와 헤드리스 실행용 `xvfb-run`을 사용한다.
Windows는 Visual Studio C++ 빌드 도구, macOS는 Xcode 도구가 필요하다. AU는 macOS에서만 빌드된다.
이번 변경은 같은 CMake 프로젝트에 `TransposeProbe` 타깃도 제공하므로 Windows에서 Linux용 g++ 링크 옵션을 쓸 필요가 없다.
Windows/macOS 빌드와 실제 DAW 실행은 해당 기계에서 추가 확인해야 한다.

```sh
cmake -S Tools/TransposeNext/plugin -B build/transpose-plugin -DJUCE_DIR=/absolute/path/to/juce-8.0.8 -DCMAKE_BUILD_TYPE=Release
cmake --build build/transpose-plugin --config Release --parallel 2
```

CMake probe 경로는 `build/transpose-plugin/TransposeProbe_artefacts/Release/TransposeProbe`이며
Windows는 `.exe`를 붙인다. GCC probe 대신 이 실행 파일을 사용하면 해당 **실행 파일의 SHA와 자가시험**을 다시 기록한다.

```sh
python Tools/TransposeNext/t6_host_check.py --plugin "build/transpose-plugin/ChimeraTransposeResearch_artefacts/Release/VST3/Chimera Transpose Research.vst3" --host-check build/transpose-plugin/TransposeResearchHostCheck_artefacts/Release/TransposeResearchHostCheck --out build/di-preflight/host --engines baseline,v3 --rates 44100,48000,96000 --shifts=-2,-1,1,2
```

이번 Linux 작업 환경에서는 `xvfb-run -a true`조차 종료 코드 1을 반환하는 래퍼 문제가 있어, 같은 호스트를 직접 실행해 정상 종료를 확인한 뒤 위 명령에 기존 `--no-xvfb`를 추가했다. 래퍼 시도의 실패 기록도 보존했다. 다른 환경에서 무조건 디스플레이 의존성을 무시하라는 뜻은 아니다.

Windows 호스트 실행 파일에도 `.exe`를 붙인다. 총 144회(표본률 3 × 입력 6 × 엔진 2 × 반음 4).
호스트 오류·검사 실패는 JSON과 비정상 종료 코드에 남긴다. v4는 이번 범위에서 제외한다.
연구 플러그인에서 `Engine=corr40-split-v3`, `Low upshift floor (v4)=Off`를 유지한다.
`Latency report=Fixed PDC`는 110/120/240샘플을 보고하며, `Live`는 0을 보고한다.
이는 실제 어택·저역 지연이 2.5 ms라는 뜻이 아니다.

## 3. 녹음 직후 입력 검사

저장소 밖의 개인 작업 폴더에 템플릿을 복사한다. 아래 `D:/Chimera-DI/20261102`는 **예시 경로**이며,
존재하거나 녹음이 들어 있다고 가정하지 않는다. 경로에 공백이 있으면 따옴표로 감싼다.

```text
D:/Chimera-DI/20261102/takes.json
D:/Chimera-DI/20261102/audio/<take-id>.wav
D:/Chimera-DI/20261102/labels/<take-id>-labels.csv
```

`wav: "audio/guitar-A1-44k1.wav"`처럼 상대 경로는 **takes.json의 폴더 기준**이다.
원본 WAV를 이동·자르기·노멀라이즈·재표본화하지 않는다. 세 표본률을 인터페이스/DAW에서 각각 설정하고
별도 연주로 녹음한다. WAV 헤더는 파일 표본률만 입증하며 네이티브 녹음 여부를 입증하지 못한다.
`take_note.capture_rate_hz`는 실제 세션 표본률, `native_recording_confirmed`는 녹음자가 확인한 후에만 true다.
같은 WAV 재사용은 해시 중복으로 거부한다. 다른 표본률로 재표본화한 사본 여부는 사람의 세션 확인이 필수다.

| 파일 ID 접두사 | 저음 기준 | steady 5프렛 기준 | 표본률별 ID 접미사 |
|---|---:|---:|---|
| guitar-A1 | A1 55.0 Hz | 73.42 Hz | 44k1 / 48k / 96k |
| bass-B0 | B0 30.87 Hz | 41.2 Hz | 44k1 / 48k / 96k |
| bass-A0 | A0 27.5 Hz | 36.71 Hz | 44k1 / 48k / 96k |

실제 악기·픽업/노브·줄/게이지·스케일·연주 방식·연주자·일시·드라이 DI 경로·인터페이스/게인·DAW/드라이버/버퍼·WAV 형식·BPM을 `take_note`에 기록한다.
앰프/CAB/게이트/컴프/EQ/피치/노멀라이즈를 끄고 테이크 내 입력 게인을 고정한다.
모노 PCM16/24/32 또는 float32 WAV만 받는다. 목표 peak는 −1 dBFS 이하, 필수 조건은 −40 dBFS 이상이며 클리핑이 없어야 한다.

실제 녹음 이후에만 manifest 상태를 `RECORDED_DI`로 바꾼다. 7개 구간의 시작·끝은 원본 WAV 기준 초 단위다.
슬라이스는 `[round(start_s × rate), round(end_s × rate))`로 처리한다.
겹침·역순·파일 범위 밖 경계·20초 초과 riff·내용 없는 구간은 수정 후 다시 검사한다.

각 구간에 앞뒤 **0.5초 이상**의 실제 뮤트를 확보한다. `silence_before_end_s`와
`silence_after_start_s`를 파형/청취로 확인해 채우면 도구가 길이와 전체 뮤트 RMS를 검사한다.
기존의 처음/끝 0.2초 RMS 검사도 유지한다. 기준은 구간 peak보다 최소 40 dB 낮음이다.
수치를 통과해도 구간의 연주 요구(steady 5초·vibrato 4초 등)는 사람이 확인한다.

```sh
# 라벨이 없어도 녹음 QC부터 실행: 모든 take를 검사하고 문제를 한꺼번에 기록
python Tools/TransposeNext/di_validation.py plan --manifest D:/Chimera-DI/20261102/takes.json --audio-only --strict --out D:/Chimera-DI/20261102/input-qc.json

# 테이크별 후보 라벨 생성. 아래 명령을 실제 9개 take-id에 각각 적용
python Tools/TransposeNext/di_validation.py label --manifest D:/Chimera-DI/20261102/takes.json --take guitar-A1-44k1 --out D:/Chimera-DI/20261102/labels/guitar-A1-44k1-labels.csv

# 엔진 출력 없이 모든 후보를 검토/수정한 후 labels 경로와 검토자/일시를 기록하고 labels_reviewed=true
python Tools/TransposeNext/di_validation.py plan --manifest D:/Chimera-DI/20261102/takes.json --strict --out D:/Chimera-DI/20261102/reviewed-qc.json
```

`--audio-only` 성공은 `AUDIO_INPUT_READY_LABELS_NOT_CHECKED`다. full plan의 `READY_TO_FREEZE`도
입력 준비 상태일 뿐 T1–T4 PASS가 아니다. `--strict` 없이 plan을 실행하면 기존 호환성 때문에 오류가 있어도
종료 코드 0일 수 있으므로 JSON 상태를 확인한다. `--strict`는 문제가 있으면 1이다.

QC JSON에는 원본 WAV SHA-256, 실제 표본률/비트 깊이/채널/프레임/길이, peak/RMS dBFS,
풀스케일·비유한 샘플 수, quiet DC, 구간 샘플 경계, 뮤트 RMS와 검토 라벨 해시가 남는다.
DC는 기존 방식대로 **가장 조용한 10%의 50 ms 블록 평균**을 사용한다. 한 테이크의 오류가 다른 테이크 검사를 중단시키지 않는다.
QC 보고서를 공개할 때는 로컬 경로·연주자 등 개인 정보가 포함되는지 확인한다.

## 4. 사전등록과 비교 기준 보존

11월 3일 라벨 검토 후 `freeze`, 사전등록 commit/push 완료 후 11월 4일 `run` 순서를 유지한다.
아래 출력 디렉터리는 실행 전에 준비한다. 실제 입력 없이 이 명령을 실행해 결과를 만들지 않는다.

```sh
python Tools/TransposeNext/di_validation.py freeze --manifest D:/Chimera-DI/20261102/takes.json --renderer build/transpose-next/TransposeProbe --engine corr40-split-v3 --out docs/evidence/transpose-t6-di-20261103/preregistration.json
# preregistration.json을 검토하고 commit/push한 뒤, 엔진마다 한 번만 실행
python Tools/TransposeNext/di_validation.py run --manifest D:/Chimera-DI/20261102/takes.json --renderer build/transpose-next/TransposeProbe --engine corr40-split-v3 --prereg docs/evidence/transpose-t6-di-20261103/preregistration.json --out D:/Chimera-DI/20261102/result-v3
```

사전등록은 원본 manifest(모든 메타데이터 포함), WAV, 라벨, renderer, 기준 JSON, DI 프로토콜,
검사 스크립트에 더해 **가져오는 측정 모듈과 PDC 계약 문서**의 해시, Python/NumPy/SciPy 버전까지 고정한다.
이 버전의 도구는 이전 버전의 사전등록과 일치하지 않으므로 **결과 확인 전** 새 사전등록이 필요하다.
기존 사전등록 파일/결과 디렉터리를 덮어쓰지 않는다. 다른 디렉터리를 사용해 재실행하는 행위까지
도구가 막지는 않으므로 엔진당 1회 규칙은 실행 기록으로 관리한다. 실패한 실행도 삭제하지 않는다.

- T1: 1–8 kHz, 2 ms RMS 어택 추가 지연 ≤10 ms, 이벤트 SNR ≥10 dB. 약한 이벤트는 별도 보고, 검출은 T4에서 판정.
- T2: 120 Hz 저역 무게중심 추가 지연 ≤10 ms(다운시프트).
- T3: 업시프트 저역 지연은 실제 DI에서 **참고 보고**. B0 15.5 ms/A0 19 ms 등 합성 비회귀 상한을 실제 DI 합격선으로 쓰지 않음.
- T4: 기존 라벨 −2/+10 ms 매칭, 정상음 음정/워블, 저역 재설정 규칙 유지.
- 측정 조건: 원본 표본률, block 128, −2/−1/+1/+2반음.

`results.json`은 기존 raw 지연/SNR 배열에 원본 WAV 기준 라벨 샘플 위치·구간 샘플 경계·원본 메타데이터를 추가한다.
`report.comparison`은 표본률별 및 **테이크/구간/반음별** 추가 보고다. 44.1 kHz와 B0/A0 long을 각각 추적할 수 있다.
어택은 전체 유효 이벤트/SNR≥10/약한 이벤트를 분리하며 최대·절댓값 최대·p5·p95를 함께 남긴다.
저역은 원래 추가 지연과 보정 후 잔차를 모두 보존한다. 합격 판정은 추가하지 않는다.

고정 L=2.5 ms 기준 잔차와 실제 정수 보고 샘플 기준 잔차를 나란히 출력한다.
44.1 kHz에서는 110샘플=2.494331 ms, 48/96 kHz에서는 120/240샘플=2.5 ms다.
과거 합성 PDC는 입력 전체 어택 유효성을, DI는 이벤트별 유효성을 사용하므로 두 모집단을 동일한 것으로 합산하지 않는다.

44.1 kHz의 과거 **53.24 ms FAIL**을 소급해 바꾸지 않는다. 이후 SNR≥10 dB 새 시드 합성 검증이 통과했다는
기록도 실제 DI 검증의 대체물이 아니다. 이번 보고는 약한 어택까지 남기므로 재발 위치를 추적할 수 있다.
B0/A0의 긴 음은 `long`, 각 음의 저역 배열, ±반음, 표본률을 함께 비교한다. 저역에 고정 PDC만 적용해 정렬됐다고 주장하지 않는다.

## 5. 녹음 당일에 필요한 입력과 미완료 항목

1. 9개의 독립 네이티브 모노 WAV와 실제 장비/세션 메타데이터. 세 표본률 모두 인터페이스·드라이버 설정 확인.
2. 각 7구간의 실제 경계와 앞뒤 뮤트 경계, 요구 연주 길이·음수 확인. 문제가 있는 take는 사전등록 전에 재녹음.
3. 엔진 출력을 보지 않은 검토자의 라벨 CSV, 검토자/일시, `labels_reviewed=true`.
4. 실행할 연구 플러그인/probe의 빌드 SHA와 OS·DAW·드라이버·버퍼 기록.
5. D01–D13 실제 DAW 점검, D06 실제 연주 지연 체감, D12 96 kHz/64샘플 CPU, D05/D13 PDC·바이패스 확인.
6. 실제 청취자 3명 이상과 블라인드 청취. macOS 실기기가 없으면 해당 칸은 미실행으로 유지.

자동 자가시험 통과는 이 여섯 항목의 완료를 뜻하지 않는다.
