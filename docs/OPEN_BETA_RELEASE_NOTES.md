# SpectralForge Chimera — Open Beta 1.3.1 · Head Controls & EQ

**Version: 1.3.1-beta.1 · Windows / macOS / Linux**

앰프 헤드 전면의 직접 조작과 모델별 노브 디자인을 확장하고, 실제 신호 경로를 따라 이동하는 Signal Path와 Tone EQ / Final EQ를 추가하는 업데이트입니다. 공개된 1.3.0의 CAB·IR·실시간 처리 수정도 포함합니다. 이 문서는 기능 설명이며 최종 소스·CI 결과·공개 상태는 릴리즈와 첨부 검증 기록을 기준으로 확인합니다.

## 주요 변경

- **헤드 직접 조작:** Classic·Dual·Matrix에서 헤드를 선택해 확대하고 전면 노브를 조절합니다. Niflheimr의 14개 노브·5채널과 Brit Edge의 6개 노브·5개 입력 경로를 포함합니다. 전체 카탈로그 26개 헤드의 전면 배치를 제공하며, 선택 가능한 앰프는 기존과 같은 25종입니다. 검수에 포함한 퇴역 Ironball이 선택 목록에 복귀하는 것은 아닙니다.
- **모델별 노브와 가독성:** 원본 하드웨어의 노브 형태·그립·캡·지시선을 참고해 앰프마다 다른 디자인을 적용합니다. Náströnd와 Niflheimr는 자체 디자인을 유지합니다. Niflheimr를 포함한 노브 크기와 간격, 라벨 크기·대비를 조정하고, 헤드 상단 모서리·손잡이·깊이 표현을 함께 정리합니다. 수치는 조작하거나 직접 입력할 때만 표시합니다.
- **조작 방법:** 노브를 드래그하고 Shift로 미세 조정합니다. 더블클릭 또는 Enter로 값을 입력할 수 있습니다. 이름이 표시된 버튼으로 채널을 선택하고 DETAILS에서 전체 컨트롤을 엽니다. A/B와 기존 채널별 설정, 호스트 자동화·프로젝트 저장을 함께 사용합니다.
- **Signal Path:** 현재 모드의 분기·합류, 활성·우회 상태와 Matrix LOW의 DI / AMP+CAB 관계를 표시합니다. 경로의 단계를 선택하면 해당 조작 화면으로 이동합니다. PATH +에서 전체 경로를 펼쳐 볼 수 있으며 화면 이동 자체는 소리 설정을 바꾸지 않습니다.
- **Tone EQ / Final EQ:** 각각 12개 밴드, 7개 필터 종류, 주파수 응답 곡선과 입력·출력 스펙트럼을 제공합니다. Tone EQ는 리그 합산과 선택적 POST Gate 뒤, POST 랙 앞에 있습니다. Final EQ는 POST·Doubler·메트로놈 뒤, OUTPUT 앞에 있습니다. 두 EQ는 기본적으로 우회되며 밴드의 주파수·게인·Q를 그래프와 수치로 조절합니다.
- **1.3 기능 유지:** Niflheimr, PRE 39종·POST 21종·프리셋 48개, 스피커 14종·마이크 20종·9개 CAB 배치와 Chimera HF를 포함한 4개 트위터 디자인을 유지합니다. 6×10 구성, 팩토리 CAB 레시피, Mic A/B 복원, 캐비넷 외장·그릴과 IR 로더 표현, 작은 블록에서의 중복 FFT 감소 및 IR 배포 범위 수정도 포함합니다. 상세 이력은 [1.3.0 릴리즈 노트](OPEN_BETA_1_3_RELEASE_NOTES.md)를 참고하세요.

## 업데이트와 기존 프로젝트

Chimera와 DAW를 종료한 뒤 설치하세요. 설치 후 SETTINGS의 **Open Beta 1.3.1** 및 build 값을 확인합니다. 1.3.1 최종 소스로 새로 빌드한 패키지를 사용하며, 공개된 1.3.0 태그와 파일은 그대로 보존합니다.

기존 플러그인 식별자·파라미터 매핑과 개인 프리셋·IR 저장 경로를 유지합니다. 헤드 전면은 기존 앰프 파라미터를 조작합니다. 두 EQ는 추가 컨트롤이며 기본값은 우회 상태입니다. 기존 프로젝트와 A/B에 포함된 IR 오디오는 계속 복원됩니다. Standalone은 공개된 베타의 업데이트를 확인하고, VST3/AU의 Updates는 릴리즈 페이지를 엽니다. 다운로드와 설치는 사용자가 실행합니다.

## 검증 범위와 남은 항목

게시 전 최종 소스에 대한 Windows·macOS·Linux 제품 빌드, 네이티브 헤드·Signal Path·EQ·CAB 검사, 패키지 검사와 Windows 설치·복구·제거 증거를 확인합니다. 이전 PR·미리보기·1.3.0의 성공 기록을 변경된 소스의 검증으로 재사용하지 않습니다. 최종 소스 SHA·CI 실행·패키지 검사 결과는 릴리즈에 첨부한 검증 기록과 build 값을 기준으로 확인합니다. 필요한 증거와 게시 조건은 [1.3.1 검증·배포 절차](RELEASE_1_3_1_PIPELINE.md)에 설명합니다.

전체 제품 검증은 완료되지 않았습니다. 엄격한 검증 목록에는 미실행 항목 107개가 남아 있으며, 개별 항목을 통과로 바꾸지 않고 별도의 공개 베타 판단에서 그 범위를 밝힙니다. 실제 Studio One 조작, 악기 DI 청음, DAW별 프로젝트 복원·인스턴스 삭제·종료의 수동 승인을 받았다는 뜻이 아닙니다. 과거 소스의 macOS 오디오 마감시간 초과 기록도 전체 컴퓨터의 무중단 처리 보증으로 대체하지 않습니다.

화면 자료는 출처를 구분합니다. 최종 소스의 각 운영체제에서 실행해 얻은 네이티브 PNG, 로컬 소프트웨어 렌더 미리보기, 디자인 일러스트·목업은 서로 다른 자료입니다. 노브 디자인 참조는 외형 비교이며 원본 하드웨어와의 음향적 동등성을 주장하지 않습니다.

캐비넷은 선형 축약 모델입니다. 혼합 유닛·별도 포트·다중 내부 구획·비선형 스피커 압축·룸 리버브는 포함하지 않습니다. Transpose의 약 43–46 ms 지연과 베이스 B현 −2반음에서의 어택 뭉개짐은 후속 과제입니다. Windows 퍼블리셔 서명 및 macOS Developer ID 서명·공증은 완료되지 않았습니다. 개인 Raven IR·NAM 가중치·개인 캡처 오디오는 공개 패키지에 포함하지 않으며, 검증된 CC BY 4.0 팩토리 IR 2개와 직접 가져온 User IR을 사용합니다. 무료 베타는 향후 유료화될 수 있고 무료 지원이 종료될 수 있습니다.

## English summary

Open Beta 1.3.1 adds enlarged, playable amp-head panels in Classic, Dual and Matrix; model-specific knob shapes and materials; larger, clearer labels; and numeric readouts shown during adjustment or entry. Niflheimr retains 14 knobs and five channels; Brit Edge has six knobs and five input paths. Front-panel layouts cover all 26 catalog entries, while 25 amps remain selectable. The retired Ironball is included in layout review only. Drag to adjust, hold Shift for fine movement, or double-click / press Enter to type. DETAILS retains access to the full controls.

Signal Path opens the controls for the selected processing stage. Tone EQ and Final EQ each have 12 bands, seven filter types, a response curve and input/output spectra. Tone EQ follows rig sum and the optional post-rig gate, before POST. Final EQ follows POST, doubler and metronome, before output trim. Both start bypassed. Existing 1.3.0 CAB, IR, state-recall and real-time fixes are retained.

Consult the release attachments for the exact source revision, native test results and package evidence; these feature notes do not certify those results. The strict product inventory retains 107 unexecuted checks; the separate beta decision discloses them. Full Studio One/manual-host acceptance and real-instrument listening are not complete. Software-rendered previews and illustrations are not native operating-system capture evidence. Hardware equivalence, universal real-time performance and completed signing/notarization are not claimed. Existing transpose limitations remain.

## Deutsche Zusammenfassung

Open Beta 1.3.1 ergänzt vergrößerte, direkt bedienbare Amp-Frontplatten in Classic, Dual und Matrix. Form, Griff, Kappe und Zeiger der Regler orientieren sich am jeweiligen Vorbild; Beschriftungen werden größer und klarer. Zahlenwerte erscheinen beim Einstellen oder bei der direkten Eingabe. Niflheimr besitzt 14 Regler und fünf Kanäle, Brit Edge sechs Regler und fünf Eingangswege. Alle 26 Katalogeinträge erhalten Frontplattenlayouts; 25 Amps bleiben auswählbar. Der ausgemusterte Ironball gehört nur zur Layoutprüfung. Ziehen regelt den Wert, Umschalt ermöglicht Feineinstellung, Doppelklick / Enter öffnet die Eingabe. DETAILS zeigt die vollständigen Bedienelemente.

Signal Path führt zum gewählten Bearbeitungsschritt. Tone EQ und Final EQ bieten jeweils zwölf Bänder, sieben Filtertypen, eine Frequenzgangkurve und Ein-/Ausgangsspektren. Tone EQ folgt der Rig-Summe und dem optionalen Post-Rig-Gate vor POST; Final EQ folgt POST, Doubler und Metronom vor dem Ausgangspegel. Beide beginnen im Bypass. Die CAB-, IR-, Zustands- und Echtzeitkorrekturen aus 1.3.0 bleiben enthalten.

Die genaue Quellrevision, nativen Prüfergebnisse und Paketnachweise sind den Release-Anhängen zu entnehmen; diese Funktionsbeschreibung bescheinigt keine Prüfergebnisse. Die strenge Produktprüfung enthält weiterhin 107 nicht ausgeführte Prüfungen; eine getrennte Beta-Entscheidung legt diese offen. Die vollständige manuelle Abnahme in Studio One und anderen Hosts sowie Hörtests mit echten Instrumenten sind nicht abgeschlossen. Software-Vorschauen und Illustrationen sind keine nativen Betriebssystem-Screenshots. Hardware-Gleichwertigkeit, unterbrechungsfreie Verarbeitung auf jedem Rechner und abgeschlossene Signierung/Notarisierung werden nicht behauptet. Die bisherigen Transpose-Grenzen bleiben bestehen.

[Published releases](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases) · [1.3.0 notes](OPEN_BETA_1_3_RELEASE_NOTES.md) · [1.2 notes](OPEN_BETA_1_2_RELEASE_NOTES.md) · [Manual](MANUAL.html)

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
