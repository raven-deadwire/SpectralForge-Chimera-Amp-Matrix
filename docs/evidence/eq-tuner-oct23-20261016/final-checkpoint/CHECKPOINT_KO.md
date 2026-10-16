# EQ·튜너 10월 23일 통합 검증 체크포인트

**코드 결합 및 로컬 핵심 검사 완료 / 플랫폼 검증 대기 / 실제 DAW 미평가 / 병합·릴리스 보류.**

- 후보 Draft PR: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/90
- 실행 소스: `428d4b9890a015f1fe1545ad737e8ed2c57af9d9`
- 소스 tree: `3ce5d11ed43d49a965c4b80a9d140966d5bd83b8`
- #59/#86/#88/#68의 고정 최신 SHA 모두 이 후보의 ancestor임을 확인했다. 기존 네 PR은 head·Draft·미병합 상태 그대로다.
- 이 증거 브랜치는 소스 브랜치와 분리했다. 아래 결과는 증거 커밋을 실행한 결과가 아니라 위 실행 소스의 결과다.

## 확인한 통합 보존

제품 Source 112개는 #88과 동일하다. CMake의 기존 literal test 등록 73개, #68 최신 수명주기·매뉴얼·승인 구분 파일 10개를 보존했다. signal-path의 EQ editing 빌드와 tuner generation 선택식도 유지한다. 초기 결합에서 빠졌던 #68 문서 정정 c2263e4c는 최종 소스에 추가 부모로 통합했다.

기존 제품 파라미터 4,967개 + 신규 튜너19/다이내믹스40의 총 5,026개를 대상으로 검사기를 이식했다. 고정 VST3 ParamID 59개와 기존 제품 ordinal을 검사하고, 뒤따르는 JUCE wrapper ordinal의 정확한 +59 이동을 명시했다. 이 이식 자체의 부정 사례 검사는 통과했지만 실제 후보 바이너리 ABI·호스트 수용이 완료됐다는 뜻은 아니다.

## 같은 소스의 로컬 실제 실행

| 검사 | 결과 |
| --- | --- |
| source/registration/workflow 보존 감사 | PASS |
| 공개 beta.2 fixture 무결성 | PASS |
| legacy evidence Python | 5/5 PASS |
| normalization render 부정 사례 Python | 5/5 PASS |
| append-only ID·순서·값·기본값 부정 사례 Python | 4/4 PASS |
| 정밀 튜너 C++ | 정확도576 + stress18 + frozen kernel bit 비교1105 PASS |
| 튜너 timing decomposition C++ | PASS |
| beta lifecycle policy C++ | PASS |
| native CMake/JUCE configure | 환경 오류: PkgConfig 실행 파일 없음 |

GCC 13.3.0, C++20 -O2. compiler command·종료 코드·소요시간·바이너리/로그 SHA-256는 local-summary.json에 있다. **로컬 native 32-route/EQ/제품 CTest를 실행했다고 주장하지 않는다.** GUI/오디오 의존성 설치도 현재 실행 환경에서 불가능했다. 원시 configure 실패를 보존한다.

## 최종 소스의 GitHub Actions

- 전체 제품: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941848
- signal-path/튜너: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941615
- EQ 편집: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941707
- beta.2 상태/정규화: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906942295
- beta lifecycle: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38906941947

조회 체크포인트에서 총 12 workflow가 QUEUED/PENDING이며 완료 결과는 없다. raw workflow-runs.json을 보존했다. 각 workflow는 같은 후보 head를 checkout하고, 구성된 inventory·raw output·JUnit·LastTest·build identity를 실패 시에도 artifact로 남기도록 구성돼 있다. 초기 `098c7474`의 CI는 최종 소스의 근거로 승계하지 않는다.

입력 PR의 실패도 남아 있다. #86의 macOS legacy 5/7 및 full 49/51 실패는 finalEQ_b10_q 표현 비교와 후속 fixture 부재였다. #88의 macOS 집중 7/8 실패에는 303.869959ms settling과 resumed-after-expiry acquire=-1이 있었다. 이 후보는 제품 Source를 변경하지 않아 그 원인을 해결했다고 주장할 수 없다.

`release_approved=false`. Windows·macOS·Linux 최종 결과와 Studio One 우선 실제 DAW 프로젝트·automation Read/Touch/Latch/Write·Undo·IR/A-B·슬롯 재사용·종료 수용까지 보류한다. 기존 PR 병합·태그·공개 릴리스는 수행하지 않았다.
