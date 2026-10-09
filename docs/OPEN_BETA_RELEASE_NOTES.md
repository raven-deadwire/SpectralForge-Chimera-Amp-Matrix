# SpectralForge Chimera — Open Beta 1.3 · Niflheimr & CAB

**Version 1.3.0-beta.1 · Windows / macOS / Linux · release preparation, 2026-10-09 KST**

Niflheimr 베이스 앰프와 Chimera 오리지널 캐비넷·스피커·마이크 모델을 통합하는 1.3 업데이트입니다. 현재 문서는 릴리즈 준비본이며, 공개 다운로드와 최종 검증 결과는 확정된 소스의 빌드가 완료된 뒤 연결합니다.

## 주요 변경

- **Niflheimr:** 다섯 채널의 SpectralForge Original 베이스 앰프와 다섯 개의 완성형 프리셋을 추가합니다. 전체 선택 가능 구성은 AMP 25종, PRE 39종, POST 21종, 프리셋 48개입니다. 캐비넷·스피커·마이크는 별도 목록입니다.
- **오리지널 CAB:** 기타 스피커 8종과 베이스 스피커 6종, 다이나믹 9종·리본 3종·콘덴서 8종의 마이크 20종, 트위터 3종을 추가합니다. Chimera 고유 음향 모델이며 상용 하드웨어와의 동등성은 주장하지 않습니다.
- **610 재구성:** 미공개 810 테스트 모델을 10인치 유닛 6개의 2열×3행 구성으로 교체했습니다. 기존 스피커·마이크 응답 모델을 활용하고 함체 높이·용적·유닛 간 간섭을 610에 맞췄습니다. 이전 테스트 설정의 7·8번 마이크 대상은 같은 열의 5·6번으로 해석됩니다. 기존 8×10 캡처/User IR의 음원과 표기는 유지됩니다.
- **캐비넷 구성:** 기타 1×12 / 2×12 / 4×12, 베이스 1×12 / 2×12 / 1×15 / 2×10 / 4×10 / 6×10을 선택할 수 있습니다. 캐비넷의 유닛 지름은 고정됩니다. 10인치 구성에는 12·15인치 유닛을 넣을 수 없습니다. 선택 목록의 중복 4×12·4×10 항목은 기존 프로젝트 호환성을 유지하면서 정리합니다.
- **독립 Mic A/B:** 마이크 종류와 수음 유닛, 위치·거리, 블렌드·레벨·극성·딜레이·필터를 각 경로에서 조절합니다. 한 마이크만 쓰거나 서로 다른 유닛을 수음할 수 있습니다.
- **룸과 외형:** Dual/Matrix에서 리그를 한 룸에 표시하고 캐비넷을 선택해 CAB 조작 화면을 엽니다. 실제로 놓인 장비를 기준으로 프레이밍하며, 마이크가 없는 룸에 예약된 마이크 여유 공간을 제거합니다. 공통 축척과 실제 가로세로 비율을 유지하고 기존 캐비넷 외장 디자인을 살립니다.
- **유닛 아트와 이름:** 유닛 14종의 서로 다른 정면 디자인, 물리 치수에 맞춘 헤드와 마이크, IR Loader의 이미지를 제공합니다. 두 기타 유닛은 **Crimson 12**와 **Nocturne 100**으로 표시하며 Crimson 12는 빨간색을 유지합니다.
- **Matrix LOW:** DI/AMP 블렌드는 DI와 AMP→CAB 경로를 섞습니다. 캐비넷은 AMP 쪽 비중에만 작용하며, Mic A/B 블렌드는 그 캐비넷 안에서 별도로 작동합니다.
- **실시간 처리:** 작은 오디오 블록에서 오리지널 스테레오 캐비넷의 중복 FFT 작업을 줄입니다. 응답 길이, 전환 페이드, 음향식과 CPU 검증 기준을 유지합니다.
- **배포 검사:** Windows에서 UTF-8 문서를 읽는 인코딩 오류를 수정하고, 중간 명령의 실패가 마지막 명령의 성공으로 가려지지 않도록 합니다. 실제 검증자료를 소스·실행·시도별로 취합해 게시 전에 다시 평가합니다.

## 업데이트와 기존 프로젝트

Chimera와 DAW를 종료한 뒤 설치하세요. 설치 후 SETTINGS에서 **Open Beta 1.3.0**과 build 값을 확인합니다. 1.3 소스에서 생성하고 검증한 패키지만 사용합니다. 기존 1.2 preview의 파일명만 바꾸어 배포하지 않습니다.

플러그인 식별자, 기존 파라미터 순서와 자동화 매핑, 개인 프리셋·IR 저장 경로를 유지합니다. CAB 구성은 추가 파라미터로 저장하며, 같은 IR 데이터는 공유 저장해 중복을 줄입니다. 호환되지 않는 캐비넷/유닛 조합은 일관된 호환 유닛으로 해석합니다. 사용자 IR은 녹음에 포함된 캐비넷과 마이크의 응답을 유지합니다.

## 검증과 알려진 한계

최종 1.3 소스에서 Windows·macOS·Linux 제품 빌드, CTest, CAB 검증, 패키지 조립과 Windows 설치·복구·제거를 다시 확인합니다. 이전 설치본의 성공 기록을 변경된 1.3 소스의 성공으로 재사용하지 않습니다. 새 소스의 실제 결과는 [1.3 준비 기록](RELEASE_1_3_PREPARATION.md)에 연결합니다.

실제 악기 DI 청음과 Studio One 등 사용 DAW에서의 연주·프로젝트 복원·UI를 연 뒤 인스턴스 삭제 및 종료 검증은 별도 확인 항목입니다. 이전 소스의 macOS 개별 오디오 마감시간 초과 기록도 유지합니다. 합성 테스트만으로 모든 컴퓨터에서의 무중단 처리나 원본 하드웨어와의 동등성을 주장하지 않습니다.

현재 캐비넷은 선형 축약 모델입니다. 서로 다른 유닛의 혼합 장착, 별도 포트·다중 내부 구획, 비선형 스피커 압축과 룸 리버브는 이번 범위에 포함하지 않습니다. Transpose의 약 43–46ms 알고리즘 지연과 베이스 B현 −2반음에서의 어택 뭉개짐은 기존 후속 과제입니다.

개인 Raven IR, NAM 가중치와 개인 캡처 오디오는 공개 패키지에 포함하지 않습니다. Windows 퍼블리셔 서명과 macOS Developer ID 서명·공증의 기존 한계도 유지합니다.

## 다음 업데이트

앰프 헤드 전면 디자인 안에서 노브를 직접 조절하는 UI를 검토·설계합니다. 기존 호스트 자동화와 채널별 설정을 재사용하며 다음 업데이트의 정확한 번호는 확정하지 않았습니다. 헤드 내부 노브 조작은 1.3 기능에 포함하지 않습니다.

## English summary

Open Beta 1.3 prepares Niflheimr and the original CAB engine. It includes 25 active amps, 39 PRE models, 21 POST models and 48 presets; 14 speakers, 20 microphones and three additional tweeter designs are a separate inventory. Nine explicit cabinet layouts enforce driver diameter compatibility. The large bass layout is a 2×3 six-driver 6×10, rebuilt from the unpublished 8×10 preview with a shorter enclosure and six-source acoustic field. Independent Mic A/B paths support source selection, modeled position/distance, blend and phase controls. Room framing uses the displayed rigs, retains a common physical scale and removes unused microphone margins. Cabinet exterior detail and fixed captured/User IR responses are preserved.

The release remains in preparation. Exact-source 1.3 packages and tests must be verified before publication. Actual instrument/Studio One acceptance, existing transpose limitations and signing/notarization status remain separately documented. Embedded head-panel knobs are planned after 1.3.

## Deutsche Zusammenfassung

Open Beta 1.3 integriert Niflheimr und die originale CAB-Engine: 25 aktive Verstärker, 39 PRE- und 21 POST-Modelle sowie 48 Presets. Dazu kommen 14 Lautsprecher, 20 Mikrofone, drei zusätzliche Hochtöner und neun Gehäusekonfigurationen mit passendem Lautsprecherdurchmesser. Das große Bassgehäuse ist jetzt ein 6×10 mit zwei Spalten und drei Reihen; das bisherige 8×10-Testmodell wurde akustisch und geometrisch entsprechend umgebaut. Mic A/B lassen sich unabhängig bearbeiten. Die Raumansicht nutzt den Platz anhand der angezeigten Rigs bei gemeinsamem physischem Maßstab; vorhandene Gehäusedetails bleiben erhalten. Eigene IRs und bestehende Projektzuordnungen bleiben unterstützt.

Die Veröffentlichung wird vorbereitet. Pakete und Prüfungen müssen zur endgültigen 1.3-Quellrevision passen. Hörtests mit echten Instrumenten, Studio-One-Prüfungen und bestehende Grenzen bleiben getrennt ausgewiesen. Regler direkt auf der Verstärkerfront sind für ein Update nach 1.3 vorgesehen.

[Published 1.2 release](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1) · [Historical 1.2 notes](OPEN_BETA_1_2_RELEASE_NOTES.md)

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
