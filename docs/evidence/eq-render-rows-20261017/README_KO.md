# EQ 렌더 비교 도구 rows 이름 충돌 수정 검증

검증일: 2026-10-17 KST. **Draft / release_approved=false / actual_daw=NOT_ASSESSED.**

## 소스와 변경 범위

- 원본 통합 PR: [#90](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/90), `428d4b9890a015f1fe1545ad737e8ed2c57af9d9`.
- 수정 PR: [#92](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/92).
- 실행 소스: `8b12476623afa8135af7ff8b7ad1d5e428207fff`.
- 실행 tree: `e1393282b7f559b4616d65d39d4a3da8026551ac`. 로컬 staged tree와 GitHub tree가 동일하다.
- 실행 소스는 고정하고, 이 보고서와 원시 검증 자료는 별도 evidence 브랜치에 기록한다. evidence 커밋 자체가 native CI를 통과했다고 주장하지 않는다.

`Tools/eq_normalization_render.py`의 `compare` 결과 목록 인자를 `rows`에서 `results`로 바꾸고 append 참조만 변경했다. 가져온 TSV reader `rows()`를 가리던 지역 이름을 제거하는 두 줄 수정이다. 그 외 변경은 기존 테스트 파일의 회귀 검사 추가와 3 OS CI의 선행 Python 검사/로그 보존뿐이다.

`Source/**`, `Tests/fixtures/**`, `Tools/eq_tuner_parameter_contract.py`, `Tools/eq_legacy_fixture.py`, CMake, 점수, 오디오·파라미터·상태·지연 비교 기준은 원본과 동일하다. `source-audit.json`에 변경 파일 SHA-256을 보존한다.

## 수정 전후 회귀 근거

| 검사 | 수정 전 | 수정 후 |
| --- | --- | --- |
| 새 compare 회귀 검사 포함 13개 test methods | 7개 methods에서 TypeError; metadata subtests 포함 10 errors | 13/13 PASS |
| 공개 beta.2 parameter append 계약 | 별도 도구 변경 없음 | 4/4 PASS |
| legacy evidence 검사 | 별도 도구 변경 없음 | 5/5 PASS |
| 고정 공개 beta.2 fixture 무결성 | fixture 변경 없음 | PASS |

기존 5개 `audio_equal` 검사에 실제 `compare` 경로 8개를 추가했다. 임시 파일에 실제 TSV·JUCE XML state·float32 audio를 기록하고 실제 reader와 비교기를 실행한다. mocking으로 이름 충돌을 우회하지 않는다.

- 같은 inventory의 A/A 비교 및 PASS evidence 기록.
- 정확한 59개 신규 파라미터 append와 wrapper index 이동을 양방향으로 검증.
- 오디오 1비트 차이, normalized 값 차이, 신규 파라미터의 비기본값을 거부.
- 중복 ParamID 및 steps/default/flags 변경을 거부.
- latency metadata와 serialized state 차이를 거부.
- 실패한 비교는 결과 목록에 PASS를 추가하지 않는지 확인.

`before-fix.log`, `after-fix.log`는 동일한 새 테스트를 수정 전후 도구에 실행한 결과다. 전자는 예상한 실패이며 후자의 실패를 숨기기 위한 expected-failure 표시는 없다.

실제 기존 Linux artifact `12049841619`(run `38906942295`)도 ZIP SHA-256 `c1f5ca338707897d171e9670811e1271488fdeeb5c456b3f7743fe973fa7ba16`로 검증한 후 다시 비교했다. 원본 도구는 `TypeError: 'list' object is not callable`, 수정 도구는 기존 `deleted candidate A/A`의 audio bits/normalized/state/latency 완전 일치를 확인했다. 이 결과는 **기존에 캡처된 1개 비교의 재평가**이며 신규 렌더 15개 전체의 결과로 취급하지 않는다. `real-capture-before-after.json` 참조.

재현 명령:

```sh
python3 Tests/test_eq_normalization_render.py -v
python3 Tests/test_eq_tuner_parameter_contract.py -v
python3 Tests/test_eq_legacy_fixture.py -v
python3 Tools/eq_legacy_fixture.py verify --output Tests/fixtures/eq-beta2
```

## 고정 소스 native CI

[EQ CI run 38940845177](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38940845177).

2026-10-17 02:55 KST 체크포인트:

| 플랫폼 | Python 도구 회귀 | EQ CTest | 실제 VST3 15개 렌더 비교 |
| --- | --- | --- | --- |
| Linux | 13/13 PASS | 8/8 PASS | **15/15 PASS** |
| Windows | 13/13 PASS | 8/8 PASS | 기존 workflow상 SKIPPED, 미검증 |
| macOS | 선행 13개 검사 step SUCCESS | native 빌드 진행 중, 이번 실행 최종 판정 없음 | Linux 전용 경로, 미검증 |

Linux [job 117107774395](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38940845177/job/117107774395)은 전체 SUCCESS이며 `active_eq_automation_effect=PASS`다. 아래 표의 각 칸은 오디오 bits, normalized 값, 지연, serialized state를 함께 검사한 결과다.

| phase | candidate A/A | beta.2 A/A | candidate → beta.2 | candidate → beta.2 → candidate | numeric automation |
| --- | --- | --- | --- | --- | --- |
| deleted | PASS | PASS | PASS | PASS | PASS |
| reused | PASS | PASS | PASS | PASS | PASS |
| automated | PASS | PASS | PASS | PASS | PASS |

**기존 TypeError는 도구 결함으로 재현·해소됐으며, 복구 후 이 15개 EQ 경로에서 실제 DSP 오디오 불일치는 검출되지 않았다.** 다른 sample rate/block size, CAB/amp 포함 경로, 실제 DAW 전체로 결과를 확장하지 않는다. macOS 기존 실패는 여전히 미해결로 유지한다. 다른 CI workflow들도 이 체크포인트에는 일부 진행 중이므로 전체 native CI PASS를 선언하지 않는다.

실제 오디오 비교 범위는 Classic의 두 EQ bank를 활성화하고 amp/cab 및 관련 외부 효과를 분리한 real VST3 host 경로다. 48 kHz / 256 samples / stereo, 비교당 32,768 float samples, midpoint parameter queues를 사용한다. deleted / reused / automated 각각 다음 5개 비교, 총 15개다.

1. candidate A/A
2. beta.2 A/A
3. candidate → beta.2
4. candidate → beta.2 → candidate
5. 공개 beta.2 numeric automation ID를 통한 candidate/beta.2 비교

오디오 bytes, 모든 기존 normalized 값, latency metadata, canonical serialized state가 정확히 일치해야 한다. 허용 오차를 추가하지 않았다. reused 상태의 automation이 실제 오디오를 바꾸는지도 기존 판정 그대로 검사한다.

Windows/macOS에서 실행하는 Python 회귀 검사는 도구 검증이다. 실제 VST3 host는 `CMakeLists.txt`의 Linux 조건, `dlopen` 및 `ModuleEntry`를 사용하는 Linux 전용 구현이며 고정 beta.2 digest도 Linux 바이너리다. 이 최소 수정에서 이를 이식하거나 다른 플랫폼의 15개 native 비교 PASS로 확장 해석하지 않는다.

## 유지하는 실패와 수용 범위

- 기존 EQ CI [38906942295](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906942295)의 macOS 결과는 **6/8**, `finalEQ_b10_q` stored/canonical 불일치와 후속 `eq-legacy/default/deleted.chimera` fixture 부재다. Python 이름 수정으로 해결된 것으로 취급하지 않는다.
- 기준 PR #90의 macOS 정밀 튜너 [job 116992764419](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941615/job/116992764419)는 3회 반복 중 44.1 kHz / 256 / 82.406889 Hz에서 acquisition 296.006042 ms로 실패했다.
- 같은 기준 소스의 전체 macOS [job 117004648714](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941848/job/117004648714)는 **63/66**이며 튜너, legacy state, normalization 3개가 실패했다. 이 snapshot에서는 CAB 관련 CTest가 통과했지만, 이전 PR의 CAB 실패 원인이나 실제 DAW 수용을 이 도구 수정으로 해결했다고 판단하지 않는다. 과거 실패와 이번 소스의 관찰 결과를 구분한다.
- 기존/신규 CI 성공 여부와 무관하게 실제 Studio One/Cubase/Cakewalk 세션, automation/Undo, 구버전 프로젝트, IR/A-B, 종료 수용은 여기서 수행하지 않았다.
- **도구 TypeError는 검증 인프라 결함이다. 실제 DSP 회귀 여부는 복구된 렌더 검사 결과로 별도 판단한다. 이 변경 자체는 제품 DSP를 수정하지 않는다.**
- 병합, 태그, 공개 릴리스, release_approved 변경 없음.

## 근거 파일

`baseline-ci-snapshot.json`, `baseline-linux-tail.log`, `baseline-macos-tail.log`, `baseline-job-*-results.log`는 기준 소스의 결과다. `*-tail.log`는 긴 build log의 끝부분, `*-results.log`는 관련 결과 줄의 발췌이며 전체 job log로 표시하지 않는다. 전체 원문과 artifact는 위 GitHub Actions에서 조회할 수 있다.

`linux-render-summary.json`은 CI의 원본 summary다. 21회 실제 VST3 렌더의 input state/output state/audio/binary SHA-256과 15개 비교의 결과를 포함한다. `linux-legacy-results.xml`은 원본 CTest JUnit, `linux-render-tool-regression.log`는 원본 Python 회귀 로그다. `linux-build-version.json`의 source_sha가 실행 소스 `8b124766...`와 일치함을 확인했다.

Linux artifact [12063643718](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38940845177/artifacts/12063643718)을 다운로드하여 ZIP SHA-256 `de06b90b3a843ac422824ddb98da496a8955135cf8e1b63302ce68e00706b52a`를 검증했고, 21개 audio/output-state 파일의 실제 SHA-256을 summary와 모두 대조했다. 원시 audio/TSV/state 파일은 해당 artifact에 포함된다.

| 실행 파일 | SHA-256 |
| --- | --- |
| VST3 host | `ef161dbfd660b984554ee00be8fe5a4ceba6e8dfd62c6acd76676a24c467d586` |
| 공개 beta.2 Linux VST3 | `a7be4d98614897b9dd98254f39ba919f5520fdde4649cc73e98a56d12feef486` |
| 수정 후보 Linux VST3 | `e360757f2dd1150f573bc7c165b18a24a96d68e02c9190f921b57ff9dc8a7e3d` |

`ci-final-checkpoint.json`은 macOS 선행 Python step SUCCESS와 native 작업 진행 상태, Linux/Windows job SUCCESS, 아직 진행 중인 다른 workflow를 구분해 보존한다. `linux-job-tail.log`, `windows-job-tail.log`, `windows-regression-excerpt.log`는 해당 job의 원문 발췌다. 실행 후 원본 PR #90의 head/Draft/미병합 상태와 수정 PR #92의 고정 head/Draft/미병합 상태도 재확인했다.
