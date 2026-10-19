# AAX 개발 설치: 실패한 rollback의 이전 번들 보존

> 아래 최초 15개 검사 기록은 `8fafef8e`의 이력이다. 후속 통합 검토에서
> 발견한 중단·제거 결함과 현재 31개 회귀 검사는 문서 마지막 절을 따른다.

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

## PR #106 → #101 통합 검토 및 추가 수정

2026-10-19 KST. 공개 main 병합·태그·릴리스·updater 변경은 승인 범위가 아니다.
통합 목적지는 `codex/aax-native-foundation-20261018`이며 #101은 Draft로 유지한다.

### 확인한 기존 native CI

[`39227592109`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39227592109)는
checkout 로그와 run metadata 모두 `8fafef8e67b92e18511d67aa548b327f4991fe4a`다.
Windows job `117977560974`, macOS job `117977560710`은 모두 SUCCESS다.
양쪽 CTest 5/5, actual ZIP 격리 설치·동일 빌드 재설치·제거 및 disposable runner
시스템 AAX 경로 검사를 확인했다. Windows LoadLibrary/ACF exports와 macOS
Universal architecture/ad-hoc code seal도 성공했다. 이 실행을 재시작하지 않았다.
이것은 해당 SHA의 native 증거이며 후속 Python 수정의 OS별 실행 결과를 대신하지 않는다.

### 추가로 재현한 결함

1. `rename()`은 완료됐지만 다음 Python 상태 플래그 기록 전에 중단되면
   이전/신규 번들 이동이 누락되어 update rollback 실패 또는 신규 설치 불일치가 발생했다.
2. 제거의 `TemporaryDirectory` 자동 정리는 이동 직후 중단 및 rollback 실패 때
   복구용 원본을 지울 수 있었다. `rmtree`가 파일 일부를 삭제한 후 실패하면
   남은 디렉터리를 되돌려도 원본 hash를 복원하지 못했다.
3. 제거 후 receipt unlink 실패/완료 직후 중단은 receipt와 bundle을 불일치 상태로 남겼다.

설치는 파괴적 이동 전에 작업 폴더 보존을 설정하고 실제 staged/backup 존재 여부로
이동 완료를 판정한다. 제거는 원본을 건드리기 전에 receipt 포함 전체 복구 복사본을
만들어 검증하고, 실패 시 그 복사본으로 복원한다. 복구 실패/재중단에서는 검증된
복사본을 지우지 않고 절대 경로를 오류에 남긴다. 추가 디스크 공간이 부족하면
원본 이동 전에 실패한다. `install()`/`uninstall()` 외 AST는 변경하지 않았다.

### 재현 및 회귀 증거

Linux x86_64 / Python 3.12.14, 실제 임시 파일시스템 + 합성 PE/Mach-O fixture.
기존 15개 검사 메서드는 그대로 유지했다.

| 검사 | 결과 |
|---|---|
| `8fafef8e` 원본 + 기존 15개 | 15/15 PASS |
| `8fafef8e` 원본 + 확장 30개 | 20 PASS / 1 FAIL / 9 ERROR |
| 후속 수정 + 같은 30개 | 30/30 PASS |
| 정상 경로 Python line 중단 검사까지 포함 | 31/31 PASS, skip 없음 |
| `py_compile`, install/uninstall 밖 AST 동일성 | PASS |

최종 도구 blob: `43e300320db6896333ab01b955c0fb37007ec31b`.
실행: `python -W error Tests/test_aax_native.py -v`.
완료된 rename/receipt syscall 직후 중단, rollback 재중단, 제거 중 일부 삭제,
receipt unlink/복원 실패, 백업 공간 부족, receipt-only/bundle-only 상태와 hash
불일치 시 무변경 거부, 성공 설치 후 정리 실패를 검사한다. 정상 경로 각 Python
line의 단일 중단 검사는 정상 설치 상태 또는 검증 가능한 복구본 보존을 확인하며,
모든 OS syscall 내부/전원 장애의 원자성을 입증하지 않는다.

개인 프리셋·IR·Pro Tools 프로젝트와 다른 AAX 플러그인 sentinel의 바이트 보존을
실패 경로에서도 검사한다. 소유권/receipt 검증 실패 시 install/uninstall은 쓰기 전에
거부한다. 제거 대상은 고정 Chimera AAX bundle과 development receipt뿐이다.
DSP, 파라미터 ID/ordinal, preset/state/IR 처리, CMake, beta expiry, 공개 정책은
변경하지 않으므로 제품 state migration은 필요하지 않다.

### 남은 위험과 별도 배포 조건

- SIGKILL/TerminateProcess/전원 장애에는 Python rollback이 실행되지 않는다.
  fsync journal이나 재부팅 후 자동 복구가 없으므로 원자적 내구성을 보장하지 않는다.
- 동시 install/uninstall, 외부 프로세스의 번들/receipt 교체(TOCTOU)는 지원하지 않는다.
  호스트 종료 후 한 번에 한 작업만 실행한다. 실제 Pro Tools/백신에 의한 Windows
  잠금과 권한·ACL·macOS 확장 속성의 완전 복원은 별도 실제 환경 검사 대상이다.
- 검증된 성공 상태를 확정한 뒤 임시 폴더 정리가 실패/중단되면 오류와 잔여물이
  남을 수 있다. 성공 install은 새 번들, 성공 uninstall은 제거 상태를 유지한다.
  남은 폴더를 재귀적으로 자동 청소하거나 복구 실패를 성공으로 처리하지 않는다.
- 합성 헤더와 오류 주입은 실제 native binary/호스트 검증이 아니다. 위 native CI는
  기존 SHA의 증거이며 새 source/통합 SHA의 Python 회귀와 별도로 기록한다.
- PACE 및 최종 플랫폼 서명, 일반 Pro Tools Windows/macOS Intel/Apple Silicon
  스캔·재생·자동화·세션/IR 복원·PDC·종료 인수, 최종 Setup/pkg는 미충족이다.
- Open Beta 통합 검증, 알려진 macOS CAB timing 실패, 다음 후보 만료일·10월 23일
  공개 승인도 별도 조건이다. 설치 회귀 PASS로 release_approved를 바꾸지 않는다.

