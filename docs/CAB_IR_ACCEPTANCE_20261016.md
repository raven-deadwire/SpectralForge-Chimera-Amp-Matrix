# PR #85 CAB·IR 직접 회귀검사 보강

기준: PR #85 `faeae1619c63c2fce3180419e777f7451d4173d8`의 승인 원장
`Validation/accepted-baseline.json` (SHA-256 `457260441cd5000c2b82abaa3b4c75d7262c5a5d9187b1615ad6e34a8938dda5`).
원장의 정의·hard gate·만료 정책·공개 승인은 변경하지 않는다.

## 기존 근거와 추가 검사

| 수용 기준 | 재사용한 기존 동작/검사 패턴 | 추가 실행/통과 조건 | 실패 조건 |
|---|---|---|---|
| A2.07 | `IntegratedProcessorTests::factoryCabContract`, `FactoryPresetTests`의 modeled-CAB·빈 private target/hash | Crom Cruach·Wild Hunt·Azhi Dahaka 각각 과거 파일명 부재/유효 합성 파일/손상 파일의 3상태에서 동일한 호스트 파라미터·유한 비무음 오디오(최대 오차 1e-7), 모든 활성 lane은 호환 layout/driver 및 모델링 Mic A | private target/hash·User IR 자동 로드·레시피/파라미터/출력 차이 |
| A5.01 | 공장 레시피, Wild Hunt 현재 모델링 경로 | 위 3상태에서 LOW 4x10/Vector/Anchor, MID 4x10/Alloy/Spear, HIGH 4x12/Nocturne/Needle 고정 | 레시피 불일치, 파일 의존성 |
| A6.01 | 공장 레시피, Azhi Dahaka 현재 모델링 경로 | 위 3상태에서 LOW 6x10/Foundry/Anchor, MID 6x10/Foundry/Hammer, HIGH 4x12/Steel/Needle 고정 | 레시피 불일치, 파일 의존성 |
| A5.05, A6.05 | `IRCollection` 검색/메모리 검증 및 `IRLibrary::decode`의 기존 크기·채널·레이트·길이·샘플 제한 | 20종 입력을 세 검증 경로와 실제 목록 `ready()`에서 대조. 허용 경계 positive control 5종, 거부 15종. NaN/+Inf/-Inf는 두 번째 채널 1025번째 샘플에 직접 IEEE float로 기록 | 거부 입력 loadable, 양수/음수 경계 허용값 거부, 검증 경로 불일치, 이유 없는 오류 |
| A8.02 | `IRLibraryTests`의 기존 embedded bytes·오디오 동일성 패턴, `loadMicIR` 실제 제품 API | 정상 개인 IR을 모든 6개 슬롯에 로드한 두 실제 processor를 연속 처리. 각 슬롯에 missing/corrupt/silent(18회) 시도 후 오류·파일명·host 값·binary state·active generation 유지, 64개 연속 블록 오디오 대조(최대 오차 1e-7, 유한·비무음). 원본 파일 보존 | 오류 없는 로드, 정상 상태/오디오/커널 교체, 손상 파일 ready, 원본 파일 변경 |

### 입력 경계

- 허용: mono/8 kHz/8 samples, stereo/384 kHz/1 s, 정확히 4 MiB, +32/-32 샘플.
- 거부: 4 MiB 초과, 44 bytes 미만, 손상 헤더, 무음, 7999/384001 Hz, 0/3 channels, 7 samples, 1 s 초과, NaN/+Inf/-Inf, ±32 바로 바깥 값.
- WAV는 테스트가 직접 생성한다. 과거 파일명은 `docs/reference/ir-catalog.json`에서 가져오며 원래 hash와 다름을 확인한다. 실제 개인 Hartke/Marshall/기타 캡처는 사용·수정·재배포하지 않는다.
- 공장 fixture는 실제 `IRCollection::roots()`의 사용자 IR 루트 내 고유 하위 폴더에만 생성한다. 기존 파일이 있는 환경에서는 작업을 중단하며 개인 폴더/설정은 덮어쓰지 않는다. 종료 시 소유한 임시 폴더만 제거한다.

## 증거 소비와 승인 경계

`ChimeraCabIRAcceptance_factory`, `_boundaries`, `_preservation`은 실제 제품 라이브러리에 연결한 CTest이다. `Tools/cab_ir_acceptance.py`는 JUnit 실행 상태·inventory·각 개별 사례와 완료 marker를 함께 확인한다. 누락·중복·스킵·실패·다른 suite의 출력은 통과가 아니다.

집중 workflow `CAB and IR negative acceptance`의 Windows/Linux/macOS 산출물은 **component 결과**다. 이 artifact를 Product 결과로 소비하지 않는다. Product consolidator는 원래 검증된 동일 commit/run/attempt의 native bundle에서 세 플랫폼 모두 해당 검사를 실행·통과했을 때만 여섯 ID를 갱신한다. SHA·원본 파일 해시 검증은 기존 `release_evidence.read_bundle`을 그대로 적용한다.

테스트/CMake 추가 때문에 PR #85의 엄격한 역사적 source fingerprint는 불일치한다. 그 보호를 완화하거나 새 파일을 임의 제외하지 않는다. 따라서 이전 37개 historical + 2개 distribution 재사용 항목은 새 브랜치에서 다시 BLOCKED이며 기존 61/25 또는 통합 fixture 82/46을 이 브랜치에 승계하지 않는다. `test_accepted_baseline.py`는 변경 소스에서 이 보수적인 판정을 검사하도록 수정했다. 나머지 항목의 새 native 근거 연결은 별도 작업이다.

제품 Source, 오디오 콜백, IR 보존 정책, host parameter ID, 프로젝트 형식은 수정하지 않는다. 이번 검사는 합성 오디오 회귀이며 실제 DAW 상태복원·개인 자산 인증·청음·오디오 품질 측정·최종 공개 승인을 대신하지 않는다. beta.3 `release_approved=false`, `expiry_approved=false`, 공개 게이트 BLOCKED를 유지한다.

## 재현

```sh
python Tools/test_cab_ir_acceptance.py
cmake -S . -B build-cab-ir -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-cab-ir --config Release --target ChimeraCabIRAcceptanceTests --parallel 2
ctest --test-dir build-cab-ir -C Release -R '^ChimeraCabIRAcceptance_' --no-tests=error --output-on-failure --output-junit cab-ir-results.xml
```

Linux에서는 기존 `Tools/run_linux_ui_tests.py -- <ctest command>`로 디스플레이를 관리한다. 실행 결과·고정 소스 SHA·실제 CI URL은 Draft PR에 기록한다. 미실행 상태를 PASS로 선기록하지 않는다.
