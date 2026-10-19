# AAX 개발 설치: 실패한 rollback의 이전 번들 보존

2026-10-19 KST. 관련: #97 / #101.
기준 소스: `1fa3b0ee924e40e1c5e089e0361f7cecaf6adc92`.
이 변경은 #101 위의 별도 설치 도구 수정이며 main/공개 태그/제품 DSP를 바꾸지 않는다.

## 재현된 문제

기존 `install()`은 이전 번들을 `TemporaryDirectory` 안으로 이동했다. 새 설치의
receipt 교체가 실패하고 rollback의 후보 삭제 또는 이전 번들 rename도 실패하면,
context 정리가 복구에 필요한 이전 번들을 삭제했다. receipt 복구 실패에서도
별도 receipt 사본이 남지 않았다. `KeyboardInterrupt`는 기존 `except Exception`을
통과해 불일치 상태가 남았다. 설치 완료 후 destination의 재검증도 없었다.

## 수정

- 같은 볼륨의 작업 디렉터리를 명시적으로 관리하고, rollback 실패 시 정리하지 않는다.
- 파괴적 작업 전에 이전 receipt 바이트를 `previous-receipt.json`에 보관한다.
- 이전/신규 번들 이동 완료 상태를 구분하고, 완료된 작업만 rollback한다.
- 설치 목적지와 복구된 이전 설치를 각각 기존 verifier로 다시 확인한다.
- 검사한 교체 단계의 `KeyboardInterrupt`에도 rollback 후 원래 예외를 반환한다.
- 복구 실패는 성공으로 보고하지 않으며 절대 recovery 경로와 수동 확인 안내를 반환한다.

## 실행한 검증

Linux x86_64 / Python 3.13.5, 로컬 실제 임시 파일시스템과 합성 PE/Mach-O 헤더 fixture.
실제 AAX 바이너리/Windows 파일 잠금/Pro Tools 세션 검증으로 계산하지 않는다.

| 코드와 검사 | 결과 |
|---|---|
| 기준 코드 + 기존 7개 검사 | 7 PASS |
| 기준 코드 + 확장 15개 검사 | 10 PASS / 4 FAIL / 1 ERROR |
| 수정 코드 + 동일 확장 검사, `-W error` | 15 PASS / 0 SKIP / 0 FAIL |
| Python bytecode 컴파일 | PASS |
| `install()` 밖의 AST / 기존 검사 메서드 | 변경 없음 |

새 8개 검사는 rollback rename/cleanup/receipt 실패, 최초 백업 이동 실패,
신규 번들 이동 실패, 중단된 업데이트, 신규 설치 실패, 목적지 재검증 실패를 다룬다.
이전 번들의 모든 파일 hash와 receipt 원본 바이트, 보존 경로를 검사한다.

```sh
python -W error Tests/test_aax_native.py -v
python -m py_compile Tools/aax_native.py Tests/test_aax_native.py
```

입력 Git blob: 도구 `3c9a1e6a59b04f0c5bcbb1b101eea8d07b957fbd`,
기존 검사 `ad8152d359c09b6e13cd2b3db46b842619fed742`.
로컬에서 GitHub 원문과 blob hash를 대조한 뒤 재현/수정했다.
수정 도구 blob `f3dd00ecea7c0a72194b336c7e3a4055a5e045de`,
확장 검사 blob `5840800ad4a06d801df74514b12d665cafee5fb9`.

## 복구 실패 시

출력된 `.chimera-aax-*` 디렉터리를 지우지 않는다. 모든 호스트를 종료하고
설치 위치, `previous.aaxplugin`, `previous-receipt.json`을 함께 보존한다.
receipt의 파일 hash와 번들을 별도 안전한 위치에서 대조한 뒤 복구한다.
번들은 설치 위치로 돌아왔고 receipt만 복구되지 않은 경우도 있으므로,
백업이 없다는 이유로 설치 위치를 삭제하거나 무조건 덮어쓰지 않는다.
이 PR은 자동 재시도/자동 복구 명령을 새로 제공하지 않는다.

## 승인 경계와 후속 검증

기존 AAX workflow가 이 도구와 검사 경로를 감시하며, 새 head에서 패키지에 포함된
도구의 실제 Windows/macOS 개발 ZIP 설치·재설치·제거를 다시 확인해야 한다.
부모의 AAX run `39127354204` 양 플랫폼 성공을 새 head PASS로 승계하지 않는다.
새 native CI 완료 전은 PENDING이다. 부모의 macOS CAB timing 실패도 그대로 남는다.

Source/CMake/parameter/preset/state/version/expiry/publisher 및 workflow는 변경하지 않는다.
PACE 서명, 일반 Pro Tools, 다음 버전 만료일, 공개 승인은 이 수정으로 충족되지 않는다.
전원 장애/SIGKILL/동시 설치/모든 명령 경계의 중단/제거 중 부분 삭제에 대한
새로운 원자성 보장은 없다. 이 수정 범위는 설치·업데이트 중 검사한 실패 경로다.
