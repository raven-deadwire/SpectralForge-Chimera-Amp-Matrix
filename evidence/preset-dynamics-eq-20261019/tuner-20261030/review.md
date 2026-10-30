# PR #222 튜너 테스트 대기시간·고정 해시 검토

검토일: 2026-10-30 KST. 대상: PR #205 `f0ba8e9c9f9eca16553802e50f612d53c2ef51e9`, PR #222 `9163ec6aa2d9591f592c6b4a8765b70bcc6f07bd`.

## 결정과 승인 범위

450/500 ms 복원 대신 #205의 **1200 ms 워커 정지 / 1500 ms 재개 관찰 제한**을 유지한다. 소유자의 2026-10-30 요청은 다음 조건부 수정 지시를 포함한다.

> 장시간 대기를 유지하는 편이 타당하다면 테스트 내용과 검증 규칙을 확인한 뒤 승인 절차에 맞게 수정해줘. 해시 고정 검사를 제거하거나 우회하지 말아줘.

이 지시에 따라 실제 변경, 원래 실패, 모든 assertion과 해시 계약을 검토한 뒤 아래 한 파일의 정확한 바이트만 재승인 범위로 기록한다. 승인 출처는 현재 소유자 요청이며, 별도의 GitHub APPROVE 리뷰를 받았다는 의미는 아니다. 과거 `49432103f2974d93cb760c39db5126979e3bb0bd`의 검토 기록 방식과 같이 고정값 옆에 근거를 연결했다.

- 파일: `Tests/PrecisionTunerIntegrationTests.cpp`
- 이전 SHA-256: `1371469c87ec73b0c2edd24d3daebc5342d026d2bdbdcf456be28581bc1ecd60`
- 이번 SHA-256: `8f71de6fc444877dd306d4c9d2483ed5262557d513fdb1c32a489e629298c7fc`
- #205의 파일을 바이트 단위로 재사용한다. CRLF도 보존한다. 줄바꿈 정규화로 해시를 맞추지 않는다.
- 제품 DSP, 250 ms stale 유효기간, 280 ms stale 관찰 대기, 250 ms 정지 진입 확인, 290 ms 획득/안정화 기준, 0.1 cent 정확도, FIFO/generation 처리와 모든 assertion은 그대로다.
- `verify_source_scope()`의 단일 SHA-256 비교와 실패 처리는 그대로다. 임의 변경 허용, 자동 재계산, 구/신 해시 동시 허용, skip을 추가하지 않는다.
- 병합·공개 릴리스·preset-only 전체 소스 승인으로 범위를 확대하지 않는다.

## 실제 변경과 원인

#205는 세 파일을 변경했다. UI 클릭의 비동기 반영 대기는 `NativeUITests.h`와 `EQAttributionDiagnostics.h`에 있으며, 튜너 파일은 두 시간값과 주석만 바뀌었다. #222의 원래 변경은 튜너 부분만 되돌렸다. UI의 다른 두 수정은 이번에도 보존한다.

`Source/GlobalDSP.h`의 test hook은 워커가 pause 요청을 읽은 뒤 `testWorkerPaused=true`로 설정하고 실제 시간으로 정지 종료 시각을 계산한다. 테스트 스레드는 이를 관찰한 뒤 1 또는 40블록을 생성·push하고 280 ms를 기다린다. 스레드들이 독립적으로 스케줄되므로 다음 조건이 필요하다.

`pause 상태 관찰 지연 + 입력 생성/push + 280 ms sleep 실제 경과 + 검사까지의 지연 < pause 기간`

450 ms에서는 명목상 170 ms만 여유가 있다. `sleep_for(280ms)`의 초과 지연이나 테스트 스레드의 선점이 이 여유를 소진하면 워커는 정상적으로 재개했는데도 `Worker resumed before stale observation check`가 발생한다. 40블록은 오디오 시간으로 pacing하지 않는 연속 push이며, 40 × 블록 길이를 고정 경과시간으로 가정하지 않는다.

재개 검사는 `completedWorkerIterationsForTest()`가 증가할 때까지 polling한다. 1500 ms는 워커의 실제 처리 성능 목표가 아니라 **남아 있는 인위적 pause + 스케줄링 + 첫 반복 완료**를 기다리는 상한이다. 1200 ms pause를 두면서 500 ms만 기다리면 통상 남은 pause 약 920 ms보다 먼저 제한에 도달할 수 있으므로 두 값은 함께 유지해야 한다. 기존 450/500에서도 과부하 시 재개 실패 가능성은 있지만, 아래 원본 로그에서 직접 확인된 실패는 조기 재개 assertion이다.

1200/1500 ms도 유한한 대기시간이므로 무제한 정지·선점을 해결하지 않는다. 극단적 runner stall이나 250 ms pause 진입 확인 실패는 여전히 가능하다. 후속으로 필요하면 명시적 pause/release handshake를 설계하되, 이는 test hook와 고정 파일의 별도 검토 범위다.

## 확인한 CI 원본

| 소스·실행 | 확인 결과 |
|---|---|
| #183 `369107be345732c428d57ba051e31553a279d3e6`, [run 40531695044](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40531695044), [macOS job 122285928713](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40531695044/job/122285928713), attempt 1 | `ChimeraPrecisionTunerIntegrationTests`가 45.47 s 후 `Worker resumed before stale observation check`로 실패. 이전 route summary는 failed=0, feeder_invalid=0. |
| 동일 run·동일 소스, macOS job 122457508668, attempt 2 | 성공. 재실행 성공은 첫 실패를 삭제하지 않는다. |
| #205, [signal-path run 40643391491](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40643391491) | Linux/Windows/macOS precision-tuner-worker 세 작업 성공. 각 작업은 CTest 3회 반복 설정. 전체 제품 workflow 성공과는 구별한다. |
| 원래 #222, [signal-path run 40984447101](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40984447101), attempt 2 | 세 플랫폼 튜너 작업 성공. Ubuntu CAB 작업 실패. 짧은 설정에서도 성공할 수 있다는 증거이며 간헐성 해소 증거는 아니다. |

macOS attempt 1 원본 artifact ID `12768324009`, 파일명 `PrecisionTuner-Worker-macOS-369107be345732c428d57ba051e31553a279d3e6-attempt-1`. 다운로드한 ZIP SHA-256: `28fd7bea6d66aa041a5cf2a295113cab1fc030fdc8df67318ec1fc49c555c90c`. `Testing/Temporary/LastTest.log`의 실패 구간을 [macos-original-failure.txt](macos-original-failure.txt)에 보존했다. 직접 job-log 조회가 실패해 같은 실행의 업로드된 원본 CTest 로그를 읽었다.

해시 자체도 계산했다. `4943210`과 원래 #222는 이전 해시와 일치한다. #205 및 확인한 main `a65be23141be26203cd8b49a93dff1895a7da2d2`는 새 파일 해시인데 pin은 이전 값이라 불일치한다. `preset-maintenance.yml`은 PR의 `preset-maintenance` 라벨을 조건으로 실행하므로 원래 #205/#222에서는 **skipped**였다. Python 단위시험 성공을 그 workflow 성공으로 쓰지 않는다.

## 이번 로컬 검증

환경: Linux, GCC 13.3, JUCE 8.0.8, Release, qualification-only. checkout `9163ec6`에 위 세 파일 변경을 적용한 로컬 빌드다. 빌드에 기록된 revision은 checkout SHA이며 이후 생성되는 GitHub 커밋의 배포 바이너리가 아니다.

- `python Tools/test_preset_maintenance.py`: **23/23 PASS**. 기존 4가지 변조 거부에 pause 1200→450, resume 1500→500, 280 ms 대기 축소 변조 거부를 추가했다. 정확한 파일만 허용하는 검사와 그 외 제품·버전·만료·배포 규칙 검사를 유지했다.
- 정식 CTest **4종 × 3회 = 12/12 PASS**, 총 152.96 s. 통합시험 세 번은 47.26/47.24/47.32 s. [출력](local-ctest.txt), [JUnit](local-ctest.xml).
- 별도 지연 주입 진단: 정식 파일을 바꾸지 않고 임시 복사본의 main에서 `stalledWorkerFreshness()`만 호출했다. `scenario.first==40`인 연속 push 직후, 기존 280 ms 대기 직전에 220 ms `sleep_for`를 추가했다. 450/500 ms는 exit 1 / 기존 조기 재개 오류, 1200/1500 ms는 exit 0 / stale 숨김·구형 큐 폐기·fresh stream 재획득 assertion 통과. 이는 스케줄링 지연의 인위적 재현이며 실제 macOS/Windows 부하 시험으로 간주하지 않는다.
- CRLF를 인식하는 `git -c core.whitespace=cr-at-eol diff --check`: PASS.

재현 명령:

```sh
cmake -S . -B build-review -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-review --target ChimeraPrecisionTunerTests ChimeraPrecisionTunerIntegrationTests ChimeraTunerTimingTests -j 3
ctest --test-dir build-review -R '^(ChimeraPrecisionTuner(Integration)?Tests|ChimeraTuner.*TimingTests)$' --repeat until-fail:3 --output-on-failure --no-tests=error
python Tools/test_preset_maintenance.py
```

## Amp Head / CAB: 독립적인 미해결 항목

직접 읽은 CI 로그는 다음과 같다.

| 범위 | 실행 / 작업 | 결과 |
|---|---|---|
| #222 CAB | [40984447101 / 123922392202](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40984447101/job/123922392202) | 16개 중 15 PASS, `ChimeraCabPanelStateTests` FAIL |
| #222 Amp Head | [40984446848 / 123832054189](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40984446848/job/123832054189) | 2개 중 1 PASS, `ChimeraAmpHeadControlTests` FAIL |
| main `2b4bf53` Amp Head | [40983673701 / 123829509748](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40983673701/job/123829509748) | 동일 오류 FAIL |

세 로그의 오류는 모두 `Head paint adds, removes or distorts pixels outside the single authored shell (second roof/feet/handle) [head model 1]`. 실제 runner는 ubuntu-26.04 image `20261025.179.1`, GCC 15.2.0이었다. Windows/macOS Amp Head 작업은 성공했다.

두 테스트는 공통 `Tests/CabHeadAppearanceTests.h::checkHeadAppearance()`를 사용한다. 원본 RGBA·메타데이터 검사를 통과한 뒤, `Source/CabHeadView.h::paint()`로 그린 shell과 독립적으로 그린 expected image의 **전체 픽셀 완전 동일성** 검사에서 실패한다. model 1은 Brit Edge다. 테스트 이름 두 개는 같은 헤드 렌더링 계약을 통과하지 못한 것이며, 별도의 CAB 오디오 결함을 뜻하지 않는다. 오류 문구만으로 실제 지붕/손잡이가 추가됐다고 확정할 수 없다.

로컬 GCC 13.3/Xvfb의 독립 프로브에서는 저장소 PNG와 같은 헤더를 사용한 26모델 × 3폭(240/640/1120) 78조건이 픽셀 차이 0으로 통과했다. JUCE graphics를 Release로 다시 컴파일한 프로브에서도 동일했다. 단, 외부 파일 resource shim을 쓴 좁은 프로브이며 CI의 전체 제품 빌드·embedded resource·GCC 15.2 환경을 재현한 시험은 아니다. **정확한 raster 불일치 원인은 아직 확정하지 못했다.**

별도 수정은 실패하는 GCC 15.2 환경에서 actual/expected/diff PNG, transform float 비트, 최초 차이 좌표·채널·최대 차이를 보존하는 진단부터 시작해야 한다. 검토 범위는 `CabHeadAppearanceTests.h`, `CabHeadView.h`, `AmpHeadShellMetadata.h`, embedded HeadShell assets 및 JUCE/software renderer·compiler 조건이다. 픽셀 허용오차 확대나 이미지 변경으로 먼저 성공시키지 않는다. 이 PR은 그 파일들과 검사 기준을 변경하지 않는다.

새 커밋의 원격 CI는 별도로 확인해야 한다. 위 과거 CI 및 로컬 튜너 PASS를 새 커밋의 전체 CI PASS나 병합 승인으로 승계하지 않는다.
