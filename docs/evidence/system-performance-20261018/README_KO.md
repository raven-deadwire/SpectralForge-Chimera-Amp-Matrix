# 시스템 성능 측정 체계 — 2026-10-18

**도구·스키마 구현 / 실제 Chimera 성능 측정 BLOCKED / 최소·권장사양 인증 미실시.**

이 변경은 이슈 #97의 성능 측정 요청을 구현한다. 기존 문서 Draft #102
(`274d9c383ca34a2bd66b59921d836231f55d36ff`, 부모 #98) 위에 쌓는 별도 Draft다.
공개 기준 제품 소스 `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`의 패키지·자동검사
근거는 #102에서 이어받되, 새로운 바이너리 성능 인증으로 승계하지 않는다.

## 구현한 범위

- 닫힌 JSON Schema 두 개: 측정 환경·제품·workload·조건·결과와 계측/재생·녹음 상태 receipt.
  CPU/RAM/OS/DAW/format/interface/driver/sample rate/buffer/oversampling/power와
  GUI·분석기·병렬 트랙·CAB 경로·프리셋/IR/입력 해시를 기록한다.
- 48 kHz·128/256 samples·4×의 핵심 36조건. Dual Crossover 12조건,
  GUI/분석기 12조건, 대표 재생/녹음 각 30분 2조건을 별도 추가한다.
- Windows x64, macOS x86_64/arm64, Linux x64와 형식별 13개 계획 슬롯.
  AAX는 Windows/macOS의 별도 목표이며 Pro Tools 버전을 임의로 지정하지 않는다.
- 표준 라이브러리만 쓰는 Python CLI: 환경 수집, 실제 실행 파일·DI·상태 bank·IR manifest
  SHA-256 고정, process RSS/working-set 수집, CSV import, 환경표 생성, 원자료 재계산.
- C++ callback recorder: 사전 할당, wall/thread CPU 계측, 전체 graph 경계 기록,
  종료 후 CSV 저장, overflow 수 집계. 제품에 자동 삽입되는 코드는 아니다.
- callback wall과 CPU의 p99/peak, budget 초과와 실제 scheduled deadline miss,
  host/device xrun·glitch를 분리한다. 미확보 counter는 null이다.
- raw 파일 해시·source/binary/settings/environment/workload identity, callback 연속성,
  측정 길이, 메모리 샘플 간격을 검증하고 결과를 다시 계산한다. 합성 측정은 실기 soak
  PASS로 승격할 수 없고 `requirements_certified=false`는 고정이다.

## 실제 확보한 환경

| 구분 | 관측 | 한계 |
| --- | --- | --- |
| OS | Linux 6.18.55 x86_64, glibc 2.39 | 공개 Ubuntu 22.04 제품 CI와 다른 환경 |
| CPU | 호스트 노출명 INTEL XEON PLATINUM 8573C | 물리 워크스테이션 인증 아님 |
| 코어 | 논리 9 / affinity 9; cgroup CPU quota 800000/100000 | 9개 독립 물리 코어 보장 아님 |
| 메모리 | host-visible 10,451,435,520 bytes; cgroup 한도 8,589,934,592 bytes | 설치 RAM·제품 peak로 간주하지 않음 |
| 장치·호스트 | `/dev/snd` 없음; 실제 DAW·인터페이스·callback adapter 없음 | 재생·녹음·xrun·프로젝트 재열기 BLOCKED |
| 제품 | 다음 통합 Release 실행 파일 없음 | callback CPU/p99/peak/miss·제품 RAM 모두 미측정 |
| 메모리 collector smoke | executor PID와 `/proc` namespace가 일치하지 않음 | own-process RSS smoke SKIP; 다른 프로세스 수치를 제품 실측으로 저장하지 않음 |

원문: [local-study.json](local-study.json),
[자동 생성 환경·62조건 표](environment-and-results.md),
[13개 OS/형식 계획 슬롯](../../../Validation/system-performance/target-environments.json).
62개 로컬 제품 조건과 13개 계획 슬롯은 모두 구체적인 사유가 있는 BLOCKED다.
장비가 준비되면 새 study의 조건은 NOT_RUN으로 시작하며 완료된 관측만 MEASURED로 바뀐다.
MEASURED는 성능 합격이나 사양 인증을 뜻하지 않는다.

## 검증과 재현

정확한 실행 로그는 [local-validation.txt](local-validation.txt),
검증한 파일 해시는 [validation-manifest.json](validation-manifest.json)에 보존한다.

```sh
python3 -m unittest discover -s Tools/SystemPerformance -p 'test_*.py' -v
g++ -std=c++17 -O2 -Wall -Wextra -Werror \
  Tools/SystemPerformance/test_recorder.cpp -o /tmp/collector-test
/tmp/collector-test /tmp/collector-only.csv
python3 Tools/SystemPerformance/performance.py validate \
  docs/evidence/system-performance-20261018/local-study.json
```

Python 검사는 입력 오염·stale identity·자료 위변조·조건 누락·합성 soak 오인 방지를
확인한다. C++ 검사는 실제 OS clock 호출, 8개 보존 행/1개 overflow 집계,
계측 구간 C++ new 0회를 확인한다. 이 수치는 Chimera 처리량이 아니다.
별도 Windows/macOS/Linux collector CI는 도구의 이식성만 검사한다.

실기 DAW에 사용할 callback export adapter는 아직 없다. 제공된 header는 adapter
구현에 필요한 계측부이며 실제 호스트 연결은 별도 의존성이다. 메모리 sampler는 준비된
호스트 PID에 실행할 수 있지만, 메모리만 확보한 경우 원자료만 보존하고 전체 callback
측정을 완료로 표시하지 않는다. 다음 빌드·실기·adapter가 없는 상태에서 가짜 측정이나
30분 무의미한 대기 작업을 실행하지 않았다.

## 문서와 배포 경계

README·INSTALLATION·SYSTEM_REQUIREMENTS·EN/DE/KO 매뉴얼과 다운로드 안내용
OPEN_BETA_RELEASE_NOTES 원문을 같은 미실측 상태로 맞췄다. 이 저장소에는 별도의 공개
다운로드 웹페이지 소스가 없으며 GitHub Releases로 연결된다. 공개 beta.1의 실제 릴리스
본문/자산은 변경하지 않는다. 이번 Draft가 병합되고 다음 배포가 확정될 때 원문을 반영한다.

Windows/macOS/Linux의 기존 OS/형식 자동검사와 패키지 확인 범위는 유지한다.
4코어/8 GB, 6–8코어/16 GB 이상은 실측되지 않은 권장 후보이며, 32 GB는 큰 세션의
여유 제안이다. 이 작업으로 인증 최소사양·최대 인스턴스 수·기존 release gate를 바꾸지 않는다.

실기 진행 절차와 metric 정의: [Tools/SystemPerformance/README.md](../../../Tools/SystemPerformance/README.md).
