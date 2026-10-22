# v1.3.2-beta.2 배포 가능성 검증 — 2026-10-19 KST

**판정: BLOCKED / 10월 23일 공개 가능 근거 없음.** 네 P0를 포함한 통합 제품 소스·바이너리가 아직 없고, beta.2 만료일과 공개 승인도 없다. 10/21 테스트 → 10/22 후보 검토 → 10/23 개발 완료 및 조건부 공개 검토는 유지한다. 목표일을 공개 승인으로 바꾸거나 미완료 기능을 제외하지 않는다.

기준은 [일정 PR #93](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/93)과 [필수 요구사항 #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97)이다. 기존 Draft #98/#100/#101/#102/#103의 구현을 복제하거나 임의로 통합하지 않았다. 이번 변경은 #93의 publisher 보강 및 검증 기록이다.

## 대상과 소스

| 구분 | 고정한 소스 | 상태 |
|---|---|---|
| 공개 beta.1 제품 | `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58` | 공개 이력 기준 |
| 조회 당시 main | `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f` | VERSION=1.3.2, RELEASE_CHANNEL=beta.1 |
| 일정 #93 감사 전 | `e1b418e12849b1a32009c2ec585e4b89aa01be8d` | Draft, main 미병합 |
| 수명주기 #98 | `b7923ea8b7a0f85cfbfafb0e0338238adc09b181` | Draft, main 미병합 |
| Transpose 연구 #100 | `6ec78c475311827ccfd1949c9ae9ce364f79c0b4` | Draft, 제품 미통합 |
| AAX #101 | `1fa3b0ee924e40e1c5e089e0361f7cecaf6adc92` | Draft, main 미병합 |
| 사양 문서 #102 | `274d9c383ca34a2bd66b59921d836231f55d36ff` | #98 위의 Draft |
| 실측 체계 #103 | `8c502f4f924886cd036c8367bc32fa6d123f59bd` | #102 위의 Draft |
| beta.2 통합 제품 | 없음 | 최종 SHA / 후보 PR / 패키지 미확정 |

main/#93/#98/#100/#101에서 다음 Git blob이 모두 동일함을 직접 대조했다. 버전 파일과 beta 정책도 동일하다. 따라서 연구 PR이나 개발 AAX의 CI 성공을 새 Transpose/FabFilter 제품 구현으로 해석할 수 없다.

| 생산 파일 | 공통 Git blob |
|---|---|
| `Source/PolyPitch.h` | `8149d7201376f8b4e6cdf89308be9e03e8fb40fe` |
| `Source/GraphicalEQ.h` | `ec6634bf43a92fba8bc74e666b7a1af8d16b19b4` |
| `Source/CommonDynamics.h` | `cf487ab9ed7900bfa46618a3c8318101540f711f` |
| `Validation/beta-lifecycle-policy.json` | `7e2128ca2bb7e37af6f36dbe0f11d1d3829cd656` |

## 독립 P0 판정

### Transpose: 지연 FAIL / 음질 수용 BLOCKED / 제품 반영 없음

- 생산 `PolyPitch::prepare()`는 32ms 이상인 2의 거듭제곱 FFT 창을 선택하며 `latency()==size`다. 48kHz 2048 samples = 42.667ms, 44.1kHz 2048 samples = 46.440ms다. 호스트 왕복 지연은 별도다.
- #100은 생산 Source/Tests/root CMake 변경이 없다. [연구 보고서 및 원자료](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/6ec78c475311827ccfd1949c9ae9ce364f79c0b4/docs/TRANSPOSE_CORRELATION_RESEARCH_20261018.md)를 확인했다. 실제 native 측정 소스는 `f2fa6acb63e272cfbbdce4ce06c50d24eefc59a0`이고 후속 head는 근거를 추가한 커밋이다.
- Low B −2 합성 배음음 기음 오차는 +67.55 cents → 0.01 cents 미만으로 개선됐다. 하지만 일정 피킹 최대 추가지연 2.88ms와 달리 강약 교대 12.10ms, 지속음 위 작은 어택 34.60ms다. 48kHz ±1/±2의 ≤10ms 요구를 통과하지 못했다.
- block64 wall deadline miss 12회, 가변 지연/고정 PDC 미해결, dyad 평가기 부적합도 남는다. 합성 단음 성적을 다성 음질 통과로 확대하지 않는다.
- Azhi Dahaka 단독 DI 및 동일 입력/레벨의 Neural DSP 원본·제품 버전·설정·캡처, 실제 DAW/loopback·청음 증거가 없다. 이 감사에서 실제 악기나 Neural DSP를 실행한 것은 아니다.

### FabFilter: 원본 분석·제품 DSP 적용 입증 BLOCKED

- 현재 `GraphicalEQ.h`는 독립 RBJ biquad, `CommonDynamics.h`는 stereo-linked feed-forward 공통 다이내믹스다. 12밴드 편집, 공유 Dynamic EQ 4노드, 컴프레서·게이트/익스팬더·리미터는 기존 공개 기능이다.
- 최신 관련 PR 및 생산 파일 비교에서 #97 이후 Pro-Q 4/Pro-C 3/Pro-MB/Pro-L 2/Pro-DS/Pro-G의 원본 응답·시간·검출·위상·지연 측정과 연결된 새 제품 적용 커밋을 확인하지 못했다. UI 존재나 기능 회귀 PASS는 그 증거가 아니다.
- 전체 FabFilter 연구 범위는 유지한다. 이번 통합 배포의 EQ/Dynamic EQ/공통 다이내믹스 적용 범위를 명시하고, 원본 버전·입출력·설정 → 분석 → 적용 커밋 → 동일 조건 비교를 연결해야 한다.

### 베타 수명주기: 구현·전용 자동검사 진전 / beta.2 배포 BLOCKED

- #98에 정확한 일·시·분·초, UTC/KST, 만료/시계 오류 표시와 만료 상태 저장·후속 복원 검사가 있다. [전용 CI 39073751028](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39073751028)는 현재 **7/7 jobs SUCCESS**다. PR 본문의 과거 진행 중 표시는 최신 결과와 구분했다.
- 활성 인스턴스는 실행을 유지하고, 만료 후 새 인스턴스는 상태를 보존하며 출력이 음소거되는 기존 오프라인 정책이다. 합성 후속 빌드 복원은 실제 사용자 DAW 세션 검증과 별개다.
- 정책 파일은 여전히 **beta.1 전용 `2027-02-11T14:59:00Z` / 2027-02-11 23:59 KST**, 14일 전 경고다. `beta_lifecycle.configure_metadata()`는 source version과 policy version 불일치를 거부한다.
- beta.2의 만료일 승인, 해당 날짜를 넣은 통합 바이너리, 설치 화면/문서/feed 일치 및 실제 DAW 복원은 미충족이다. 기존 날짜를 승계하지 않는다.

### AAX: 개발 빌드 PASS / 서명 BLOCKED / 일반 Pro Tools NOT_TESTED

- #101의 Windows x64/macOS Universal AAX Native와 host bypass PDC 수정이 있다. [39127354204](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39127354204)의 두 native job 로그에서 실제 compile, **각 5/5 CTest**, 개발 설치/재설치/제거 성공을 확인했다.
- 패키지는 `AAX-UNSIGNED-DEVELOPMENT`다. `Tools/aax_native.py`는 `pace_signed=false`, `normal_host_accepted=false`, `release_approved=false`를 생성한다. macOS ad-hoc seal은 PACE 또는 최종 Apple 서명 수용 증거가 아니다.
- Transpose/FabFilter/#98 변경은 AAX branch에 통합되지 않았다. 같은 beta.2 DSP/정책을 포함한 Windows/macOS 최종 서명 설치 패키지와 바이너리/패키지 hash가 필요하다.
- 일반 Pro Tools의 Windows x64/macOS Intel/Apple Silicon에서 scan/load, mono/stereo, automation, PDC, session save/reopen, preset/IR, UI, delete/session/app shutdown, beta 경계 검증이 없다. 개발 compile을 이 항목에 대입하지 않는다.
- Avid/PACE 도구·서명 권한·실제 호스트 환경 확보가 외부 의존성이다. 이 감사에서는 계정 미보유를 단정하거나 인증/계약을 대신 수행하지 않았다.

### 공통 산출물: 시스템 실측 BLOCKED

#102는 사양 초안, #103은 환경 schema/collector 및 계획된 측정표다. [#103 전용 CI 39127884362](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39127884362)는 현재 SUCCESS이지만 실제 제품 callback adapter 및 DAW 측정은 없다. 실제 Release 모듈의 source/binary hash, 대표 하드웨어/OS/host/driver, 48kHz 128/256·4×, Classic/Dual/Matrix, Transpose bypass/−2, 1/2/4인스턴스, GUI/분석기, 30분 동작/재열기 근거가 필요하다. 미실측 8/16/32GB 제안을 인증 최소사양으로 승격하지 않는다.

## 최신 CI와 한계

| 고정 소스 | 직접 조회한 CI 결과 | 배포에 대한 의미 |
|---|---|---|
| #93 `e1b418e` | [publisher 39209811173](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39209811173) 5/5 SUCCESS; [제품 39209811064](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39209811064) FAILURE | macOS `ChimeraOriginalCabIntegrationTests` 실패, CAB-only p99 block budget 초과. 56/57, assembly skipped. 통합 gate **7 PASS / 121 BLOCKED**, hard gates 7/63 PASS. beta.2 통합판의 판정이 아니라 이 일정 branch의 기존 제품 검증 결과다. |
| #98 `b7923ea` | lifecycle 7/7 SUCCESS; [통합 39073751041](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39073751041) FAILURE | macOS tuner 192kHz/64/B0 route에서 acquire 186.682875ms, settle=-1, error=99999. 수명주기 PASS로 상쇄하지 않는다. 전체 제품 workflow는 별도로 SUCCESS다. |
| #100 `6ec78c4` | [제품 39074028939](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39074028939) FAILURE | macOS GraphicalEQUITests: stopped audio analyzer decay 실패, 56/57, assembly skipped. 연구 수치와 구분한다. |
| #101 `1fa3b0e` | AAX 전용 및 전체 제품 SUCCESS; [CAB 39127354198](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39127354198), [튜너 통합 39127354277](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39127354277) FAILURE | 모든 CI 통과가 아니다. AAX compile 성공은 이 실패나 실제 Pro Tools 수용을 면제하지 않는다. 튜너 실패 job/step은 직접 확인했지만 해당 대형 로그는 connector 전송 오류로 회수하지 못했다. |

원격 workflow의 성공/실패/취소는 [snapshot.json](snapshot.json), 직접 확보한 로그의 핵심 행은 [ci-excerpts.txt](ci-excerpts.txt)에 보존한다. PR-triggered run 목록과 지정한 job을 조회했으며 모든 수동 실행의 전수 목록이나 이 감사에서의 native 재실행으로 주장하지 않는다. 실패를 재시도 성공으로 지우거나 threshold를 완화하지 않았다.

## 발견한 배포 게이트 결함과 수정

기존 `publish_release.release_policy()`는 모든 beta에 `I2.PITCH_LIVE`와 `I2.PITCH_DI`의 제외를 요구한다. `weekly_publication`의 고정 128/130개 검사와 legacy-gap 승인 경로는 새 FabFilter/AAX/Pro Tools P0를 포함하지 않는다. #93 일정의 `requirements_issue: 97`과 설명문은 이 누락을 자동 검증하지 않는다.

수정 전 **네트워크 없는 합성 승인/근거 fixture**로 재현했다. 실제 사용자 승인을 생성한 것이 아니다.

| 합성 입력 | 수정 전 평가기 |
|---|---|
| beta.2, 새 버전/SHA에 맞춘 legacy-gap receipt, 21 PASS / 107 BLOCKED | `publication_allowed=true` |
| beta.2, 기존 128개 검사 전부 합성 PASS | `publication_allowed=true`, `all_required_checks_passed=true` |
| 1.3.2-rc.1, 기존 130개 검사 전부 합성 PASS | `publication_allowed=true` |

이는 전체 공개 트랜잭션을 실행한 증거가 아니다. 별도 승인/CI/패키지 검사는 원래 존재한다. 결함은 그 조건들이 충족되어도 **새 필수 P0가 검사 목록에서 빠질 수 있다는 점**이다.

수정은 기존 #93에서 다음과 같이 제한했다.

- `ISSUE97_P0_GATE_UNIMPLEMENTED` 차단: beta.1 이후의 beta/RC/stable 및 더 높은 버전은 현재 acceptance 체계로 공개할 수 없다. 범용 publisher가 원격 조회·staging 전에 거부하고, 직접 승인 평가 호출도 같은 제한을 적용한다.
- 과거 beta.1 계약과 공개 이력은 보존한다. 별도 플래그, 일정 날짜, 재작성된 일반 승인 receipt 또는 기존 검사 전부 PASS로 새 P0를 통과시킬 수 없다.
- 이것은 **P0 검증 체계를 완성했다는 주장이 아니라 그 체계가 없는 동안의 fail-closed 차단**이다. 해제에는 Transpose 지연/음질, FabFilter 실제 반영, 버전별 beta 정책, 서명된 AAX/일반 Pro Tools 및 실측 근거를 최종 source/binary에 연결하는 검증 구현과 검토가 필요하다. 단순 날짜 도래/승인 bool 변경으로 해제하지 않는다.
- Windows/macOS/Linux의 기존 calendar-contracts job에서도 publisher/approval 회귀 suite를 실행하도록 연결했다. 새 PR이나 중복 제품 구현은 만들지 않았다.

### 수정 검증

- 수정 전 신규 재현: 3 test methods, **9 failing assertions/subtests**. [p0-before.log](p0-before.log).
- 수정 후 publisher/approval: **73/73 PASS**. [p0-after.log](p0-after.log).
- 일정·publisher·승인·근거·과거 publisher·consolidator 총 7 suite: **224/224 PASS**, 0 SKIP. [p0-regression.log](p0-regression.log).
- 기존 strict evidence의 stale producer/run/artifact 거부 시험은 역사적 scope fixture로 유지했고, 새 scope 차단에 가려져 허위 PASS가 되지 않도록 positive fixture의 버전만 분리했다.
- `git diff --check` PASS. Source, 제품 CMake, VERSION, RELEASE_CHANNEL, beta expiry, validation threshold, 승인 원장, waivers, 공개 자산은 수정하지 않았다.
- 이 보고서 작성 시 새 patch의 원격 CI는 아직 실행 전이다. 위 원격 결과는 표의 고정 SHA에만 적용한다. 새 head의 publisher CI 결과는 PR #93에서 별도로 확인한다.

## 배포 가능 상태로 바뀌는 조건

1. 네 P0를 실제 제품에 통합하고 beta.2 최종 source SHA 및 실행 가능한 후보를 고정한다. 미완성 연구 엔진을 완료 기능으로 넣지 않는다.
2. Transpose 동일 DI/Neural 원본 비교, ±1/±2 ≤10ms 목표, 약한 어택·기음·코드·artifact·고정 PDC·실시간 수용을 각각 충족한다.
3. FabFilter 원본 버전/측정/분석과 실제 DSP 적용 커밋·회귀 결과를 연결한다.
4. beta.2 전용 만료일을 승인받고 그 날짜가 들어간 최종 바이너리/UI/설치/feed와 만료 전후 데이터 회수·업데이트 복원을 검증한다.
5. 같은 제품의 최종 Windows/macOS AAX 패키지에 PACE·플랫폼 서명을 완료하고 일반 Pro Tools 3환경의 설치/동작/복원/PDC/종료 근거와 hash를 확보한다.
6. 실제 환경·성능표를 작성하고, 관련 CI 실패를 해결한 동일 source의 필수 CI/패키지/호스트 증거를 재확보한다. 구 SHA의 PASS를 자동 승계하지 않는다.
7. 새 필수 근거를 강제하는 acceptance/publisher 검증을 검토·통합한 뒤 **별도의 최종 공개 승인**을 받는다.

10/23에 조건이 남으면 **BLOCKED 유지 및 명시적인 일정 재검토**다. 자동 공개·번호만 증가·자동 이월·AAX 제외·beta.1 만료일/공개 승인 승계는 수행하지 않는다. 이 감사와 Draft 수정은 main 병합이나 릴리스 승인이 아니다.
