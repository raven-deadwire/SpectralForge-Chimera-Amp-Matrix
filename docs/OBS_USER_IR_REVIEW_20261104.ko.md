# OBS 사용자 IR 복원 결함 및 main 통합 검토 — 2026-11-04 KST

## 판정과 고정한 소스

**현재 main에는 결함이 남아 있다. #252의 수정 자체는 세 운영체제의 실제 OBS 시험을 통과했다.**
이 문서와 추가 회귀 시험은 승인 전 로컬 제안이며 원격 브랜치·PR·main을 변경하지 않았다.

| 대상 | 확인한 상태 / SHA |
|---|---|
| main | `d90511c9520af565dadc12c47793b8bdffd4c4bb` |
| #251 | 2026-11-02 23:09:27 KST 병합; 원본 HEAD `9a382a593d66a4866bfa9002da1c37c803b8d4ce`; merge `6fca18392cbbe8b8463a8f20a20d61ea7d41ad8d` |
| #252 | Open / Draft / 미병합; HEAD `c2ff13a418a63adea5b5e51eb206a29de87fcfd1` |
| #252 Base | 아직 `claude/quirky-fermi-b0k2ck`; main으로 전환 필요 |
| 사용한 JUCE | 8.0.8, `d6181bde38d858c283c3b7bf699ce6340c050b5d` |

main에는 `Source/IRStateEncoding.h`가 없다. `IRLibrary::restore()`와 `irState::pool()/unpack()`이
6MiB 문자열 길이만 검사한 뒤 JUCE `MemoryBlock::fromBase64Encoding()`을 호출한다.
JUCE는 `<length>.`의 signed int 값을 size_t로 변환하여 먼저 할당한다. 짧은 `-1.A`가
SIZE_MAX 할당으로 이어지므로, 디코딩 후 4MiB 검사로는 막을 수 없다.

이번 작업에서도 **main의 실제 IRLibrary.cpp + 실제 공장 WAV + JUCE 8.0.8**에 #252의 회귀 시험을 연결했다.
512MiB 주소 공간 제한과 core dump 비활성화 조건에서 `FAIL: std::bad_alloc`, 종료 코드 1을 재현했다.
시험 harness가 예외를 잡은 결과이며 실제 OBS 프로세스가 크래시했다는 측정은 아니다.
동일한 시험에 수정된 IRLibrary.cpp를 연결하자 손상 복원 7종 및 기존 수명주기/오디오 검사가 통과했다.

## 최신 #252 CI: 문서의 미확인 표기를 갱신할 근거

아래 결과는 모두 **c2ff13a**에 대한 결과다. main에 다시 통합한 새 HEAD에 자동 승계하지 않는다.

OBS 실행: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41438625046

| 환경 | 실제 사용자 IR 시험 | 재시작 레벨 / 주파수 분포 거리 | 해제 |
|---|---|---|---|
| Windows, OBS 31.1.2 | PASS; job `125254606912` | -0.0 dB / 0.000 | 10/10 |
| Linux, OBS 30.0.2 | PASS; job `125254607085` | -0.0 dB / 0.003 | 10/10 |
| macOS Apple, OBS 31.1.2 | PASS; job `125254606638` | -0.0 dB / 0.000 | 10/10 |

세 job의 checkout 및 시험 로그에서 다음을 직접 확인했다.

- 정상 400Hz 저역통과 IR, 잘못된 형식, `-1.A`, 인코딩은 정상이지만 WAV가 아닌 데이터의 첫 복원.
- 정상 IR의 세 live lane 연결과 저장된 원본 WAV 바이트 일치; 재시작 후 같은 검사.
- 원본 IR 파일 없이 재시작한 뒤 레벨 차 <1dB, 주파수 분포 거리 <0.02.
- 손상 IR은 cabinet filters와 비교하여 분포 거리 <0.02, 레벨 차 <1dB.
- 두 차례 정상 종료 및 누적 10개 인스턴스 해제. **동시 10개가 아니라 5개씩 두 번**이다.
- 별도 다중 필터/붙여넣기 시험도 세 플랫폼에서 PASS.

손상 IR은 최초 저장 시 정상적인 fallback 상태로 정리될 수 있다. 재시작 검사는 이 상태의 지속성을 확인하며,
손상 문자열 자체가 저장 파일에 유지되어 두 번째로 다시 디코딩된다는 증거는 아니다.
해제 로그 10/10은 인스턴스 수명주기 증거이며 ASan/LSan 누수 전수 검증으로 확대하지 않는다.

워크플로 전체 conclusion은 **failure**다. `installer-safety (macos-15)` job `125254606941`만
**cancelled**, runner 이름과 steps가 없고 로그는 404다. 취소 원인 자체는 확인되지 않았으며,
사용자 IR 테스트 실패로 해석할 근거는 없다. 이 설치 시험은 미완료로 남긴다.

빌드 실행: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41438625035

| 플랫폼 | CTest 결과 | 근거 |
|---|---|---|
| Windows | 106/106 PASS | job `125255513485`의 실제 CTest 출력 |
| Linux | 95/95 PASS | artifact `13147689403`의 JUnit XML, LastTest.log 및 evidence.json |
| macOS | 94/94 PASS | artifact `13157726229`의 JUnit XML, LastTest.log 및 evidence.json |

Linux/macOS artifact의 SHA 및 run ID를 대조하고 evidence.json의 파일 SHA-256도 검증했다.
두 플랫폼의 거대한 전체 job 로그는 도구 전송 오류로 읽지 못했으므로 실제 CTest 결과 artifact를 사용했다.
IRStateEncoding, IRLibrary, IRCollection, CabPanelState, StateRecallAudio 및 프리셋 관련 시험이 포함된다.
Linux/macOS LastTest.log에는 구형 A-only 프로젝트 이행, A/B·6개 IR slot, 원본 파일 삭제 후 복원,
6개 near-limit stereo IR, hash 불일치와 dangling reference 거부가 PASS로 남아 있다.
테스트 수 차이는 플랫폼별 구성 차이다. 구성된 시험 안에서는 실패/건너뜀 0이다.

main의 정확한 d90511c HEAD에서 조회된 Actions 실행은 AAX Native Development `41655816566`이며
Windows/macOS job은 성공했다. 이는 #252 수정의 OBS 검증이 아니며 Linux·전체 빌드·OBS 결과를 대체하지 않는다.

## 변경 범위와 안전한 통합 절차

main...#252 비교는 ahead 1 / behind 35, merge base는 #251의 9a382a5다.
#251을 다시 가져올 필요 없이 #252 단일 변경만 보존한다.
Base 브랜치는 #255의 OBS 에디터 작업에도 재사용됐다. PR API가 표시한 과거 base SHA만 신뢰하지 않는다.

로컬 3-way merge 검사는 **OBS/README.md 한 곳의 충돌**을 발견했다. 제품 소스 충돌은 없다.
해결안은 #252의 inline/pooled 실패 처리 설명을 넣으면서 main의 #255 에디터 창 검증 내용을 그대로 보존한다.
CMake의 새 테스트 등록도 main의 최신 타깃 설정을 유지한 채 추가했다.

승인 요청 범위:

1. 변경 직전 main 및 #252 HEAD가 위 SHA와 같은지 재조회한다. 달라지면 새 diff와 검증을 먼저 갱신한다.
2. #252 Base를 main으로 변경한다.
3. #252 head c2ff13a에 main d90511c를 merge하고 README 충돌 및 아래 테스트/문서를 적용한다.
   두 이력을 보존하는 merge commit으로 갱신하여 force push를 피한다. 이전 Base 브랜치에는 쓰지 않는다.
4. Draft는 유지하고 새 HEAD의 Open Beta Build and Test 및 OBS add-on을 확인한다.
5. 새 HEAD의 CI 및 취소된 macOS 설치 시험이 통과한 다음 별도 main 병합 승인을 요청한다.

최종 변경 파일:

- 제품: `Source/IRStateEncoding.h`, `Source/IRLibrary.cpp`, `Source/IRStateAssets.h`.
- 시험: `Tests/IRStateEncodingTests.cpp`, `Tests/IRLibraryTests.cpp`, `Tests/CabPanelStateTests.cpp`, `OBS/tests/obs_user_ir_test.py`.
- 등록/문서: `CMakeLists.txt`, `OBS/README.md`, 기존 #252 검토 문서 및 이 문서.

제품 변경은 #252와 동일하다. 파라미터 ID와 상태 스키마를 변경하지 않는다.
4MiB(4,194,304바이트)는 기존 import/decode 정책이며 이번에 새로 정상 IR의 허용 크기를 줄이는 변경이 아니다.
inline 손상 IR은 해당 slot을 필터로 대체하고 정상 다른 lane을 보존한다.
pooled IR_ASSETS 손상은 상태 적용 전에 전체 복원을 거부하여 현재 세션을 유지한다.

## 이번 로컬 추가 검증과 남은 시험

이번에 실제 실행한 검사:

| 검사 | 결과 |
|---|---|
| 수정 전 main IRLibrary + 손상 회귀 | 의도한 FAIL: std::bad_alloc |
| 통합안 IRLibrary + 동일 회귀/기존 시험 | PASS: 손상 7종, 이전 IR 제거, 정상 lane 보존, 필터 출력 일치, 파일 삭제 후 원본 바이트/오디오 보존, worker 취소/해제/warm cache |
| 강화한 IRStateEncodingTests | PASS: 258개 길이의 정상/구형 trailing-dot 왕복, 0., 잘못된 접두부 17종, 4MiB-1/정확히 4MiB/완전한 4MiB+1 payload 거부 |
| 실제 pool/unpack 독립 probe | PASS: 음수/초과/overflow 4종 거부, 거부 시 table 불변, 구형 trailing-dot 바이트 왕복 |
| Python 구문 및 git diff --check | PASS |

g++ 13.3 C++20으로 실제 JUCE 모듈을 직접 링크했다. 전체 플러그인/CMake/실제 OBS를 로컬에서 빌드·실행한 결과는 아니다.

`Tests/CabPanelStateTests.cpp`에는 pooled 길이 접두부 4종을 실제 processor에 넣어
복원 전후 직렬화된 전체 세션이 동일한지 검사하는 코드를 추가했다.
저장 API의 성공도 검사하여 빈 결과끼리 비교해 통과하는 경우를 막았다.
**이 새 전체 processor 검사는 아직 실행하지 않았다.** 독립 pool/unpack PASS로 대체하지 않는다.

통합 후 필요한 실행:

- 새 HEAD에서 세 OS의 IRStateEncoding/IRLibrary/CabPanelState/StateRecallAudio/BetaLifecycleProcessor를 포함한 전체 CTest.
- 세 OS 실제 OBS의 사용자 IR, 여러 필터/붙여넣기, 프리셋 복원, 최신 main의 에디터 종료 회귀.
- 취소된 macOS installer-safety 완료 확인.

여전히 미실행/미확인인 범위:

- 위 로컬 통합안의 새 HEAD 전체 CI와 새 processor 보존 검사.
- Windows OBS 32.2.2와 macOS Intel/Rosetta에서 **새 사용자 IR 전용** 시험. 해당 환경은 일반 프리셋 시험만 통과했다.
- 모든 과거 배포판의 실제 사용자 프리셋 전수 검사, 사용자 보유 IR 전수 검사, 실제 오디오 장비와 장시간 방송.
- 4MiB 경계의 **실제 WAV를 OBS에 넣는** 호스트 시험. 현재 경계 증거는 byte decoder 시험이고, 기존 CTest는 near-limit 실제 stereo IR 복원이다.

## 원본 링크

- #251: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/251
- #252: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/252
- main 복원 코드: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d90511c9520af565dadc12c47793b8bdffd4c4bb/Source/IRLibrary.cpp#L397-L422
- #252 디코더: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/c2ff13a418a63adea5b5e51eb206a29de87fcfd1/Source/IRStateEncoding.h
- #255: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/255
