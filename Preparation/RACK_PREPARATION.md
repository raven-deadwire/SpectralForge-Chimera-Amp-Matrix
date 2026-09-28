# R1a — POST 원본 조절부 및 독립 상태 (2026-09-28)

`RACK · POST 조절부` 탭에 기존 POST 중 컴프레서·프리앰프·EQ 각 3개,
총 9개의 원본별 조절부를 추가했습니다. 기존 페달 v2와 앰프 v3 상태는
그대로 유지합니다. PRE 최대 5개, 전체 인벤토리 80개도 유지합니다.

| 기존 모델 | 선택한 원본 / 범위 | 핵심 구분 | 자료 상태 |
|---|---|---|---|
| Console VCA | SSL XLogic G Series, 2005 Rev 0A | 단계형 Attack/Release/Ratio, 연속 Threshold/Make-up, 외부 SC, Autofade | 핵심 패널 확인 |
| FET 76 | UA 1176LN D/E 계열 하드웨어 재발매 | Input/Output, 시계 방향으로 빨라지는 Attack/Release, Attack OFF, All Buttons | 핵심 패널 확인 |
| Opto Level | UA Teletronix LA-2A | Gain/Peak Reduction, Compress/Limit, 별도 후면 R37 | 핵심 패널 확인 |
| N73 Colour | Neve 1073 Classic 프리앰프 부분 | Mic/Line 단계형 Gain, Phase; EQ는 별도 패널 | 핵심 패널 확인 |
| V5 Pure | Avalon V5 모노 | 21단 Boost, 10단 Tone Bank, 입력 모드 | **일부 확인** |
| ISA Blue | Focusrite ISA One | 4단 Gain/30–60/Trim, Mic Z, 독립 Instrument Gain/Z | 핵심 패널 확인 |
| Console E | SSL E Series 500 EQ | 4밴드, 중역 Q, HF/LF Bell, Brown/Black | 핵심 패널 확인 |
| N73 Shelves | Neve 1073 Classic 전체 EQ 부분 | 고정 12 kHz HF, 단계형 MF/LF/HPF | 핵심 패널 확인 |
| Passive Tube | Pultec EQP-1A | 동시 LF Boost/Atten, 별도 HF Boost/Atten 주파수 | **일부 확인** |

Avalon의 예전 V5 PDF는 검색 색인만 읽혔고 본문 접근은 실패했습니다.
현재 제조사 V5 페이지에는 V55 설명이 섞여 있어 전체 패널 대조 완료로
취급하지 않습니다. Pultec도 제조사 주파수/기능 자료를 확인했지만 전체
전면 스위치 검토를 마치지 않았습니다. 7개 확인 상태는 지정한 핵심
패널 범위의 문헌 확인이며 하드웨어 반응·DSP 인증이 아닙니다.

`rack_controls.py`에 모델별 원문 URL, 리비전, 제외 기능, 설명을 넣었습니다.
모든 연속 값은 0–1 노브 위치입니다. 50%는 임시 초기 위치이며 물리 dB,
ms, Hz, 측정된 포트 테이퍼나 출고값으로 환산하지 않습니다. 미터에는
신호 없음만 표시합니다. 1176 미터 OFF와 LA-2A R37도 상태 대상일 뿐
실제 전원/압축/계측 동작은 없습니다.

## 저장 계약

`ui/rack_state.js`의 `.crlab` v1은 3개 고정 구획(bus/preamp/eq), 모델별
설정 뱅크, 구획별 Bypass, 별도 소프트웨어 Trim/Level, Undo/Redo를
저장합니다. 서로 다른 모델의 값은 자동 환산하지 않습니다. 독립 JSON
형식이며 `.cbp`, `.calab`, `.chimera`, 실제 DAW 상태의 대체 형식이 아닙니다.

불러오기는 검증 성공 후 한 번에 적용합니다. 중복 키(이스케이프된 같은
키 포함), 비유한 수/지수 오버플로, 깊이·크기 초과, 금지된 프로토타입 키,
알 수 없는 모델/컨트롤, 다른 구획의 모델, 누락 키, 잘못된 자료형·범위·버전을
거부하고 기존 상태와 Undo 이력을 유지합니다.

## 검증과 연결 경계

실행된 검사는 `VERIFICATION.md`에 구분해 기록했습니다. 이 작업은 실제
`Source/`의 POST 파라미터나 DSP와 연결되지 않습니다. 기존 DSP 원시
파라미터를 원본 노브 위치로 조용히 매핑하지 않았습니다. 생산 통합에는
원본별 DSP 계약·호스트 안정 ID·기존 상태 마이그레이션·모델 전환·측정
검사가 추가로 필요합니다.

나머지 MOD/DELAY/REVERB 12개 POST는 인벤토리에 보존하며 패널 검토
대기로 남깁니다. 릴리스, Windows/DAW, NAM/IR 실물 비교 또는 오디오
정확성의 완료를 이 R1a 결과로 판정하지 않습니다.
