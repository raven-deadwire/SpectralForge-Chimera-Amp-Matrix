# SpectralForge Chimera — Open Beta 1.1.1

**Version `1.1.1-beta.1` · Windows / macOS / Linux · 공개 베타**

베타 1.1 이후의 UI 부하 개선, Gate Range, 앰프·시그니처 프리셋 통합과 설치 버전 정리를 포함합니다.

## 주요 변경

- **게인과 프리셋 재정비:** 리드·하이게인 채널의 프리앰프 포화를 보강했습니다. 38개 프리셋의 용도별 게인·톤·컴프레서 설정을 정리하고, 출력값을 합성 다중 음역 신호로 재조정했습니다. 메뉴와 앞/뒤 이동을 같은 분류 순서로 맞췄습니다. 음량 측정은 실제 DI 청감 승인을 대신하지 않습니다.
- **노브·엔진 일치:** 일반 Factory 프리셋도 화면의 AMP/PRE/POST 노브와 같은 native 엔진을 불러옵니다. 선택된 음색 뱅크는 프리셋 값으로 초기화하고, 사용하지 않는 PRE 모델의 사용자 설정은 유지합니다. 저장된 프로젝트를 열 때 자동으로 새 프리셋으로 바꾸지는 않습니다.
- **PRE·베이스 EQ 수정:** BDDI Blend=0에서도 EQ가 작동하도록 수정하고, Cali76의 Attack/Release 방향·시간과 독립 DRY/OUT을 바로잡았습니다. SVT-VR/B7K/DB751/Subway의 확인된 EQ 범위·주파수 및 Subway HPF 구조를 정정했습니다. 수동 톤스택의 회로 상호작용과 실물 일치까지 검증한 것은 아닙니다.

- **UI 부하 개선:** 화면 전체를 반복 갱신하던 작업을 줄이고, 보이는 패널과 변경된 값 중심으로 갱신합니다. 전체 오디오 DSP CPU 감소율을 의미하지 않습니다.
- **Gate Range:** 게이트가 닫힐 때 감쇠할 양을 조절합니다. `Full`은 기존 동작을 유지하며, 이전 프로젝트의 Release·Hold와 게이트 경로를 보존합니다.
- **앰프·프리셋 통합:** E670FE 기반 모델과 베이스 시그니처를 통합했습니다. 기타 시그니처는 A Path To Alsatia, Feel My Wrath, Blackhearted, Dark Matters of Throne의 4종입니다. 개발자의 실사용 검수를 거쳐 베타 배포 대상으로 확인했습니다.
- **RIGS 표시:** 앰프의 작은 원본명 표시에서 `REFERENCE:`를 제거해 PRE·POST와 같이 모델명만 표시합니다.
- **호환성 검사:** 기존 파라미터 순서와 Ironball의 저장 상태·DSP를 보존하며, 상태 복원·출력 레벨·컴프레서 GR·플러그인 제거 시나리오의 회귀 검사를 보강했습니다.
- **설치 버전 통일:** 제품, Windows 설치 정보, 패키지와 앱 내 버전을 같은 버전 정보에서 생성합니다. 앱·VST3 설치 경로 선택과 개인 프리셋·IR 보존을 유지합니다.

## Transpose — 현재 한계

**트랜스포즈의 저지연·음질 개선은 장기 과제로 분리했습니다.** 현재 추가 알고리즘 지연은 44.1 kHz에서 약 46.4 ms, 48/96 kHz에서 약 42.7 ms입니다. Azhi Dahaka 프리셋을 베이스 B현에서 −2반음으로 사용했을 때 “츅츅”거리고 음의 몸통과 연속 피킹이 뭉개진다는 피드백이 있습니다.

이 증상이 나타나는 파트에서는 Transpose를 끄고 실제 튜닝을 사용하세요. 이번 버전은 해당 음질 문제나 10 ms 이하 지연 목표를 달성한 버전으로 안내하지 않습니다. 기존 저역 에너지 계산·바이패스 전환 수정은 포함하며, 추가 피치 엔진 변경은 없습니다.

## 종료 안정성과 검증 범위

Windows VST3에서 UI를 열었던 인스턴스를 제거한 뒤 다른 인스턴스를 계속 사용하는 자동 검사 등을 보강했습니다. 개발자는 2026년 10월 4일 종료·삭제, 연속 사용, DI·Gate·E670FE 및 타이밍에 관한 실사용 검수를 완료했다고 확인했습니다. 이 확인은 개발자의 테스트 환경에 대한 베타 승인입니다. Studio One·Cubase·Sonar의 모든 버전과 시스템에서의 호환성을 보증하지는 않습니다.

앰프·이펙트는 원본 장비를 참고한 소프트웨어 모델이며 실물과의 동등성이나 제조사 인증을 의미하지 않습니다. 자동 빌드·패키지 검증과 개발자 실사용 승인은 구분해 기록합니다. 실측 수치나 하드웨어 동등성 인증을 의미하지 않습니다.

## 설치 및 파일

Windows는 DAW와 Chimera를 종료한 뒤 Setup에서 앱·VST3 경로를 선택하세요. 사용자 지정 VST3 폴더는 DAW 검색 경로에도 추가해야 합니다. 설치 후 SETTINGS에서 **Open Beta 1.1.1**과 build 값을 확인하세요.

| 운영체제 | 다운로드 |
| --- | --- |
| Windows x64 | [Setup](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SpectralForge-Chimera-1.1.1-beta.1-win64-Setup.exe) · [Portable ZIP](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SpectralForge-Chimera-1.1.1-beta.1-win64.zip) |
| macOS Apple Silicon / Intel | [Universal PKG](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SpectralForge-Chimera-1.1.1-beta.1-macos-universal.pkg) |
| Linux x86_64 | [DEB](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SpectralForge-Chimera-1.1.1-beta.1-linux-x86_64.deb) · [TAR.GZ](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SpectralForge-Chimera-1.1.1-beta.1-linux-x86_64.tar.gz) |

[오프라인 매뉴얼](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/MANUAL.html) · [SHA-256 체크섬](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.1-beta.1/SHA256SUMS.txt)

개인 Raven IR과 NAM 캡처는 공개 패키지에 포함하지 않습니다.

Windows는 퍼블리셔 코드서명 전이며 macOS는 Developer ID 서명·공증 전입니다. 기존 무료 공개 베타 정책을 유지하며, 향후 유료화와 무료 지원 종료 가능성이 있습니다.

## English summary

Open Beta 1.1.1 includes lower idle UI redraw cost, Gate Range, integrated E670FE and signature presets, plain RIGS reference-model captions, expanded compatibility tests and consistent product/package versions. Transpose latency and sound-quality improvements are deferred: nominal algorithmic delay remains about 43–46 ms, and bass B-string playing at -2 semitones can lose note body and attack separation. The maintainer confirmed personal host, sound and timing acceptance on 2026-10-04. This is beta acceptance for the tested environment, not universal DAW compatibility or hardware-equivalence certification. Downloads and checksums are linked above.

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
