# Issue #3: Cakewalk Sonar / Cubase 10.5 종료 회귀 검증

검토일: 2026-10-13 (KST). 기존 코드 검토 기준:
`3817246f779a50f19b5378ec9661a2275e6b91b2`.

**현재 판정: 실제 Cakewalk Sonar 및 Cubase 10.5 검증은 NOT_RUN이다.**
이 문서와 자동 호스트 검사의 추가는 Issue #3의 해결 증거가 아니다.
실제 DAW에서 아래 결과와 설치 바이너리 식별 정보를 확보하기 전에는 이슈를 해결 완료로
분류하거나 CI 통과만으로 닫지 않는다.

## 1. 신고가 구분한 현상

[Issue #3](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/3)의
2026-09-25 본문은 Cakewalk Sonar와 Cubase 10.5에서 다음 차이를 보고했다.

- Chimera가 삽입돼 있고 재생만 했을 때 프로젝트 닫기는 정상이다.
- UI를 한 번 연 이후 프로젝트를 닫으면 DAW가 멈춘다.
- [후속 댓글](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/3#issuecomment-5831985941)은
  프로젝트를 닫지 않고 **플러그인을 제거하는 동작에서도** 같은 프리즈가 발생한다고 설명한다.

본문은 UI를 먼저 닫았는지, DAW 전체를 종료했는지, 정확한 설치 바이너리·호스트 패치 버전이
무엇이었는지 밝히지 않는다. 후속 댓글도 제거가 완료된 뒤의 종료 장애로 단정할 수 없다.
따라서 UI를 닫은 제거와 UI가 보이는 제거를 나누고, 제거 명령의 반환부터 관찰한다.
Sound Forge는 추정으로만 언급됐으며 AAX/Pro Tools는 검사되지 않았다. 이들을 검증된
재현 호스트로 취급하지 않는다.

## 2. 자동 검사와 실제 DAW 검사의 역할

| 검증 | 확인할 수 있는 범위 | 확인할 수 없는 범위 |
| --- | --- | --- |
| `ChimeraVST3LifecycleTests` | 실제 Windows VST3 모듈을 별도 프로세스에 로드해 에디터·처리·제거·참조 해제·최종 DLL 언로드가 반환하는지 검사 | Cakewalk/Cubase의 실제 프로젝트 관리, UI 캐시, 메시지 처리, 그래픽 드라이버 조합 |
| `ChimeraVBlankFailureTests` | 기존 JUCE 수정이 실패 반복 조건에서도 종료 신호를 처리하는지 독립 회귀 검사 | 신고자의 실제 멈춘 스레드가 VBlankThread였다는 입증 |
| 아래 DAW 절차 | 특정 DAW·설치 바이너리·프로젝트에서 UI 이력과 제거/종료별 동작 및 로그 확인 | 다른 DAW·다른 빌드까지의 포괄적 해결 보증 |

기존 이미지 자원 소유권, IR·튜너 작업 정지, convolution 해제, VBlank 종료 루프 수정은
[기존 진단 문서](STUDIO_ONE_TEARDOWN.md)에 정리돼 있다. 원 신고자의 프리즈 스택은 아직
없으므로 같은 수정을 다시 추가하거나 `stopThread(-1)` 등을 추측으로 바꾸지 않는다.
새 계측은 에디터·멤버 소멸과 기존 정지 동작을 세분화하며 DSP 동작을 변경하지 않는다.

자동 검사에서는 UI 미사용, UI 열기·닫기 후 프로젝트 수명 종료, UI가 있는 인스턴스 제거,
다른 인스턴스가 살아 있는 제거, 마지막 인스턴스 제거 뒤 모듈을 유지하는 상황을 분리한다.
컨트롤러와 컴포넌트의 해제 순서를 바꿔도 검사한다. 플러그인의 JUCE 메시지 루프와 연결하지
않은 호스트에서 API 호출 전·후를 기록하고, 멈춘 자식 프로세스는 watchdog 시간 초과로
실패 처리한다. 이 독립 호스트의 이름을 바꾸는 것은 해당 상용 DAW를 실행한 증거가 아니다.

### 자동 회귀 실행과 증거

기존 7개 경로를 보존하고 다음 12개를 검사한다.

| 경로 | 검사 내용 |
| --- | --- |
| `headless`, `headless-component-first` | 에디터를 한 번도 만들지 않는 대조군, 두 가지 최종 해제 순서 |
| `editor-pumping`, `editor-pending` | UI 사용 중 메시지 처리/대기 상태에서 종료 |
| `multiple-instances` | 세 인스턴스와 공유 UI 자원 종료 |
| `reopen-project` | 동일 자식 프로세스에서 DLL 생성·종료·언로드를 세 번 반복 |
| `live-remove-controller-first`, `live-remove-component-first` | 두 인스턴스를 유지하며 열린 UI의 대상 인스턴스를 세 번 제거·재생성 |
| `editor-closed-before-project-controller-first`, `editor-closed-before-project-component-first` | UI를 미리 닫고 메시지·추가 오디오 처리를 계속한 뒤 프로젝트 종료 |
| `remove-last-controller-first`, `remove-last-component-first` | 마지막 인스턴스 제거 → 빈 프로젝트/같은 모듈 유지 → 재삽입 → 다시 제거 → 빈 프로젝트 종료 |

Windows 개발 환경에서는 다음처럼 실행한다. `ChimeraVST3LifecycleTests`라는 기존 CTest
이름은 유지하며, 이제 실제 호스트 실행과 원시 증거 대조를 함께 수행한다.

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ChimeraAmpMatrix_VST3 ChimeraVST3LifecycleTests ChimeraVBlankFailureTests --parallel 2
ctest --test-dir build -C Release -R '^(ChimeraVST3LifecycleTests|ChimeraVST3WatchdogTests|ChimeraVST3LifecycleEvidenceTests|ChimeraVBlankFailureTests)$' --output-on-failure --no-tests=error
```

`.github/workflows/vst3-lifecycle.yml`은 PR의 정확한 head SHA에서 이 대상들을 빌드하고 실행한다.
기존 product/candidate CTest 경로도 같은 검사를 재사용하며 원시 기록을 보존한다.

- `build/vst3-lifecycle-evidence/`: 경로별 호스트 `.log`, 플러그인 `.trace.log`,
  `events.jsonl`, `summary.json`, `verification.json`, 외부 실행 로그.
- `verification.json`: 소스/빌드 정보, 실제 호스트·VST3 파일 SHA-256, 원시 파일 해시,
  완료된 호출과 인스턴스/에디터 수의 대조. 불완전·실패·단일 선택 경로는 전체 통과가 아니다.
  `commercial_daw_acceptance`는 `NOT_RUN`, `issue_3_resolved`는 `false`로 유지된다.
- `build/vst3-watchdog-evidence/`: 같은 부모 프로세스 감시 경로에 정지·출력 반복을
  의도적으로 주입하는 두 시험. 250ms 감시로 예상된 강제 종료와 미완료 단계를 보존해야 한다.
  이 합성 시험은 VST3 제품이나 DAW 실행 결과로 집계하지 않는다.
- `build/vblank-failure-verification.json`: 기존 JUCE 원본/수정 클래스의 실패·성공·혼합
  반환 조건 비교. JUCE 수정이나 기준은 이번 작업에서 바꾸지 않는다.

실제 VST3 경로의 자식 제한은 기존처럼 각각 45초다. 부모가 `host.return_ready` 이후
운영체제의 프로세스 종료를 확인하고, 호출 전·후 쌍과 플러그인 로그를 독립 검증한다.
출력 반복으로 감시가 무력화되지 않도록 파이프 읽기에도 상한을 둔다. 이 강제 종료는
오직 실패한 테스트 자식에 적용되며 제품이나 수동 DAW 진단의 종료 전략이 아니다.

## 3. 재현 전 준비

1. Cakewalk Sonar와 Cubase 10.5의 정확한 제품명·패치 버전, Windows 빌드, 오디오 장치·드라이버,
   샘플레이트/버퍼, GPU·드라이버 및 모니터 구성을 기록한다. 자동으로 채워지는 `session.json`의
   DAW 파일 버전과 함께 보존한다.
2. 같은 개발 빌드의 x64 VST3를 사용한다. 릴리스/개발 아티팩트 URL, 소스 커밋, VST3 버전,
   실제 로드한 모듈 경로와 SHA-256을 기록한다. 이전 설치본이나 스캐너가 선택한 다른 경로를
   확인한다. 아래 기본 경로는 사용자 지정 설치 경로에 맞게 바꾼다.

   ```powershell
   Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3\Contents\x86_64-win\SpectralForge Chimera.vst3'
   ```

3. 호스트 자동 UI 열기를 끄고 DAW를 완전히 종료한다. 각 행은 새 DAW PID와 새 진단 폴더를
   사용한다. 앞선 케이스에서 생성된 UI·정적 이미지·모듈 캐시를 다음 케이스로 가져오지 않는다.
4. 빈 프로젝트의 오디오 트랙 하나에 Chimera 한 개를 넣는다. 기본 상태와 기본 제공 자원을
   사용하고 다른 플러그인은 넣지 않는다. 약 10초 재생 후 정지한다. 실제 신고 프로젝트는
   이 최소 절차 뒤에 **복사본**으로 검사한다.
5. 일반 64비트 Windows PowerShell에서 도구를 실행한다. 설치 ZIP은 전체를 압축 해제하고,
   실행 파일 선택 창에서는 바로가기나 별도 실행 프로그램이 아닌 실제 Sonar/Cubase `.exe`를
   선택한다. 시나리오마다 아래 `-Scenario` 값을 바꾼다.

   ```powershell
   .\Tools\Trace-Chimera-Session.ps1 -Scenario ui-closed-remove
   ```

   설치된 선택형 도구는 `ReferenceTools\Trace-Chimera-Session.ps1`에 있다.
   `-HostExecutable`과 `-OutputDirectory`는 기존 방식대로 사용할 수 있다. 실행 정책 우회,
   관리자 권한, 기존 DAW 프로세스의 강제 종료는 도구에 필요하지 않다.

## 4. 최소 DAW 매트릭스

각 행의 UI 미사용/사용 여부는 로그로도 대조한다. UI 미사용은 단순히 창이 현재 안 보이는
상태가 아니라 **해당 프로세스에서 에디터를 만들지 않은 상태**다. DAW가 삽입 시 창을
자동으로 열었다면 UI 미사용 행을 통과 처리하지 않고 새 프로세스로 다시 실행한다.

| ID / `-Scenario` | UI 및 제거 동작 | 종료 순서와 별도 판정 | Cakewalk Sonar | Cubase 10.5 |
| --- | --- | --- | --- | --- |
| N-PC / `no-ui-project-close` | UI 미사용, 삽입 유지 | 프로젝트 닫기 반환 → 호스트 셸 반응 → DAW 종료 | NOT_RUN | NOT_RUN |
| C-PC / `ui-closed-project-close` | UI를 열어 렌더링 확인 후 닫기, 삽입 유지 | UI 닫기 반환 → 프로젝트 닫기 반환 → DAW 종료 | NOT_RUN | NOT_RUN |
| N-EXIT / `no-ui-host-exit` | UI 미사용, 삽입 유지 | 프로젝트가 열린 채 DAW 종료 | NOT_RUN | NOT_RUN |
| C-EXIT / `ui-closed-host-exit` | UI를 열었다 닫기, 삽입 유지 | 프로젝트가 열린 채 DAW 종료 | NOT_RUN | NOT_RUN |
| R-N / `no-ui-remove` | UI 미사용 상태에서 삽입 제거 | **제거 자체 반환** → 프로젝트 닫기 → DAW 종료 | NOT_RUN | NOT_RUN |
| R-C / `ui-closed-remove` | UI를 열었다 닫고 삽입 제거 | **제거 자체 반환** → 프로젝트 닫기 → DAW 종료 | NOT_RUN | NOT_RUN |
| R-V / `ui-visible-remove` | UI가 보이는 상태에서 삽입 제거 | 에디터를 수동으로 먼저 닫지 않고 **제거 자체 반환** → 프로젝트 닫기 → DAW 종료 | NOT_RUN | NOT_RUN |

R-V가 호스트 UI 구조상 불가능하다면 `SKIP`과 이유를 기록한다. R-C 성공으로 R-V를
통과 처리하지 않는다. N-EXIT/C-EXIT는 메뉴에서 DAW 종료를 요청하고, 프로젝트 닫기를
미리 수행하지 않는다. 저장 질문은 모든 행에서 동일하게 처리하고 프로젝트 복사본만 쓴다.

각 동작 뒤 PowerShell에 `PASS`, `HANG`, `FAIL`, `SKIP`을 입력한다. 최소 프로젝트에서
저장 질문 등을 처리한 뒤 약 15초 동안 응답이 돌아오지 않으면 `HANG`으로 기록해 수집한다.
이는 조사 시작 기준이며 영구 교착의 입증이나 제품의 종료 시간 사양이 아니다. 별도 시계로
관찰 시간을 기록한다. 스크립트 입력 시각의 차이는 사람의 반응 시간을 포함한다.

제거/프로젝트 닫기 `PASS`는 명령 반환과 남은 DAW 셸의 반응이 필요하다. 제거가 멈췄다면
프로젝트 닫기·앱 종료까지 추가로 누르지 않는다. DAW 종료 `PASS`는 원래 PID가 종료돼야
하며 창만 사라진 상태는 통과가 아니다. 스크립트는 `PASS` 입력과 실제 PID 상태가 모순되면
`FAIL`로 기록한다. 충돌·오류 대화상자·비정상 종료도 `FAIL`이다.

첫 선별은 행별 한 번, Issue #3 해결 판단용 검증은 두 DAW에서 각 필수 행을 새 PID로
세 번 연속 통과시킨 결과를 남긴다. 원 신고 프로젝트 복사본으로 UI 사용 후 프로젝트 닫기와
제거도 재확인한다. 원 프로젝트를 확보하지 못했으면 그 제한을 명시한다. 이 문서를 추가한
시점에는 어떤 실제 DAW 행도 수행하지 않았다.

## 5. 로그와 멈춘 위치 판독

`session.json`은 사용자가 관찰한 각 동작과 결과, UTC 입력 시각, 동작 전후 로그 줄 수,
원래 DAW PID의 종료 여부를 저장한다. 프리즈 뒤의 미수행 동작은 결과가 없는 상태로
남는다. `SKIP`, 결과 누락, 빈 로그는 성공 증거가 아니다. 이 파일은 수동 검사 기록이며
제품 호환성을 자동 승인하는 판정 파일이 아니다.

`Chimera-lifecycle.log`는 단조 증가 시각과 `thread`/`instance`를 유지하며 `pid`와 Windows
`tid`를 추가한다. `related`는 에디터·작업과 소유 프로세서를 연결한다. 메시지 스레드가
있는 경계에는 `message_thread`와 `on_message_thread`를 기록한다. `tid`는 Windows 덤프의
스레드 번호와 대조하고, `thread`는 JUCE의 스레드 식별자 표현으로 취급한다.

| 경계 | 추적 이벤트 / 외부 증거 | 판독 시 한계 |
| --- | --- | --- |
| UI 생성·소멸 | `processor.create_editor`, `editor.create.body`, `editor.destroy.body`, `editor.destroy.members.end` | `*.body.end`는 소멸자 본문 끝이다. `*.members.end`는 파생 클래스 멤버 정리 후이며 JUCE 기반 클래스 소멸 전이다. 둘 다 전체 VST3 view 해제 반환이나 DLL 언로드와 같지 않다. |
| 에디터 내부 종료 | `editor.timer.stop`, `editor.chooser.reset`, `editor.dialogs.destroy`, `editor.support.reset`, `editor.look_and_feel.detach` | 마지막 미완료 `.begin`은 조사 구간을 좁힌다. 그 호출이 원인이라는 단독 증거는 아니다. |
| 공유 UI 자원 | `artwork.bank.destroy`, `cab.artwork.destroy`, `head.shell.bank.destroy` | 다른 에디터가 같은 자원을 소유하면 마지막 에디터까지 남는 것이 정상일 수 있다. UI 닫기가 실제 에디터 소멸인지도 로그로 확인한다. |
| IR·튜너 작업 | `ir.owner`/`tuner.owner`, `ir.stop.requested`, `ir.stop`, `ir.worker`, `tuner.stop.requested`, `tuner.stop`, `tuner.worker` | 작업 인스턴스를 소유 프로세서와 맞춘다. 정지 요청은 종료 완료가 아니며, 시작되지 않은 작업에는 `worker.end`가 없을 수 있다. 중복 release/reprepare도 구분한다. |
| convolution 및 프로세서 | `cab.clear`, `cab.kernel.destroy`, `processor.release`, `processor.destroy.body`, `processor.destroy.members.end` | 하나의 worker 종료만으로 내부 convolution loader와 모든 멤버가 정리됐다고 판단하지 않는다. 최종 외부 release 반환까지 대조한다. |
| JUCE 메시지 스레드 | 에디터/프로세서 경계의 `message_thread`·`on_message_thread`, 덤프의 UI 스택 | 등록 스레드와 호출 위치의 관찰값이다. 메시지가 계속 처리된다는 생존 신호가 아니며, 단독 `on_message_thread=0`으로 호스트 계약 위반을 확정하지 않는다. |
| VBlankThread | 기존 실패 루프 회귀 검사 + 실제 프리즈 덤프의 스레드·대기 스택 | VBlankThread의 시작·종료를 제품 로그로 직접 계측하지 않는다. 실패 반환이 반복되는 조건과 드라이버의 `WaitForVBlank` 호출 내부에서 멈추는 조건은 다르다. 자동 회귀 통과만으로 후자나 신고자의 VBlank 원인을 해결했다고 판단하지 않는다. |
| VST3 반환·DLL 해제 | 자동 호스트의 view/component/controller release 전·후, `ExitDll`·`FreeLibrary` 전·후, 모듈 매핑 및 후속 메시지 처리 검사 | 플러그인 내부 로그는 자신의 DLL 언로드 완료를 증명할 수 없다. 실제 DAW는 프로젝트를 닫거나 마지막 삽입을 제거해도 DLL을 캐시할 수 있으므로, 즉시 언로드가 없다는 사실만으로 실패 처리하지 않는다. |

플러그인 `elapsed_us`는 각 계측 scope의 실행 구간이다. 서로 다른 프로세스/타이머 구현의
절대 단조 시각을 임의로 빼지 않는다. 수동 동작은 `trace_records_before/after` 범위로
연결하고, 자동 호스트는 자체 `operation_id`와 호출 전·후를 사용한다. 덤프에서 기다리는
스레드와 기다림 대상이 확인된 경우에만 lock/작업 종료/메시지/VBlank/DLL 경계의 원인
후보를 좁힌다. 마지막 로그 한 줄로 원인을 단정하지 않는다.

메시지 스레드 필드는 해당 scope 진입 시점의 등록 스레드 정보다. 동일한 scope의 끝까지
메시지 처리가 계속됐다는 의미가 아니다. 선택형 로그는 동기식 mutex·파일 쓰기를 사용해
타이밍에 영향을 줄 수 있으며 `processBlock`에서는 기록하지 않는다. 따라서 멈춤 구간을
찾는 자료로 사용하고 실시간 CPU·CAB 성능 측정치로 사용하지 않는다.

실제 DAW는 종료 API의 전·후를 자동 호스트처럼 직접 기록하지 않는다. DAW 로그의 소멸
이벤트가 모두 끝났는데 PID가 남는다면 작은 덤프의 스택·모듈 정보를 확인한다. UI 캐시로
에디터 소멸이 나중에 발생할 수 있고, 메시지 스레드나 VBlankThread는 JUCE 기반 클래스
또는 모듈 정리 단계에서 기다릴 수 있다. 이를 구분하기 위해 로그와 덤프를 함께 본다.

## 6. 수집과 결과 판정

프리즈를 다시 재현하면서 덤프가 필요한 경우 다음처럼 같은 시나리오를 실행한다.

```powershell
.\Tools\Trace-Chimera-Session.ps1 -Scenario ui-closed-remove -CaptureDump
```

동작 결과를 기록한 뒤 **프리즈가 유지된 상태에서** Enter를 누르면 도구가 시작한 PID의
작은 덤프를 한 번 저장한다. `SKIP`은 로그만 수집한다. 원래 프로세스가 종료됐다면
다른 PID에 연결하지 않는다. 선택한 실행 파일이 별도 호스트 프로세스에 작업을 넘겨
종료됐거나, 로그에 플러그인 검색 프로세스 PID만 있으면 그 실행을 유효한 호스트 검사로
취급하지 않는다. 실제 호스트 실행 파일을 확인해 새로 시작한다.

자료는 로컬 폴더에만 저장되고 자동 업로드되지 않는다. 작은 덤프는 스레드 스택·모듈과
메모리 안의 문자열/경로가 포함될 수 있으므로 공유할 자료를 선택한다. 실행 정책 우회나
권한 승격으로 수집을 강제하지 않는다. ZIP은 수집 당시의 스냅샷이다. 이후 종료 이벤트는
원본 로그에만 추가되므로 성공 실행은 DAW 종료 뒤에 ZIP을 수집한다.

검증 결과에는 다음을 함께 기록한다.

| 항목 | 기록 내용 |
| --- | --- |
| 설치 식별 | 아티팩트 URL, 소스 SHA, VST3 버전·경로·SHA-256 |
| 실행 환경 | DAW 제품·패치 버전, Windows, 오디오/GPU 드라이버, 샘플레이트·버퍼 |
| 시나리오 결과 | 위 행 ID, 회차, 최소/신고 프로젝트 구분, 각 동작 PASS/HANG/FAIL/SKIP |
| 실패 경계 | UI 닫기 / 삽입 제거 / 프로젝트 닫기 / DAW 종료 중 최초 실패와 관찰 시간 |
| 증거 | 동일 실행의 `session.json`, 수명 주기 로그, 선택한 덤프, 원래 PID 종료 여부 |
| 판정 제한 | 미실행 행, 사용하지 못한 호스트·원 프로젝트, 덤프 없는 원인 추정 |

자동 Windows 호스트 검사와 이 매트릭스는 별도 상태로 보고한다. 구체적인 재현 원인이
관찰되면 기존 수정과 중복하지 않는 변경만 개발 PR에 추가한다. 실제 DAW 결과가 없거나
일부만 통과했다면 **회귀 검사 보강 완료 / Issue #3 해결 검증 대기**로 보고한다.
