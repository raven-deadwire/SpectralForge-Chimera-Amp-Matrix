# 다이내믹스·EQ 청음 체크리스트 — #163 (2026-10-26 KST)

[#163](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/163)이 main에 넣은 변경을 실제 기타·베이스로 들어보고 판정하기 위한 목록입니다. 릴리즈 스케쥴의 "다이내믹스·EQ: 실제 기타·베이스 청음, 기존 세션 복원 확인" 항목에 해당합니다.

- 판정은 **듣는 사람의 PASS / FAIL / 보류**입니다. 자동 테스트 결과(합성 pluck 렌더, 레벨·GR 수치)는 이 체크리스트를 대신하지 않습니다.
- 이 체크리스트가 끝나도 DAW 자동화·호스트 수명주기 승인은 별도입니다.
- 근거 문서: [PRESET_DYNAMICS_EQ.md](PRESET_DYNAMICS_EQ.md)(프리셋 게이트·Expander·톤 커브), `Source/ExtraGates.h`(Gate 2·Expander), `Source/CommonDynamicsPanel.h`(Gate 탭), `Source/GraphicalEQPanel.h`(밴드별 DYNAMIC).

## 0. 준비

- [ ] **빌드 기록:** 테스트한 빌드의 커밋 SHA, OS, 포맷(VST3/AU/Standalone), 샘플레이트·버퍼를 기록한다.
- [ ] **비교 기준:** 공개판 1.3.2-beta.1(또는 프리셋 유지보수판 `presets-2026.10.19.1`)을 같은 세션에서 A/B할 수 있게 준비한다.
- [ ] **DI 클립 녹음:** 아래 소재를 DI로 한 번 녹음해 두고 재사용한다. 같은 연주로 비교해야 차이를 판단할 수 있다.
- [ ] **모니터링 레벨 고정:** 비교하는 동안 모니터 볼륨을 바꾸지 않는다. 출력 레벨 차이는 기록만 하고 귀로 보정하지 않는다.
- [ ] **악기:** 기타(6현 표준, 가능하면 Drop A/7현 Low B), 베이스(4현, 가능하면 5현 Low B). 픽업·톤 노브 위치를 기록한다.

### DI 소재

| ID | 악기 | 내용 | 주로 확인할 것 |
|---|---|---|---|
| S1 | 기타 | 단음·코드를 길게 울리고 자연 감쇠(4–6초) | 꼬리가 끊기지 않고 자연스럽게 사라지는지 |
| S2 | 기타 | 팜뮤트 리듬 → 갑자기 정지 → 2초 침묵 → 재시작 | 정지가 타이트한지, 침묵 중 히스, 재시작 첫 음이 깎이지 않는지 |
| S3 | 기타 | 레가토·해머링 리드, 약한 음 포함 | 약한 음이 게이트에 먹히지 않는지 |
| S4 | 기타 | 볼륨 노브 스웰, 아주 작게 치는 클린 아르페지오 | Expander가 작은 소리를 과하게 줄이지 않는지 |
| S5 | 기타 | Low B / Drop A 리프 | 저역 펀치·두께, Dynamic EQ가 저역을 뭉개지 않는지 |
| S6 | 베이스 | 핑거 롱노트·고스트 노트 | 고스트 노트 보존, 서스테인 꼬리 |
| S7 | 베이스 | 픽 / 슬랩 프레이즈 | 어택 보존, 하이엔드가 과하게 깎이지 않는지 |
| S8 | 공통 | 연주 없이 악기를 물린 채 10초(하이게인에서 험·히스) | 쉬는 구간 노이즈가 충분히 닫히는지 |

## 1. Gate 탭 — PRE RIG / POST RIG 동시 게이트

PRE 게이트와 POST 게이트가 함께 동작합니다. 기존 게이트 설정은 `gateAfterRig` 쪽에 남고(SWAP PRE / POST), Gate 2가 반대쪽을 맡습니다. 모든 게이트는 클린 DI로 감지합니다.

- [ ] **PRE·POST 동시 사용 (S2, S8):** 두 게이트를 모두 켜면 쉬는 구간이 한쪽만 켰을 때보다 조용하고, 재시작 첫 음의 어택이 깎이지 않는다.
- [ ] **SWAP PRE / POST (S2):** 누르면 메인 게이트 설정이 반대쪽으로 옮겨 가고, 소리가 예상한 방향으로 바뀐다(PRE: 앰프로 들어가는 히스 차단 / POST: 앰프 히스까지 차단). 눌러도 클릭·팝이 나지 않는다.
- [ ] **POST 게이트 지연 정렬 (S2, S3):** 하이게인·트랜스포즈 사용 중에도 POST 게이트가 음보다 늦게 열리거나 꼬리를 일찍 자르지 않는다.
- [ ] **연주 중 전환:** 재생하며 게이트를 켜고 끄거나 SWAP해도 끊김·잡음이 없다.

## 2. Gate 2 — Gate / Expander 모드

- [ ] **Gate 모드 (S2, S8):** 쉬는 구간을 닫는 정도가 threshold·range 설정과 맞다.
- [ ] **Expander 모드와 Ratio (S1, S4):** Ratio를 1.5 → 2 → 4로 올리면 작은 소리 감소가 점점 커지고, 낮은 비율에서는 꼬리가 부드럽게 줄어든다.
- [ ] **Range (S8):** Range를 줄이면 완전히 닫히지 않고 지정한 만큼만 줄어든다. "Full"은 완전히 닫힌다.
- [ ] **끈 상태:** Gate 2를 끄면 소리가 Gate 2 추가 전과 같다(기본값 꺼짐).

## 3. Expander 탭

- [ ] **배치 (S1, S2):** `Expander after rig`를 켜고 끌 때, 앞(PRE) / 뒤(POST) 위치 차이가 들린다.
- [ ] **꼬리 (S1, S4):** threshold 근처에서 꼬리가 자연스럽게 줄어들고, 게이트처럼 뚝 끊기지 않는다.
- [ ] **메인 게이트와 함께 (S1, S8):** Expander가 먼저 부드럽게 줄이고, 메인 게이트는 그 아래에서만 닫힌다. 꼬리 모양은 유지되고 침묵은 완전히 닫힌다.
- [ ] **Hold·Release (S2):** 짧은 정지 사이에서 펌핑·채터링(빠르게 열렸다 닫힘)이 없다.

## 4. EQ 밴드별 Dynamic (Tone EQ / Final EQ)

밴드에서 DYNAMIC을 켜면 공유 슬롯 4개 중 하나를 쓰고, threshold·cut·ratio·attack·release 줄이 나타납니다.

- [ ] **동작 확인 (S5, S7):** 문제 대역(예: 저역 붐, 2–4 kHz 거칠기)에 DYNAMIC을 걸면, 큰 음에서만 줄고 작은 음은 그대로다.
- [ ] **과감쇠 없음 (S1, S6):** 정적인 연주에서 소리가 계속 먹먹해지지 않는다(cut이 최대에 머물지 않는다).
- [ ] **4개 제한:** Tone·Final을 합쳐 5번째 밴드에 DYNAMIC을 켤 때, 다른 밴드가 몰래 꺼지지 않고 제한이 표시된다.
- [ ] **켜기·끄기:** DYNAMIC을 끄면 그 밴드는 정적 EQ로 돌아가고 소리가 튀지 않는다.

## 5. 팩토리 프리셋 (역할별)

#163은 프리셋마다 메인 게이트와 함께 **Expander 또는 Gate 2 중 하나**를 넣고, 역할별 톤 커브(Tone band 3·4, Final band 3)를 추가했습니다. 각 프리셋은 beta.1과 같은 DI로 A/B합니다.

| 역할 | 프리셋 | 추가 단계 | 들을 것 |
|---|---|---|---|
| rhythm | Tight Rhythm, Amber Heavy, Melodic Death Rhythm, G+G Tight / Wide, Feel My Wrath, Blackhearted, Dark Matters of Throne, Thall Rhythm, Rotten Grind, Slam Impact | 반대쪽 Gate 2(Expander 2:1) | S2: 정지가 beta.1만큼 타이트한지, 프레이즈 사이 히스가 줄었는지. 350 Hz 컷으로 저역이 얇아지지 않았는지 |
| clean | Clean Sustain, Bell Clean, Ambient Clean | Expander | S1·S4: 꼬리와 작은 음 보존, 9 kHz +1 dB 공기감이 거칠지 않은지 |
| edge | Edge Chime, Classic Crunch, G+G Clean / Crunch, Prism Edge | Expander | S1·S3: 크런치 꼬리, 2.2 kHz 프레즌스가 쏘지 않는지 |
| lead | Melodic Lead, Silk Smooth Lead, A Path To Alsatia, Molten Lead | Expander | S3: 약한 레가토 음 보존, 1.5 kHz 부스트가 코맹맹이 같지 않은지 |
| finger (베이스) | Finger Round, Vintage Bass DI, B+B Warm / Definition, Modern Clean | Expander | S6: 고스트 노트, 90 Hz 로우 셸프가 붐하지 않은지 |
| pick (베이스) | Pick Punch, Modern Grind, Pick Attack Matrix, Metal Clank | Expander | S7: 픽 어택, 1.2 kHz 부스트 |
| slap | Slap Studio | Expander | S7: 슬랩·팝 어택이 깎이지 않는지, 레벨이 너무 작지 않은지 |
| matrix | Bass Matrix, G+B Low Anchor, G+B Air / Weight, B+B Clean / Grind, Low B Foundation, Crom Cruach, Wild Hunt, Azhi Dahaka, Death Grind, Heavy Pulse, Slam Punch | Expander | S5: LOW 대역 DI·블렌드가 유지되는지, 1.8 kHz 부스트 |
| envelope | Filter Lead, Bass Envelope | Expander(range 9 dB) | 필터 스윕이 그대로인지(톤 커브 없음) |
| fuzz | Fuzz Texture, Wool Bass Fuzz, Spectral Texture, Sludge Mass, Doom Fuzz | Expander(range 9 dB) | S1·S8: 퍼즈 꼬리, 쉬는 구간 노이즈 |

- [ ] 역할마다 최소 1개 프리셋을 기타·베이스 각각 맞는 소재로 들었다.
- [ ] rhythm 프리셋은 전부 S2로 확인했다(정지 타이트함은 장르 핵심).
- [ ] **레벨:** 프리셋 간 이동 시 출력 레벨이 크게 튀지 않는다. 특히 Slap Studio(가장 작음)와 Amber Heavy·Thall Rhythm(가장 큼)을 비교한다.
- [ ] **리미터:** 입력을 +6 dB 올려 Amber Heavy를 칠 때, 리미터(−1 dBFS)가 들리게 찌그러뜨리지 않는다.

## 6. 기존 세션·프리셋 복원

새 파라미터(Gate 2·Expander)는 기본값이 꺼짐이라, 예전 세션은 소리가 같아야 합니다.

- [ ] **beta.1 프로젝트:** beta.1로 저장한 DAW 프로젝트를 새 빌드로 열면 게이트 동작과 소리가 beta.1과 같다(Gate 2·Expander 꺼짐).
- [ ] **사용자 프리셋·참조 파일:** beta.1에서 저장한 `.chimera`/참조 파일을 열면 값이 그대로다. 팩토리 프리셋을 다시 고르기 전까지 새 레시피로 바뀌지 않는다.
- [ ] **A/B 슬롯:** A/B 전환 시 Gate 2·Expander·Dynamic EQ 설정이 각 슬롯에 맞게 돌아온다.
- [ ] **개인 IR:** 세션에 넣어 둔 IR이 다시 열어도 유지된다.
- [ ] **자동화(참고):** 게이트 threshold·Expander ratio 자동화를 재생해도 잡음 없이 따라간다. DAW 승인은 별도 항목이다.

## 7. 기록 양식

| 날짜 | 빌드 SHA | 포맷/DAW | 항목 | 프리셋·설정 | 소재 | 결과 | 메모(무엇이 어떻게 들렸나) |
|---|---|---|---|---|---|---|---|
| | | | | | | PASS / FAIL / 보류 | |

- **FAIL:** DI 클립, 프리셋 이름 또는 저장한 `.chimera`, 빌드 SHA를 이슈에 첨부한다. 고친 뒤에는 같은 클립으로 다시 듣는다.
- **보류:** 판단이 어려운 항목은 사유를 적고, 다른 사람이 같은 클립으로 다시 듣는다.
- 이 체크리스트 결과를 릴리즈 스케쥴의 다이내믹스·EQ 항목에 반영한다. 결과 없이 "청음 완료"로 표시하지 않는다.
