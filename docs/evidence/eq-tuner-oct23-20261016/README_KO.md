# 10월 23일 EQ·튜너 통합 후보

개발 목표: **1.3.2-beta.1**, 조건부. `release_approved=false`.
기존 #59/#86/#88/#68을 변경하거나 병합하지 않는 별도 Draft 후보다.

## 입력과 결합

| PR | 고정 head |
| --- | --- |
| #59 EQ 편집 | `7ef99333ce14315538ec4aed350d802ca0d78e23` |
| #86 EQ 정규화 | `52aadac80c65082e8d50356110eaca29e6bce9ba` |
| #88 튜너 통합 | `0f859400abfb944fc99ea9cfab842c77a6851a50` |
| #68 릴리스 기반 | `c2263e4c90bffb9a56cc41285ebf8207e641b3aa` |

#88은 #68을 포함한다. #86은 #59의 후속 이력을 포함한다. #88에서 별도 브랜치를 만들고 #86 전체 이력을 두 번째 부모로 결합한다. CMake의 CRLF/LF 차이를 정규화해 검토한 뒤 **양쪽 테스트 등록 블록을 모두 보존**했다. signal-path.yml은 #88의 EQ 편집 빌드와 세대 관리 검사 선택식을 그대로 유지한다.

충돌한 GraphicalEQEditingTests, GraphicalEQUITests, SignalPathUITests는 #59의 canonical 값·두 EQ 뱅크·빈/점유 슬롯 검사를 #68의 신호 경로·다이내믹스 검사와 함께 유지한다. 기존 UI 중립 gain의 더 좁은 의도 검사도 보존한다. 제품 `Source/`는 #88과 바이트 동일하다.

## 호환성 검사의 통합 이식

기존 4,967개 제품 파라미터 뒤에 #68의 튜너 19개와 다이내믹스 40개가 추가돼 총 5,026개다. #86의 총수 4,967 고정 검사를 이 후보에 맞게 이식한다. 기존 슬롯 순서·ID·범위·기본값 검사를 유지하며 전체 5,026개 raw/normalized 상태의 반복 복원과 신규 기본값을 검사한다. Undo 독립성은 EQ 외 기존 4,845개와 신규 59개를 모두 포함한다.

VST3 비교는 기존 제품의 4,967개 index/ParamID를 고정한다. 제품 뒤에 있는 JUCE Bypass/MIDI wrapper의 **목록 index는 59만큼 이동**하며 numeric ParamID·상대 순서·메타데이터는 그대로 요구한다. 신규 59개는 이름으로부터 고정한 ParamID/순서를 정확히 검사한다. normalized 비교는 기존 값 전부의 완전 일치와 신규 기본값을 요구한다. 오디오 비트·XML 상태·개인 IR·A/B 조건은 완화하지 않는다. 실제 DAW automation 수용을 대신하지 않는다.

eq-legacy-state workflow도 1.3.2 qualification 빌드로 설정하여 나머지 집중/전체 제품 workflow와 동일한 소스·제품 정체성을 사용한다. 공개·배포용 전환은 없다.

## 입력 PR에서 확인한 실패

- #59의 문서 head에는 실행이 없으며 이전 `babfecda995141ef3001fc6f9c04c05409ff7ad6`의 결과는 그 소스의 기록이다.
- #86 `52aadac`: graphical-eq 3 OS 성공. eq-legacy-state macOS는 `finalEQ_b10_q` stored/canonical 비교 실패와 그 후속 fixture 부재로 **5/7 PASS, 2 FAIL**. signal-path Linux는 CabPanelState 실패. 전체 제품 macOS도 실패. 원래 실패를 삭제하거나 허용 오차로 덮지 않는다.
- #88 `0f859400`: 집중 튜너 Windows/Linux 성공, macOS **7/8 PASS, 1 FAIL**. 원시 로그에는 44.1k/256/440Hz settling **303.869959 ms**와 resumed-after-expiry `acquire_ms=-1`가 있다. 같은 job의 별도 resume 검사 성공이 이 실패를 취소하지 않는다. 초기 조회 시 전체 제품 Windows/Linux 성공, macOS 진행 중이었다.

`prior-ci/`에 조회한 PR·workflow/job 메타데이터와 실패 job 원문을 보존한다. 해당 결과는 통합 소스의 PASS가 아니다.

## 검증 및 판정

`Tools/check_eq_tuner_integration.py`는 입력 PR의 CMake 등록 합집합, 제품 소스 무변경, workflow head checkout·실패 업로드, 실제 구성된 CTest의 중요 검사 존재를 확인한다. 전체 제품 CI는 CTest inventory/원시 로그/JUnit/제품 provenance를 실패 시에도 업로드한다. 집중 EQ/정규화/튜너와 전체 제품은 같은 후보 head를 checkout한다.

최종 실행 SHA·원시 로그·CI URL은 이 Draft PR의 실행 기록에 고정한다. 이 문서의 초기 체크포인트에서는 통합 네이티브 PASS를 주장하지 않는다.

실제 Studio One 우선: 구버전 프로젝트 열기/저장/재열기, Read/Touch/Latch/Write와 DAW Undo, 삭제·재사용 슬롯의 기존 automation lane, A/B·프리셋·개인 IR, 다중 인스턴스, 튜너 on/off·입력 중단·재개, 96/192kHz 및 실제 버퍼, 종료를 검증해야 한다. Cubase/Cakewalk 종료 수용도 별도다. 플랫폼 검증과 실제 DAW 수용 완료 전 **병합·릴리스 금지**.
