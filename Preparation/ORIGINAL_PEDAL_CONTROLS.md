# 기존 PRE 25개 — 원본 조작부 개정

2026-09-28 / Preparation 전용 / 제품 DSP·배포본 변경 없음

기존 공통 노브를 보존하는 것과 원본 컨트롤을 제공하는 작업은 분리한다. `controls`는 원본 패널 대상 정의, `legacy_controls`는 옛 원시 값 저장이다. `hw.*` 조절은 옛 값이나 DSP를 변경하지 않는다.

제조사 패널·매뉴얼 확인: **19개 변형**. 역사적 원본 1차 자료 최종 확인 보류: **6개 변형**. 후자의 조작부는 임시 대상 정의이며 검증 완료가 아니다. 원본 세대가 모호했던 것은 선택한 대상 세대를 명시했다. A2/실물 반응 검증과는 별개의 점검이다.

| 키메라 모델 | 대상 원본 | 전면 제어 | 내부 제어 | 자료 상태 |
|---|---|---|---|---|
| Green 808 | Ibanez TS808 | OVER DRIVE, TONE, LEVEL | 없음 | 확인 |
| Gold Drive | Klon Centaur | GAIN, TREBLE, OUTPUT | 없음 | **추가 확인 필요** |
| Rodent | Pro Co RAT 2 | DISTORTION, FILTER, VOLUME | 없음 | **추가 확인 필요** |
| Bass DI | Tech 21 SansAmp Bass Driver DI v2 | LEVEL, BLEND, TREBLE, MID, BASS, PRESENCE, DRIVE, MID SHIFT [500 Hz/1 kHz], BASS SHIFT [40 Hz/80 Hz] | 없음 | 확인 |
| Micro Bass | Darkglass Microtubes B3K v2 | BLEND, TONE, LEVEL, DRIVE, GRUNT, MID BOOST | 없음 | 확인 |
| Studio VCA | MXR M87 Bass Compressor | RELEASE, ATTACK, OUTPUT, RATIO [4:1/8:1/12:1/20:1], INPUT | 없음 | 확인 |
| Red OTA | MXR Dyna Comp M102 | OUTPUT, SENSITIVITY | 없음 | 확인 |
| Optical | Diamond Compressor CPR-1 | COMP, EQ, VOL | EQ IN, 4.8 kHz HI-CUT | 확인 |
| Studio FET | Origin Effects Cali76 FET Compressor (2024) | IN, OUT, DRY, RATIO, ATTACK, RELEASE | 없음 | 확인 |
| Variable Mu | Manley Stereo Variable Mu — original linked-input panel | DUAL INPUT, STEREO LINK [SEP/LINK], THRESHOLD, ATTACK, RECOVERY [8 s/4 s/0.6 s/0.4 s/0.2 s], OUTPUT, MODE [COMPRESS/LIMIT], THRESHOLD, ATTACK, RECOVERY [8 s/4 s/0.6 s/0.4 s/0.2 s], OUTPUT, MODE [COMPRESS/LIMIT] | 없음 | 확인 |
| Q Sweep | EHX Q-Tron — original big-box panel | MODE [LP/BP/HP/MIX], SWEEP [UP/DOWN], RANGE [HI/LO], PEAK, GAIN, BOOST | 없음 | 확인 |
| Tron Band | Musitronics Mu-Tron III — vintage target | GAIN, PEAK, MODE [LP/BP/HP], RANGE [LO/HI], DRIVE [UP/DOWN] | 없음 | **추가 확인 필요** |
| Reverse Sweep | Musitronics Mu-Tron III — vintage target | GAIN, PEAK, MODE [LP/BP/HP], RANGE [LO/HI], DRIVE [UP/DOWN] | 없음 | **추가 확인 필요** |
| Bass Envelope | MXR M82 Bass Envelope Filter | DRY, FX, DECAY, Q, SENS. | 없음 | 확인 |
| Dynamic Wah | BOSS AW-3 | DECAY, MANUAL, SENS, MODE [UP/DOWN/SHARP/HUMAN/TEMPO] | 없음 | 확인 |
| Big Sustain | EHX Big Muff Pi — NYC | VOLUME, TONE, SUSTAIN | 없음 | 확인 |
| Round Face | Dunlop Fuzz Face JDF2 | VOLUME, FUZZ | 없음 | 확인 |
| Bender | Sola Sound Tone Bender Professional MKII — target | LEVEL, ATTACK | 없음 | **추가 확인 필요** |
| Wool Bass | ZVEX Woolly Mammoth | VOL, EQ, PINCH, WOOL | 없음 | 확인 |
| Gated Factory | ZVEX Fuzz Factory — five-knob | VOLUME, GATE, COMP, DRIVE, STAB | 없음 | 확인 |
| RC Clean | Xotic RC Booster — original four-knob | GAIN, VOLUME, TREBLE, BASS | 없음 | 확인 |
| Treble Lift | Dallas Rangemaster — vintage target | SET | 없음 | **추가 확인 필요** |
| Micro Lift | MXR Micro Amp M133 | GAIN | 없음 | 확인 |
| EP Lift | Xotic EP Booster | GAIN | +3 dB GAIN, BRIGHT / FLAT EQ | 확인 |
| Linear Power | EHX LPB-1 — Nano | BOOST | 없음 | 확인 |

## 모델별 범위·출처

### Green 808 / legacy.drive.0

원본 OVER DRIVE·TONE·LEVEL로 구성했습니다. 기존 Tone의 Hz 값을 원본 Tone의 노브 위치로 환산하지 않습니다.

- https://www.ibanez.com/usa/products/detail/ts808_99.html

### Gold Drive / legacy.drive.1

GAIN·TREBLE·OUTPUT 대상 정의입니다. 원본 1차 자료 확인은 남아 있으며 별도 Blend 노브를 추가하지 않았습니다.

- 이번 점검에서 원본 1차 자료를 확보하지 못했다.

### Rodent / legacy.drive.2

RAT 2를 대상 리비전으로 명시했습니다. FILTER·VOLUME을 공통 Tone·Level로 대체하지 않으며, 원본 매뉴얼 확인은 남아 있습니다.

- 이번 점검에서 원본 1차 자료를 확보하지 못했다.

### Bass DI / legacy.drive.3

v2의 7개 노브와 두 주파수 전환을 구분했습니다. v1과 혼용하지 않으며 물리 출력 패드·팬텀/접지 제어는 이번 패널에서 제외했습니다.

- https://www.tech21nyc.com/products/sansamp-2/bassdriver-di/
- https://www.tech21nyc.com/wp-content/uploads/2023/11/BSDR_v2_OM4.pdf

### Micro Bass / legacy.drive.4

v2의 4개 노브·GRUNT·MID BOOST를 정의했습니다. 구형 ATTACK 스위치를 함께 넣지 않았고 기존 DSP의 리비전 일치는 별도 검증입니다.

- https://www.darkglass.com/en-int/products/microtubes-b3k

### Studio VCA / legacy.comp.0

INPUT·OUTPUT·ATTACK·RELEASE와 4단 RATIO입니다. Studio VCA는 기존 키메라 이름이며 M87의 회로 방식을 인증하는 표기가 아닙니다.

- https://www.jimdunlop.com/content/manuals/M87.pdf

### Red OTA / legacy.comp.1

OUTPUT·SENSITIVITY 두 노브입니다. 원본에 없는 Attack은 전용 조작부에서 제거하고 옛 상태 값으로만 보존했습니다.

- https://www.jimdunlop.com/content/manuals/M102.pdf

### Optical / legacy.comp.2

원형 CPR-1의 COMP·EQ·VOL과 내부 EQ/Hi-cut 스위치입니다. 현행 COMP/EQ의 MIDS·TILT·ATTACK을 섞지 않았습니다. 기존 DSP가 어느 리비전을 참고했는지는 별도 확인이 필요합니다.

- https://www.diamondpedals.com/pages/legacy-support
- https://cdn.shopify.com/s/files/1/0716/1387/4479/files/Manual-Diamond_Compressor.pdf?v=1678928483

### Studio FET / legacy.comp.3

2024 Cali76 FET의 IN·OUT·DRY·RATIO·ATTACK·RELEASE입니다. DRY는 원음 레벨을 추가하는 기능이며 단순한 반대 방향 Wet/Dry 크로스페이드로 가정하지 않습니다.

- https://origineffects.com/product/cali76-fet-compressor/

### Variable Mu / legacy.comp.4

물리 페달이 아니라 스테레오 랙 레퍼런스를 한 PRE 슬롯에서 다루는 정의입니다. 구형 공통 연속 INPUT 패널과 좌우 제어를 선택했고, 현행 단계형 INPUT·HP SC와 혼용하지 않았습니다.

- https://www.manley.com/products/pro-audio/dynamics/variable-mu
- https://mlabs2017.squarespace.com/s/Manley-Stereo-Variable-Mu-Limiter-Compressor-Session-Recall-Sheet.pdf

### Q Sweep / legacy.filter.0

제조사 사진에서 MIX를 포함한 4단 MODE와 SWEEP·RANGE·BOOST를 확인했습니다. BOOST는 회전식 외형이지만 ON/OFF이며 연속적인 Mix 노브가 아닙니다.

- https://www.ehx.com/products/q-tron/
- https://b2155914.assetcdn.net/2155914/wp-content/uploads/2023/05/Picture21-1.jpg

### Tron Band / legacy.filter.1

빈티지 Mu-Tron III의 5개 조작부를 대상으로 한 임시 정의입니다. 현행 제품의 LEVEL을 섞지 않았으며 빈티지 원본 매뉴얼 확인은 남아 있습니다.

- https://mu-tron.com/

### Reverse Sweep / legacy.filter.2

Tron Band와 같은 하드웨어 계열이며 MODE·DRIVE의 시작 설정을 다르게 둡니다. 빈티지 원본 매뉴얼의 최종 확인은 남아 있습니다.

- https://mu-tron.com/

### Bass Envelope / legacy.filter.3

DRY·FX를 독립 레벨로 분리했습니다. DECAY는 릴리스 시간이 아니라 감쇄 시 멈추는 주파수를 조절하는 기능입니다.

- https://www.jimdunlop.com/content/manuals/M82.pdf

### Dynamic Wah / legacy.filter.4

물리 조작부는 4개입니다. HUMAN을 고르면 같은 MANUAL·SENS가 VOWEL 2·VOWEL 1로 표시됩니다. 모드별 추적·모음 반응과 외부 입력/EXP는 아직 DSP에 연결되지 않았습니다.

- https://www.boss.info/global/products/aw-3/
- https://static.roland.com/assets/images/products/main/aw-3_top_main.jpg

### Big Sustain / legacy.fuzz.0

NYC Big Muff Pi의 VOLUME·TONE·SUSTAIN으로 표시합니다. 원래 Sustain을 공통 Drive로 바꾸지 않았습니다.

- https://www.ehx.com/products/big-muff-pi/

### Round Face / legacy.fuzz.1

JDF2의 VOLUME·FUZZ 두 노브입니다. 원본에 없는 Tone을 제거했으며 픽업 부하 반응은 시제품에서 구현하지 않습니다.

- https://www.jimdunlop.com/content/manuals/JDF2.pdf

### Bender / legacy.fuzz.2

Professional MKII의 LEVEL·ATTACK을 대상 정의로 두었습니다. 원본 패널 확인은 남아 있으며 MKIII/IV의 Tone을 섞거나 Attack을 컴프 시간으로 취급하지 않습니다.

- https://macaris.co.uk/product-category/colorsound/

### Wool Bass / legacy.fuzz.3

VOL·EQ·PINCH·WOOL입니다. PINCH의 게이팅/파형 반응을 공통 Tone·Drive에 합치지 않습니다.

- https://www.zvex.com/guitar-pedals/woolly-mammoth-guitar-effects-pedal
- https://static1.squarespace.com/static/555e332ce4b0577e788c3a16/t/56329249e4b0b94d8f649311/1446154825093/ZVEX%2BWoolly%2BMammoth%2BInstructions.pdf

### Gated Factory / legacy.fuzz.4

제조사 사진의 VOLUME·GATE·COMP·DRIVE·STAB 다섯 노브입니다. Tone은 제거했으며 노브 간 상호작용과 발진은 별도 DSP 검증 대상입니다.

- https://www.zvex.com/guitar-pedals/fuzz-factory-guitar-effects-pedal
- https://images.squarespace-cdn.com/content/v1/555e332ce4b0577e788c3a16/1530208977456-YTYPEWLPZF90YRMOO22R/ZVEX_Fuzz_Factory.jpg

### RC Clean / legacy.boost.0

원형 RC Booster의 GAIN·VOLUME·TREBLE·BASS입니다. RC Booster V2의 추가 게인 채널을 섞지 않았습니다.

- https://xotic.us/manuals/pdf/RC_Booster_manual.pdf

### Treble Lift / legacy.boost.1

단일 SET 노브를 대상으로 합니다. 원본 1차 자료 확인은 남아 있으며 Bass·Treble 노브는 제거했습니다.

- 이번 점검에서 원본 1차 자료를 확보하지 못했다.

### Micro Lift / legacy.boost.2

M133의 단일 GAIN 노브입니다. EQ가 있는 Micro Amp+와 구분했습니다.

- https://www.jimdunlop.com/mxr-micro-amp/

### EP Lift / legacy.boost.3

전면 GAIN 하나와 내부 DIP 두 개를 구분했습니다. DIP의 출고 상태와 달리 연속 노브의 50%는 시제품 초기 위치일 뿐입니다.

- https://xotic.us/effects/ep-booster/
- https://xotic.us/effects/ep-booster/pdf/ep_booster_manual.pdf

### Linear Power / legacy.boost.4

단일 BOOST 노브입니다. LPB-3의 EQ 조절부를 가져오지 않았습니다.

- https://www.ehx.com/products/lpb-1/

## 저장·UI 정책

기존 `.cbp`에서 옛 제어 값과 MIDI 키를 그대로 보존한다. 원본 조작부는 별도 초기 위치로 생성하고, 옛 값과 물리 노브 사이의 임의 보정·재매핑은 하지 않는다. `.chimera`/실제 DAW 프로젝트 마이그레이션은 구현하지 않았다.

한 화면 최대 다섯 페달을 유지한다. “기존 계열 점검”에서 다섯 계열을 선택하면 각 계열의 기존 5종을 확인할 수 있다. 복잡한 제어는 상세 뷰, 내부 DIP는 별도 그룹이며 미터에 가상의 GR은 표시하지 않는다.

이번 범위: 정의 데이터·UI·준비용 저장 호환·테스트. 실제 오디오 노브 반응·모델 DSP·실제 MIDI/오토메이션·Windows VST3·DAW 종료 오류 수정은 미완료다.
