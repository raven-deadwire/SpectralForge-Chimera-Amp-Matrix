# PR #231 병합 후 main 통합 검토 및 승인 대기 패치

확인일: 2026-11-03 KST. 아래 검토·로컬 시험은 외부 쓰기 전에 작성한 기록이다. 이후 사용자가 후속 Draft PR 생성과 새 HEAD CI 진행을 승인했다. 승인 후에도 main은 `0b247dc`이고 #250은 Open Draft임을 다시 확인했다. 새 CI 결과는 PR의 해당 HEAD와 연결해 별도로 기록한다. PR 병합·릴리스는 승인 범위에 포함되지 않는다.

## 판정

검토 시작 시 PR #231은 Open / mergeable:true였으나, 작업 도중 **2026-11-03 09:51:59 KST에 외부에서 main으로 병합**됐다(merge `9d342ad`). 이어 #241도 병합되어 최종 기준 main은 `0b247dc`다. 이 검토에서 외부 쓰기는 수행하지 않았다. 오래된 충돌을 해결하거나 닫힌 #231을 갱신하는 작업은 더 이상 필요하지 않다.

Phase 7 적용 중 편집 보호의 빈틈과 별도 PR #250의 CAB 보호를 합친 **main 기준 후속 패치**를 준비했다. 이 문서 작성 시 새 작업 트리는 전체 빌드·회귀 미검증이다. 기존 #231 HEAD의 녹색 CI를 새 main이나 이 패치의 통과로 승계하지 않는다. 사용자 승인에 따라 Draft PR과 새 HEAD 검증을 진행하며, 병합 판단은 그 결과 확인 후다.

## 정확한 기준

| 구분 | SHA / 상태 |
|---|---|
| PR #231 | HEAD `98b172a0ab3eff3dac6e63b49cf075736993d795`, 최종 조회 Closed / merged, merge `9d342ad6126137c28d293f7746a4845db059e517` |
| 최종 main 직접 git 조회 | `0b247dc1ea6278fa966197cb1cf06c9ac6cc07a2` (#231 및 #241 포함) |
| 최초 main 직접 git 조회 | `6fca18392cbbe8b8463a8f20a20d61ea7d41ad8d` |
| 최초 PR HEAD와 main의 공통 조상 | `58e193434ffe48dac8c96005e3479e84a1cd1ed4` |
| 최초 main 병합 결과 tree (이력) | `770ba4d2aeb5706dbc732d2853023c4c414f1049` |
| 과거 본문 로컬 검증 | `cca1084bcae562adbe2e0e25e43d02ffe24a9abc` / 파일 해시 기반 기록 |
| 참고 PR #250 | `85c34bc0df6636f4bda11c9dbb8feaa9e254871d`, Open Draft, main 미포함 |

최초 main `6fca183`와 PR HEAD를 로컬 병합했을 때 충돌은 없었고 #251의 OBS 관련 4개 파일만 추가됐다. 최종 재확인에서 #231과 #241 병합을 확인하여 그 main 변경도 로컬에 반영했다. 최종 main은 #231 HEAD의 후손이며 두 Auto Set 파일 및 공유 편집 추적기·Phase 7 시험은 #231 HEAD와 동일하다. 배포되는 패치는 최종 main `0b247dc`에 대한 5개 파일의 추가 변경만 담는다. #241의 테스터·빌드 수정은 기준 main으로 보존되며 후속 패치에서 다시 수정하지 않는다.

## 두 충돌 파일의 최종 구성

| 파일 | 유지한 내용 / 이번 변경 |
|---|---|
| `Source/AutoSet.h` | 최종 main 및 PR HEAD와 바이트 동일. MASTER L/R 독립 캡처, 별도 AMP ring, CAB/tone 등 기존 mono sample tap, BS.1770 호출, 실제 새 Final EQ 렌더와 출력 제한을 함께 유지한다. 최종 후속 패치에서 이 파일을 다시 수정하지 않는다. |
| `Source/AutoSetController.h` | main의 Phase 8–12 순서·잠금·캡처와 PR의 Phase 7 계산을 유지한다. #250의 `finishCab()`를 동일하게 이식하고 Phase 7 적용 완료 검사와 롤백 범위를 보강한다. |

BS.1770의 채널별 K-weighting 에너지 합산, high-pass 분자 `{1,-2,1}`, -18 LUFS 목표, Final EQ ±3 dB, per-run trim ±12 dB, 출력 범위, -1 dBFS sample-peak 우선 제한, strength/deadband 이후 보수적 감쇠 반올림은 변경하지 않았다. 파라미터 ID·범위·직렬화도 변경하지 않았다.

## 새로 확인한 코드 문제와 수정안

### 1. 마지막 Output 쓰기 콜백 이후 검사 누락

기존 `finishLevel()`은 Final EQ 쓰기 후 `intact`를 검사하고, 그 뒤 `set("output", ...)`을 호출한 다음 곧바로 예측 LUFS·peak와 성공 결과를 작성한다. Output의 begin/value/end 콜백에서 호스트가 Final EQ나 Dynamic EQ를 바꾸면 렌더에 쓴 필터와 현재 필터가 달라져도 기존 예측을 보고할 수 있다. 마지막 Output 쓰기에서 소유권을 잃어도 앞서 배치한 EQ의 일괄 롤백에 도달하지 않는다.

수정안은 각 쓰기 전과 Output 쓰기 직후, 최종 결과 작성 전까지 같은 편집 검사를 유지한다. 중간 개입 시 이 단계가 소유한 Final EQ와 Output 변경만 직전 상태로 복구하고, 사용자·호스트가 만진 필드는 남긴다. 무효화된 예측은 `lastLevel`에 기록하지 않는다. 성공한 뒤에만 Final EQ의 UNDO 그룹을 등록한다.

### 2. 잠긴 읽기 의존성의 적용 중 검사 누락

`snapshot`에는 의도적으로 잠기지 않은 쓰기 대상만 들어간다. 그러나 기존 적용 후 검사는 이 `snapshot`만 순회한다. 따라서 Output 잠금 상태에서 EQ 쓰기의 콜백이 Output을 편집하거나, Final EQ 잠금 상태에서 Output 쓰기의 콜백이 Final EQ를 편집하는 경우, 또는 잠긴 Dynamic EQ 슬롯이 변경되는 경우를 놓칠 수 있다. Auto Set의 잠금은 호스트 편집을 차단하는 기능이 아니다.

수정안은 잠긴 항목도 포함하는 `levelReadSnapshot`의 편집 revision·gesture를 적용 종료까지 검사한다. 기존 쓰기 snapshot의 값·소유권 검사도 유지한다. Auto Set 자체 알림은 기존 `ParameterEdits`가 revision에서 제외하므로 정상 배치는 허용하고 A→B→A와 same-value 호스트 알림은 무효화한다.

두 문제는 **코드 경로 검토로 확인**했으며 새 54개 컨트롤러 회귀의 런타임 재현·통과는 아직 주장하지 않는다.

### 3. Phase 9 부분 배치 보호 결합

최종 main과 #231 HEAD에는 #250의 보호가 없다. POSITION·DISTANCE·GAIN의 소유권을 한 단위로 검사하고 개입 시 Auto Set 소유 부분만 복구하는 `finishCab()` 및 `Tests/AutoSetCabEditTests.h`를 #250에서 가져왔다. 두 본문은 #250의 고정 SHA와 바이트 동일함을 검사했다. #250의 별도 Dynamics UI 타이밍 수정과 문서는 이 패치에 포함하지 않았다.

## 잠금·소유권·UNDO 검토

- 공통 `Source/AutoSetParameterEdits.h`는 main 및 PR HEAD와 바이트 동일하다. 전역 writing 플래그나 편집 revision 초기화를 추가하지 않았다.
- AMP/BOOST/POWER/SPACE 분석과 실행 순서, 기타 단계의 `set()`/`undo()`/잠금 매핑은 유지했다.
- Phase 7 롤백은 `finalEQ_*` 및 `output`만 대상으로 하고, 단계 시작 전 snapshot 값까지 복구한다. Dynamic EQ나 앞 단계 결과를 되감지 않는다.
- 각 복구 직전과 write 내부에서 소유권을 확인한다. 호스트가 적용 또는 복구 콜백에서 만진 값을 다시 소유하지 않는다.
- 성공 시 기존 Final EQ 그룹 UNDO가 유지되고, 중단 시 새 그룹을 등록하지 않는다. 기존 사용자의 EQ·Dynamic EQ 예약 슬롯을 유지하는 선택 규칙도 그대로다.
- 소스 동일성 검사는 런타임 회귀 통과가 아니다. 비동기 호스트 자동화, 실시간 처리와 편집의 경합, live smoothing과 실제 DAW UNDO는 추가 검증 대상이다.

## CI·과거 기록·이번 실행의 구분

| 근거 | 대상 | 확인 결과 |
|---|---|---|
| PR 본문·`REVIEW_FOLLOWUP.md` | `cca1084`에 대응하는 과거 파일 해시 | 당시 로컬 4/4, reference 144/144 기록. 현재 작업 트리 결과로 사용하지 않음 |
| Auto Set CI 41415540052 | checkout 로그가 `98b172a`와 일치 | Linux 4/4, Windows 2/2, macOS 2/2 PASS |
| 같은 CI Linux | `98b172a` | AutoSet 226.41 s, parameter safety 0.05 s, loudness reference 3.02 s, reference ownership 0.00 s |
| 같은 CI Windows | `98b172a` | AutoSet 251.82 s, parameter safety 0.09 s |
| 같은 CI macOS | `98b172a` | AutoSet 348.66 s, parameter safety 0.13 s |
| Head controls CI 41415540114 | `98b172a`, Ubuntu | Packaging + AmpHeadControl 2/2 PASS; head test 166.26 s |
| Signal-path CI 41415540060 | `98b172a`, Ubuntu | 17/17 PASS; CabPanelState 87.08 s |
| Open Beta 41415540097 | HEAD에 연결된 작업 조회 | 플랫폼 작업 success. 전체 로그를 모두 재검토한 것은 아니며 배포 승인으로 해석하지 않음 |
| 이번 로컬 libebur128 비교 | 후보와 바이트 동일한 `AutoSetLoudness.h` | GCC 13.3 직접 빌드. libebur128 1.2.6, 144/144 PASS, 최대 오차 0.00000133191 LU. CSV가 기존 reference CSV와 바이트 동일 |
| 이번 로컬 reference 수명 검사 | 실제 libebur128 객체 | 성공/init/add/measure 실패 4개 PASS |
| 이번 로컬 Capture 검사 | `AutoSet.h`의 실제 Capture 정의 추출 | 44.1/48/96 kHz, 150,000회 순환: AMP/CAB 분리, AMP+MASTER stereo, mono right reset, 물리 ring wrap PASS |
| 새 후보의 전체 AutoSet·UI·3개 OS | main `0b247dc`+#250 CAB+새 적용 보호 | **미실행 / 미검증** |

Windows/macOS CI에서는 libebur128 reference target이 OFF였다. 이 두 OS에서 reference 비교가 통과했다고 표시하지 않는다. 워크플로 전체 녹색도 skipped step이나 real DAW/청감 검증을 뜻하지 않는다.

이번 환경에는 CMake 및 JUCE 제품/UI 빌드에 필요한 시스템 개발 의존성이 준비돼 있지 않다. 패키지 갱신도 권한 오류로 실패했다. 이용 가능한 GCC로 JUCE에 의존하지 않는 reference 실행 파일과 실제 Capture 정의만 빌드했다. Capture 추출 시험은 전체 플러그인·오디오 스레드 동시 실행 시험이 아니다. 합성 신호 비교도 실제 DI나 청감 판정이 아니다.

## 과거 Windows·Ubuntu 실패의 현재 관련성

### Windows Auto Set

원래 HEAD `d531b81`의 job 124166756783에서 `ChimeraAutoSetTests`가 91.24초 후 SegFault로 끝난 로그를 확인했다. 이후 `d3543f6f7320093d60d21624747f413ea857da05`가 테스트의 `finalRender()` 내부 대형 `GraphicalEQ`를 스택에서 `unique_ptr` 힙 객체로 옮겼다. 커밋은 Linux 1 MB 스택 제한과 gdb에서 재현한 경위를 기록한다. 이번 검토에서는 그 gdb 재현을 다시 수행하지 않았다. 현재 소스의 힙 할당과 `98b172a` Windows 2/2 통과는 직접 확인했다. 과거 null 조회 수정만으로 이 크래시가 해결됐다고 귀속하지 않는다.

### Ubuntu native UI

원래 두 실패 모두 `Head paint adds, removes or distorts pixels ... [head model 1]`이었다. CAB의 IR/상태 시험 자체는 그 오류 이전에 통과한 로그가 있다. 후속 `0c84def3ca0b5a86625c7b20da341c3f0b84c394`는 GCC 15.2의 FMA contraction 때문에 동일 수식의 y 변환이 `c25fb9d2` / `c25fb9d0`로 갈리는 재현과 `-ffp-contract=off` 조치를 기록한다. 현재 HEAD에는 그 GCC 설정이 포함되고 두 해당 UI 시험도 통과했다. 픽셀 동일성 함수의 허용 오차를 완화하지 않았음을 diff로 확인했다. 이번 환경에서 GCC 15.2·native display 재현은 수행하지 않았다. Phase 7 BS.1770 계산 실패와 연결할 근거는 없다.

이 이력들은 새 후보의 플랫폼 통과를 대신하지 않는다.

## 새로 추가·연결한 회귀와 실행 조건

1. 새 `levelRegression::applyEditTransactions()` 54개: EQ enabled 또는 Output의 begin/value/end 콜백, Output/Final EQ 잠금 유무, 잠긴 Dynamic EQ target, Output 자체 재진입, 지속 편집·A→B→A·same-value 알림. 다른 모든 파라미터의 보존, 부분 쓰기 복구, stale 성공 결과 차단과 UNDO 후 동일 상태를 검사한다. **작성 완료, 전체 빌드/실행 전**.
2. #250 CAB 편집 92개: A/B mic, 네 strength, POSITION/DISTANCE/GAIN, held gesture, begin/value/end, rollback/UNDO. 기존 `--cab-regression`, 새 `--cab-edits`와 전체 실행에 연결했다. **이번 결합 트리에서는 미실행**.
3. 기존 full Auto Set: 모든 단계 lock/strength, Gate 2·Expander, Tone/Dynamic EQ ownership, 반복 실행·UNDO, Phase 7 30개 render·28개 보존, CAB IR·저음 베이스·Phase 11/12 회귀를 유지한다. 새 플래그 `--level-edits`는 기존 Final EQ 편집 검사와 새 54개를 모두 실행한다.
4. Linux reference 144개와 수명 4개를 전체 CTest에서 재실행하고 Windows/macOS controller CTest 및 Ubuntu Head/CAB UI를 새 SHA로 실행한다.
5. Windows 1 MB 스택 조건의 전체 Auto Set, 실제 DAW에서 잠금 상태의 자동화 및 UNDO, Phase 9→Tone/Dynamics→Phase 7의 실제 처리 경로, 모노/한쪽/역위상 스테레오와 44.1/48/96 kHz DI, live filter switching과 true-peak를 별도 검증한다.

권장 기본 빌드/시험:

```sh
cmake -S . -B build-level -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_UI_PROBES=ON -DCHIMERA_BETA_QUALIFICATION_ONLY=ON \
  -DCHIMERA_LOUDNESS_REFERENCE_TESTS=ON
cmake --build build-level --config Release \
  --target ChimeraAutoSetTests ChimeraAutoSetLoudnessReferenceTests --parallel 2
xvfb-run -a ctest --test-dir build-level -C Release \
  -R '^ChimeraAutoSet(Tests|ParameterSafetyTests|LoudnessReferenceTests|ReferenceOwnershipTests)$' \
  --output-on-failure --no-tests=error
```

Windows/macOS는 reference 의존성이 없다면 OFF로 구성하고 AutoSetTests/ParameterSafetyTests만 실행하되 reference는 미실행으로 기록한다. Ubuntu UI는 기존 `Tools/run_linux_ui_tests.py`와 `ChimeraAmpHeadControlTests`, `ChimeraCabPanelStateTests`를 사용한다. 빠른 진단에는 빌드된 Auto Set 실행 파일의 `--level-edits`, `--cab-edits`, `--dynamic-regression`, `--power-bass-controller`, `--space-regression`을 사용할 수 있다.

-1 dBFS는 캡처한 재료의 sample-peak 예측이다. -1 dBTP, 미래 입력, 실시간 전환·smoothing 안전 보증으로 확대하지 않는다. 잠금이나 trim 범위 때문에 목표를 달성하지 못하는 경로의 경고는 유지한다.

## 근거 링크

- [PR #231](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/231)
- [PR #250](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/250)
- [현재 HEAD Auto Set CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41415540052)
- [현재 Ubuntu head job](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41415540114/job/125178351251)
- [현재 Ubuntu signal-path job](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41415540060/job/125179471797)
- [과거 Windows SegFault](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41085868211/job/124166756783)
- [Windows 테스트 스택 수정](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/d3543f6f7320093d60d21624747f413ea857da05)
- [Ubuntu FMA 수정](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/0c84def3ca0b5a86625c7b20da341c3f0b84c394)

## 승인 후 외부 반영 범위

최종 main을 기준으로 새 후속 브랜치와 Draft PR을 만들고, CAB 보호·Phase 7 적용 보호·회귀·이 검토 문서의 총 5개 파일을 반영한다. 닫힌 #231의 브랜치/본문은 갱신하지 않는다. 새 PR 본문에는 역사적 로컬 결과, #231 HEAD CI, 새 후보 대기 상태를 분리해 적는다. 새 HEAD 결과 전에는 통과나 병합 가능 품질 판정을 기재하지 않는다. 승인 시점에 main이나 #250이 이동했으면 중복 반영 여부를 다시 확인한다. #250 브랜치 변경, PR 병합, 릴리스·배포는 이 승인 범위에 포함하지 않는다.
