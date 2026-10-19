# 프리셋 EQ·다이내믹 보완 — 독립 개발 후보

기준: main `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f` (2026-10-19).
대상: 기존 48개 공장 프리셋. 기존 PRE/앰프/CAB/POST와 게이트·크로스오버·LOW
blend, 프리셋 이름·ID·순서·출력 노브 0 dB를 보존한다. 사용자 프리셋/프로젝트를
불러올 때 새 설정을 덮어쓰지 않는다. 공장 프리셋을 명시적으로 선택할 때만 적용한다.

## 신호 경로와 변경

PRE → Classic/Dual/Matrix 앰프·CAB 합산 → 기존 게이트 → Tone EQ → 공통 COMP
→ 기존 POST → 공간/스테레오 유틸리티 → Final EQ → 출력 → 샘플 피크 리미터.

| 처리 | 적용과 목적 |
|---|---|
| Tone bell 1 | 프리셋별 160–400 Hz. 정적 0 또는 −0.5 dB, 신호가 커질 때만 최대 1–2 dB 억제. 저역 기음에 HPF를 더하지 않음 |
| Tone bell 2 | 800–2200 Hz의 넓은 +0.5 dB. 이미 해당 POST 강조가 있는 Original/Niflheimr 일부는 비활성 |
| Final shelf | 필요한 프리셋만 −0.5 dB. 기존 CAB 필터와 중복되는 가파른 LPF를 추가하지 않음 |
| Final dynamic bell | 2600–4400 Hz, 정적 0 dB, 최대 1–2 dB 억제. POST 이후의 거친 상단만 제어 |
| 공통 COMP | Bell Clean, Edge Chime, Classic Crunch, Melodic Lead, G+G Clean / Crunch, G+B Air / Weight, Silk Smooth Lead의 7개만 사용. PRE/POST/Matrix LOW 압축이 활성인 프리셋에는 추가하지 않음 |
| 공통 COMP 설정 | 1.4:1–1.5:1, attack 25–35 ms, 병렬 mix 35–50%, makeup 0 dB. detector HP만 사용하며 오디오 저역은 자르지 않음 |
| 출력 리미터 | −1 dBFS **sample peak**, fuzz release 180 ms / 나머지 100 ms. true peak나 lookahead 리미터가 아님 |

48개 레시피의 정확한 주파수·Q·threshold·range는
`Source/FactoryPresetFinishing.h`에 있으며, UI의 기존 컨트롤로 확인·수정할 수 있다.
Dynamic EQ 네 슬롯 중 두 개만 할당한다. 각 threshold는 Tone/Final의 실제 단계
레벨 기준이며 입력 DI의 dBFS와 같다고 가정하지 않는다. 원본 소리·장르에 대한
청감 승인은 아직 이루어지지 않았다.

`signal-path-audit.csv`는 각 프리셋의 원래 경로, PRE/POST/LOW 압축, POST EQ와
새 공통 COMP 적용을 기록한다. 정적 EQ는 작게 제한했지만 기존 POST와 합쳐진
톤의 적절성은 실제 DI 청음에서 최종 판단한다.

## 호환성과 P0 분리

네 snapshot 생성 함수에 동일한 후처리를 연결한다. DSP 구현, 5,026개 파라미터의
ID·순서·범위·기본값·자동화 속성, 상태 schema, preset metadata ID는 바꾸지 않는다.
새 EQ 밴드의 occupancy는 기존 factory RestoreScope가 enabled 상태에서 복원한다.
공장 프리셋 전환은 기존대로 Undo/Redo 경계이며 이전 프리셋의 밴드가 남지 않는다.

#97의 Transpose, FabFilter 엔진 연구, 베타 수명주기, AAX 제품 소스와 배포 정책은
수정하지 않는다. #98/#100/#101/#103 변경 경로를 대조했고, 유일한 공통 파일인
CMakeLists.txt에는 기존 integrated test 바로 뒤에 테스트 등록 3줄만 추가한다.
별도 개발 PR이며 main 병합·태그·공개 패키지·updater 변경을 실행하지 않는다.
이전 작업 공간의 레시피·회귀검사 초안을 재사용하고, PRE/LOW 중복 압축 제거,
게이트 보존, 음량 정렬 비교·트랜지언트·실제 preset automation 회귀를 추가했다.
자동 배포·프리셋 pack publisher 초안은 포함하지 않는다.

## 자동검증 계약

- 기존 48개 preset nominal / 입력 +6 dB pluck, peak < 0.95와 RMS > −30 dBFS 기준 유지.
  **리미터를 끈 상태에서도** 같은 검사로 gain staging을 확인한다.
- 변경 전/후: 모든 레시피의 새 EQ/다이내믹 파라미터를 등록 기본값으로 되돌린
  상태가 기존 main snapshot과 동일하다는 변경 범위 검사를 선행한다. 같은 DSP에서
  같은 합성 pluck를 렌더한다. 이는 구버전 설치 바이너리 대조 시험은 아니다.
- 원래 음량 대비 RMS 변화 ±1.5 dB 이내. 오프라인 RMS 정렬 후 crest factor 및
  알려진 각 pluck 시작 20 ms 창의 peak 변화 ±1.5 dB 이내. 이 창은 실제 악기
  onset/피킹 지연 추정치가 아니며 모든 연주 기법의 트랜지언트를 대표하지 않는다.
- 실제 binary 저장/복원 오디오 최대 오차 < 1e−6. 저장된 이전 설정 복원 시
  모든 파라미터 유지. 프리셋별 지연 동일, 공통 COMP와 PRE/POST/LOW 중복 없음.
- 대표 5개 프리셋 × 44.1/48/96 kHz × mono/stereo = 30개 경로에서 6개 EQ·dynamic·
  compressor·limiter 파라미터를 자동화. 출력 유한성/피크, 복원 후 같은 자동화의
  샘플 일치, host begin/end gesture 균형, 지연 유지 확인.
- 기존 EQ DSP/UI/편집, CommonDynamics 및 GateProcessor의 파라미터·상태·자동화
  계약도 실행한다. 기술 회귀 결과를 실제 DAW나 청감 통과로 표시하지 않는다.

## 청음·실제 호스트 확인 항목

1. 실제 기타/베이스 DI로 클린 약음·강음, 하이게인 팜뮤트, 멜로딕 리드, 데스 리듬,
   슬램 저음 반복, 둠 긴 sustain을 음량 정렬하여 비교한다.
2. Low B 및 A, 피크가 큰 slap/pick에서 Dynamic EQ가 상시 컷으로 굳지 않는지,
   충분히 복귀하는지, 중역 존재감/클랭크를 과도하게 지우지 않는지 확인한다.
3. 재생 중 프리셋 전환, Studio One 실제 automation Read/Touch/Latch/Write,
   프로젝트 재열기, 장시간 녹음/재생 및 Windows/macOS 설치본을 확인한다.
4. `*-before.wav` / `*-after-matched.wav`는 48 kHz 합성 fixture 참고 페어다.
   두 파일에 공통 headroom pad를 적용하고 RMS를 맞추며 실제 DI 청음 증거로 쓰지 않는다.

실행 결과와 한계는 `docs/evidence/preset-finishing-20261019/RESULTS_KO.md`에 기록한다.
