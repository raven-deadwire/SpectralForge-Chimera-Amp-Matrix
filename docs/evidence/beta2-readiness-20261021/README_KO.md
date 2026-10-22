# beta.2 후보·조건부 공개 검토 — 2026-10-21 KST

**판정: BLOCKED. 10/22는 후보 준비 상태 검토, 10/23은 차단 해제 및 일정 재산정 시점이다. 현재 근거로 10/23 개발 완료 또는 공개를 확정할 수 없다.**

PR #93의 기존 `Tools/issue97_gate.py` → `weekly_publication.evaluate()` →
`publish_release.py` 경로를 그대로 사용한다. 새 게이트·우회 승인·음향 기준을
추가하지 않는다. `readiness-review.json`은 기존 27개 보고서 ID에 연결한
감사 색인이며, 실제 수용 보고서나 `issue97/manifest.json`이 아니다.

## 기준과 소스

10:50 KST 조회 스냅샷. 전체 run 조회는 SHA별 `per_page=100`을 사용했고
각 응답의 `total_count`와 반환 개수가 일치했다. 조회 후 CI 상태는 바뀔 수
있으며, 공개 시 기존 게이트가 같은 소스의 최신 run/attempt/artifact를 다시 검증한다.

| 대상 | 확인한 SHA | 구분 |
| --- | --- | --- |
| main | `4aeb3033d15e04dbaa785811f4faf95db72882a9` | #108 프리셋 유지보수 통합. 네 P0의 beta.2 통합 소스가 아님 |
| #93 기존 게이트 | `153f1a6a0400ad38287bbd59030e06dd794fb5cb` | 감사·게이트 기준, 제품 후보로 선택하지 않음 |
| #98 | `b7923ea8b7a0f85cfbfafb0e0338238adc09b181` | 잔여시간/수명주기 구현, Draft·미병합 |
| #100 | `6ec78c475311827ccfd1949c9ae9ce364f79c0b4` | 이전 Transpose 독립 연구 |
| #112 | `650feaa8955281232fc8c840bd82afdeda5df7e0` | #100 위의 corr40-split 독립 연구, Draft·미병합 |
| #101 | `8ff4dbf59639f54c751232999da6e78055691703` | AAX 개발 및 #106 복구 수정 통합, Draft·main 미병합 |
| #110 | `3a942a125d652a234d0f481fca3af2bdf6897635` | 기능별 화면·4밴드 DSP 구현, Draft·미병합 |
| #111 | `ffab682bef3468001f98624bda3051796341ab51` | 기각 실험 이후 **실제 Source를 수정한 후속 후보**, 검증 진행 중 |

이들 feature HEAD는 확인한 main의 조상이 아니다. main과 #93의 제품 소스도
같지 않다. #108 유지보수·튜너 테스트 변경을 이 Draft로 임의 병합하지 않았다.
소스 트리·파일별 blob/SHA-256은 `source-status.json`, `source-files.json`에 있다.

현재 공개 제품 태그는 `v1.3.2-beta.1`, 원래 제품 소스는
`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`이다. 별도 프리셋 유지보수 릴리스
`presets-2026.10.19.1`의 소스 `50e7836011cac9a007fe29b078cd983309fca068`도
확인했다. 이 유지보수 공개·승인으로 beta.2 P0나 만료일을 승인하지 않는다.

## 네 P0별 구현과 미충족 근거

### P0-1 Transpose

기존 제품 Transpose와 독립 연구 실행 파일을 구분한다. #112는 생산
`Source/`, root CMake, 제품 파라미터/상태/PDC를 바꾸지 않는다.

48kHz·128 samples·−2반음의 **합성 작은 어택 고역 peak 추가 지연**은
B0 2.750ms, A0 2.688ms, A1 2.667ms다. #100의 B0 34.604ms를 최신 후보의
작은 고역 어택 결과로 계속 인용하지 않는다. 다만 다음 실패가 남아 있다.

- 저역 추가 에너지 중심 지연 10.686–16.877ms.
- 상향 변환의 기본 어택 최대 37.687ms.
- 작은 0.001 A1 marker 6/16 검출 누락, 최대 30.188ms; holdout 검출도 95/96.
- 가변 지연/고정 PDC 미구현, 대역 정렬·착색 미검증. block64 CPU/wall deadline miss 1회.
- 실제 동일 DI/Azhi Dahaka/Low B 비교, Neural DSP 원본 캡처, 실제 DAW 수용 미확보.

구현 소스 `979c13d65e1cf42fb21ecbca6b8c39b434da0111`, 측정 tree
`cae306afa1d77eb431f11d7550153e6b7bbe0ff5`, 보고서 HEAD `650feaa8…`를
구분한다. 고역 peak 수치를 전체 알고리즘 지연·PDC·왕복 지연으로 바꾸지 않는다.
**48kHz ±1/±2 전체 추가 지연 ≤10ms 요구는 미달이며 P0 BLOCKED다.**

### P0-2 FabFilter·멀티밴드

#110의 여섯 화면과 실제 IN/OUT 분석, 독립 4밴드 DSP는 해당 개발 브랜치에
구현됐다. 원본 FabFilter 캡처·응답/검출/시간/위상 비교 및 검토된 수치 기준,
실제 DI 청취·호스트 수용과는 별도다. 기본 제품에 있던 EQ/Dynamic EQ/컴프레서/
게이트/샘플 피크 리미터의 존재만으로 이 P0를 통과시키지 않는다.

**#111은 두 단계를 반드시 분리한다.**

1. `1669a805f67c55acf4e3600172befd456455c7e0`의 독립 allpass 실험과
   `0123668f74c7e61c84f209d91eaa54df864786b6`의 보고서: 700Hz OFF 감쇠
   −28.645→−0.483dB, cold ON −59.385→−0.052dB 개선. 그러나 저역
   최대 +6.547dB, 32/288조건 실패로 **REJECT_CANDIDATE**. 이 실험은
   생산 소스를 바꾸지 않았고 #110 Global ON/OFF를 해결한 것이 아니다.
2. 이후 `63a72f4ac3551b76d4eb204c699469d23c1bde4e` 및 최신
   `ffab682bef3468001f98624bda3051796341ab51`: `Source/MultibandDynamicsCore.h`
   와 `Source/CommonDynamicsPanel.h`를 실제 수정했다. 고정 상태의 최소 위상
   합성·전환 및 샘플레이트별 제어 시간 보정 후보다. 최신 전용 run
   **39529062371**은 조회 시 대기/실행 중이다. 따라서 PR 본문의
   “생산 Source 불변” 설명을 최신 HEAD에 적용하지 않는다. **새 소스는 구현됨/
   검증 대기**, 기존 실험은 **기각**으로 기록한다. 이전 실험의 실패나 성공을
   새 구현의 수치 결과로 전용하지 않는다.

후속 PR 본문에는 새 SHA의 로컬 224 impulse pairs/1,728 transitions PASS,
정적 magnitude 차이 <0.000178dB 및 1,222,740 paired CPU callback 측정이 보고됐다.
이는 **작성자 보고**이며 이 감사에서 최종 원자료를 재검증한 결과는 아니다.
본문도 이후 새 구현을 설명하도록 갱신됐다. 활성 위상이 달라졌고 공유 환경의
마감 초과·실제 DI/설치 DAW 미검증이 남는다. 새 수정안의 통합 승인이나
음향·호스트 수용은 아직 없으므로 P0 BLOCKED다.

### P0-3 베타 수명주기

#98에 정확한 남은 일·시간·분·초 표시와 정책/화면/패키지 대조,
만료 상태의 합성 후속 빌드 복원이 구현됐다. lifecycle run **39073751028**은
7/7 job SUCCESS다. 과거 본문의 “진행 중”은 이 결과로 갱신한다.
실제 설치 DAW에서 만료·데이터 회수·업데이트 복원은 확인되지 않았다.
beta.2 전용 승인 만료일·정책·바이너리·안내의 일치도 확보되지 않았다.
beta.1의 2027-02-11 23:59 KST를 beta.2로 승계하지 않는다.

### P0-4 AAX Native

#101에는 공통 DSP AAX 타깃, Host Bypass 지연 보상, #106의 설치 복구 수정이
있다. 최신 소스 run **39245173843**의 Windows/macOS 개발 빌드 모두 SUCCESS,
원본 로그에서 각각 CTest 5/5를 확인했다. 산출물 이름도
**AAX-UNSIGNED-DEVELOPMENT**, 버전은 beta.1이다.
PACE·최종 플랫폼/설치 서명과 일반 Pro Tools의 Windows x64, macOS Intel,
Apple Silicon 수용 자료는 확보되지 않았다. AAX의 기존 제품 PDC 검사는
#112의 가변 지연 후보가 통합·검증됐다는 뜻이 아니다.

## CI·패키지의 실제 확인 범위

| 소스 / run | 최신 확인 | 공개 판단에 남는 제약 |
| --- | --- | --- |
| #93 / 39411898532 | publisher 계약 5/5 SUCCESS | 제품 또는 DAW 수용 아님 |
| #93 / 39411898520 | validation-contract FAIL, build/assembly SKIPPED | 임시 `.git` 정리 `OSError: Directory not empty`; strict **0 PASS / 128 BLOCKED**, native 미실행 |
| #93 / 39411898510 | Windows candidate job SUCCESS | 세 OS beta.2 통합 후보가 아님 |
| #110 / 39398055113 | 전체 build/assembly SUCCESS. 실제 JUnit Windows **67/67**, macOS **59/59**, Linux **60/60**, 0 failure/skip | 패키지 beta.1, `published:false`, 서명/공증 false; strict **21 PASS / 107 BLOCKED** |
| #110 / 39398055159 | macOS tuner FAIL | 다른 native/UI PASS로 상쇄하지 않음 |
| #110 / 39398055143 | macOS CAB panel 18/19, FAIL | 96k/64/stereo: wall p99 **806.208µs > 666.667µs**, miss **19/1600** |
| #101 / 39245173849 | 전체 build/assembly SUCCESS. JUnit Windows 66, macOS 59, Linux 60, failure/skip 0 | beta.1, strict **21 PASS / 107 BLOCKED**, 별도 signal-path FAIL |
| #112 / 39525516597, 39525516420 | 전체 build / Windows candidate 실행 중 | 완료·수용으로 계산하지 않음 |
| #111 / 39529062371 | 새 구현 전용 numerical/native 검증 대기·실행 중 | #110 또는 이전 #111 결과를 새 SHA에 승계하지 않음 |

아카이브 4개를 실제 다운로드하여 API의 크기·SHA-256, 내부 release-gate/
candidate-source/CTest 파일을 대조했다. 식별자·해시와 추출한 판정 원문은
`artifact-verification.json` 및 `raw/`에 보존한다. 전체 배포 설치파일 자체나
실제 설치 후 서명·호스트를 이번 감사에서 검증한 것은 아니다.

#93 Windows integration artifact **12263847903**의 다운로드는 API 크기보다
짧고 유효한 ZIP이 아니어서 증거로 제외했다. job 성공만으로 누락된 바이트 검증을
대체하지 않았다. 다른 세 소스의 작은 strict gate 아카이브는 정상 검증했다.
어떤 아카이브에서도 새 beta.2를 수용하는 `issue97/manifest.json`은 확인되지 않았다.

## 시스템 요구사항의 별도 연구

#102 `274d9c383ca34a2bd66b59921d836231f55d36ff`는 사양 후보 문서이며
#98 위에 쌓여 있다. #103 `ea63851f55e0ed882d400f0b5353d31161b1908a`는
실제 `ChimeraProcessor::processBlock()`을 호출하는 전체 그래프 호스트를 구현했다.
PR에는 Windows 48k/128/4x/Matrix 4인스턴스/Transpose −2에서 60초 합성
오디오 처리에 130.489초, 22,500/22,500 예산 초과가 보고되어 있다.
이 감사에서 그 로컬 원자료 ZIP을 재측정하지 않았으며 **작성자 보고**로 구분한다.
전용 계측 계약 run 39456022585는 SUCCESS지만 제품 build 39456022564,
update/publisher workflow는 FAIL이다. 독립 프로세서 호스트·GUI 닫힘·합성 입력은
설치된 DAW/물리 장치/30분 재생·녹음/세션 재열기 근거가 아니다.
`requirements_certified=false` 및 공통 시스템 수용 BLOCKED를 유지한다.

## 기존 게이트 연결 및 실행

`readiness-review.json`의 P0별 `required_report_ids`와 공통 integration/native_daw/
system ID가 기존 inventory의 27개를 빠짐없이 참조한다. 요구사항 fingerprint는
`3e2e01d2981a500a809516b6c0d55e57c9a724125d5afb77a48ae20fad9460e9`로 불변이다.
연구 스냅샷을 수용 manifest에 넣거나 `synthetic:false`로 바꾸지 않았다.

실행: `python Tools/issue97_gate.py --version 1.3.2-beta.2`

결과는 종료코드 1 / **BLOCKED**, `source_sha:null`, `publication_allowed:false`다.
원문은 `current-readiness.json`이다. 통합 소스·패키지·실제 증거·검토된 승인이
없다는 기존 차단을 그대로 유지한다. 계약 테스트 결과는 `local-tests.json`에
별도 기록하며 제품 수용 자료로 사용하지 않는다.

#93의 CI 임시 Git 폴더 정리 실패에 대응해 **테스트 fixture에서만** 자동 GC와
maintenance를 비활성화했다. 임시 폴더를 지우는 동안 백그라운드 Git이 쓰지
않도록 격리하며 cleanup 예외를 숨기거나 실제 정책/검사 기준을 낮추지 않는다.
원본 로그만으로 모든 경합 원인을 확정한 것은 아니며, 원격 새 SHA CI 결과는 별도다.

## 10/22·10/23 운영 판단

- **10/22 후보 검토:** 단일 통합 제품 SHA, 변경별 코드 연결, beta.2 전용 정책과
  실제 후보 산출물이 없는 상태에서는 후보 동결 완료로 표시하지 않는다.
- **10/23:** 현 근거로 개발 완료·공개 일정은 **재산정 필요**다. 기존 blocked
  슬롯은 증거 재검토 시점으로 보존하며 공개 약속으로 해석하지 않는다.
- 검토 시에도 P0 또는 필수 증거가 미충족이면 미달 원인·남은 작업·외부 의존성을
  기록하고 기존 일정 계약에 따라 활성 날짜를 비우는 deferred/remap 변경을
  명시적으로 수행한다. 다음 날짜는 구현·실측·서명/호스트 확보 계획을 근거로
  정하며, 지금 근거 없이 10/30로 자동 이월하거나 beta.3를 소비하지 않는다.
- 네 P0, strict baseline, 같은 소스의 실제 패키지/호스트 자료, beta.2 만료일 및
  별도 공개 승인이 모두 충족되어야 기존 publisher가 공개 가능성을 평가한다.

일정 검사 fixture의 10/19 고정 시계를 현 snapshot 날짜로 맞추고, 재예약 검사만의
합성 10/20 날짜를 아직 미래인 10/24로 이동했다. 실제 예약과 마감 기준은 불변이다.

이번 변경은 #93의 감사/일정 검토 근거와 fixture 격리만 갱신한다.
main 병합, 다른 feature 브랜치 변경, 버전/만료일/승인 정책 수정, 태그·릴리스·
업데이트 피드 변경을 수행하지 않는다.
