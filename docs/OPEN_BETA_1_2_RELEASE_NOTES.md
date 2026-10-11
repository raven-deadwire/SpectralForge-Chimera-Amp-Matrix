# SpectralForge Chimera — Open Beta 1.2 · Náströnd

**Version `1.2.0-beta.1` · Windows / macOS / Linux · 2026-10-05 KST**

SpectralForge Original 기타 앰프 Náströnd와 새 PRE / AMP / CAB / POST 프리셋을 추가한 공개 베타입니다. 사용자 테스트에서 제기된 하이게인 양, 채널 간 음량 차이와 Fimbulvetr의 먹먹함을 추가 보정했습니다.

## 주요 변경

- **Náströnd:** Fenrir · Surtr · Níðhöggr · Fimbulvetr · Ragnarök의 다섯 실제 앰프 채널. 각 채널은 Classic/Dual/Matrix의 해당 위치에서 13개 노브를 별도로 기억합니다. 처음에는 기본값, 다시 선택하면 마지막 편집값을 불러옵니다. ALL → RESET CHANNEL은 선택한 채널만 초기화합니다.
- **채널 보정:** Fenrir를 기준으로 다른 채널의 출력과 대역 균형을 조정했습니다. Fimbulvetr는 저중역 강조, interstage 저역·대역폭, supply 응답과 어택 대역을 함께 조정했습니다. 강한 PRE·POST를 포함한 합성 테스트 체인에서 가중 음량 차이를 추가로 줄였습니다.
- **Original 프리셋:** Thall Rhythm, Molten Lead, Rotten Grind, Sludge Mass, Slam Impact. PRE부터 캐비닛과 POST까지 구성하며 Rotten Grind는 crossover Dual, Slam Impact는 Matrix입니다. Thall BLOOM 7.5, Molten ROT 7.5, Slam Low DI Comp 0.5 / DI-AMP 75%로 시작합니다.
- **전체 43개 프리셋 OUTPUT 0 dB:** 출력 노브는 유니티 값으로 통일했습니다. Ambient Clean은 낮은 앰프 게인을 유지하고 POST Console VCA로 출력을 확보합니다. 필요한 피크 여유는 앰프·POST의 표시되는 출력 단계에서 확보합니다.
- **기존 하이게인 프리셋:** 제공된 Melodic Death Rhythm, Orange Heavy, Tight Rhythm, Dual Tight/Wide 설정을 반영하고 17개 PRE/AMP 게인 경로를 회귀 검사합니다. Modern Clean 프리셋 이름에 원본 브랜드를 노출하지 않습니다.
- **아트·매뉴얼:** 승인된 Náströnd 헤드 아트를 유지하며 EN/DE/KO 매뉴얼에 채널 기억과 새 프리셋을 안내합니다. 활성 앰프 24개, PRE 39개, POST 21개입니다.

## 업데이트와 기존 프로젝트

DAW와 Chimera를 종료한 뒤 업데이트하세요. 설치 후 SETTINGS에서 **Open Beta 1.2.0**과 build 값을 확인합니다. 새 프리셋 값은 Factory/Signature를 다시 불러올 때 적용됩니다. 저장된 노브·채널·사용자 IR 상태는 유지하며, DSP가 변경된 채널은 기존 프로젝트에서도 음색과 음량이 달라질 수 있습니다. OUTPUT 0 dB는 신호 피크를 0 dBFS로 정규화한다는 뜻이 아닙니다.

Standalone의 업데이트 확인은 beta 채널에서 새 버전을 안내하며, 수동 확인은 SETTINGS → Manual / Bug report / Updates에서 가능합니다. VST3/AU에서는 릴리즈 페이지를 엽니다. 다운로드와 설치는 사용자가 진행합니다.

## 검증 범위와 알려진 한계

최종 소스의 Windows·macOS·Linux 빌드, 전체 CTest 및 패키지 검사와 Windows 설치·실행·VST3 로딩·복구·제거 검증을 통과한 파일만 게시합니다. 패키지 바이트·SHA-256·업데이트 매니페스트·소스 SHA를 대조합니다. 기존 태그와 배포 파일은 덮어쓰지 않습니다.

새 보이싱의 정량 비교는 합성 입력 기반입니다. 이전 1.1.1의 개발자 실사용 확인을 이번 음색의 새로운 실제 DI 청음 승인으로 재사용하지 않습니다. Studio One·Cubase·Sonar의 모든 환경에 대한 호환성이나 원본 하드웨어와의 동등성을 보증하지 않습니다.

Transpose는 이번 릴리즈에서 변경하지 않습니다. 약 43–46 ms 알고리즘 지연과 베이스 B현 −2반음의 몸통·연속 어택 뭉개짐은 장기 개선 항목입니다. EICH T900의 정확한 NAM, Meshuggah head-only 기준, Granophyre 캡처의 물리적 출처 등 기존 레퍼런스 한계는 [비교 보고서](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/v1.2.0-beta.1/docs/NASTROND_REFERENCE_COMPARISON.md)에 유지합니다.

## 다운로드

| 운영체제 | 패키지 |
| --- | --- |
| Windows x64 | [Setup](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SpectralForge-Chimera-1.2.0-beta.1-win64-Setup.exe) · [Portable ZIP](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SpectralForge-Chimera-1.2.0-beta.1-win64.zip) |
| macOS Apple Silicon / Intel | [Universal PKG](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SpectralForge-Chimera-1.2.0-beta.1-macos-universal.pkg) |
| Linux x86_64 | [DEB](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SpectralForge-Chimera-1.2.0-beta.1-linux-x86_64.deb) · [TAR.GZ](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SpectralForge-Chimera-1.2.0-beta.1-linux-x86_64.tar.gz) |

[매뉴얼](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/MANUAL.html) · [체크섬](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.2.0-beta.1/SHA256SUMS.txt) · [이전 1.1.2-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.1.2-beta.1)

개인 Raven IR, NAM 가중치와 개인 캡처 오디오는 공개 패키지에 포함하지 않습니다. Windows 퍼블리셔 서명과 macOS Developer ID 서명·공증은 아직 제공하지 않습니다. 기존 무료 공개 베타 정책을 유지하며, 향후 유료화와 무료 지원 종료 가능성이 있습니다.

## English summary

Open Beta 1.2 adds the five-channel Náströnd Original amp with independent 13-knob channel memories, five complete PRE/amp/cab/POST rigs, and OUTPUT 0 dB across all 43 factory recalls. It incorporates the owner's high-gain references and further balances channel audibility through hot PRE/POST processing. Fimbulvetr has less low-mid congestion and clearer attack. Ambient Clean obtains level through gentle POST compression. Existing parameter identities and stored settings remain intact, while revised DSP can change existing session tone and level. Tests use synthetic signals; new recorded-DI, hardware-equivalence and universal DAW acceptance are not claimed. Known transpose, capture-reference, signing and notarization limitations remain.

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
