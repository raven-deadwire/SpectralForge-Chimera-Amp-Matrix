# SpectralForge Chimera — Open Beta 1.1.2 · Day-one patch

**Version `1.1.2-beta.1` · Windows / macOS / Linux · 2026-10-05 KST**

1.1.1에서 보고된 약한 하이게인 기본 음색을 보정하는 데이원 패치입니다.

## 주요 변경

- **하이게인 기본값:** 5150/6505, VH4, Fortin을 포함한 리드·하이게인 채널의 게인과 입력 응답을 조정했습니다. 약한 입력에서의 포화와 압축을 별도로 측정했습니다.
- **기본 채널:** ZUTA는 CH3로 시작합니다. Rectifier CH3는 Modern, CH2는 Vintage가 기본입니다.
- **실제 native 엔진 대조:** 활성 앰프 23개를 점검하고, 확보한 22개 모델군의 고유 NAM 145개를 공식 NAM Core로 렌더링했습니다. 22개 모델군에 채널별 입력·톤 보정을 적용했습니다. 보정은 확보된 채널과 캡처 범위에 한정됩니다.
- **프리셋:** 하이게인 Factory와 기타 시그니처의 게인을 재조정하고, 전체 38개 프리셋의 출력 레벨을 다시 측정했습니다. Wild Hunt의 상단 대역과 버스 컴프레서도 피크 여유를 확보하도록 조정했습니다.
- **렌더러:** 원래의 native 노브·채널 상태를 직접 렌더링할 수 있게 했습니다. 기존 WAV에 다시 렌더링할 때 예전 데이터가 남던 문제를 수정했습니다.
- **Ironball:** 폐기 상태를 유지하며 NAM 교정에서 제외했습니다. 기존 프로젝트를 위한 내부 호환 슬롯은 유지합니다.

## 교정 범위와 남은 항목

EICH T900의 정확한 원본 NAM은 아직 확보하지 못했습니다. ENGL은 제공된 E670FE EL34, SVT-CL은 제공된 개조 헤드, SUNN은 제공된 1998 리이슈를 주 기준으로 반영했습니다. SVT-CL의 순정 회로나 SUNN의 1970년대 회로와 동등하다는 의미는 아닙니다. 6505 1992 Original과 VH4 전체 앰프 캡처를 주 기준으로 삼고, 프리앰프 전용·6534·EVH 변형은 보조 비교로 구분했습니다.

클론, DI 출력, 복합 체인 캡처의 범위와 ZUTA의 작성자 설명·메타데이터 불일치는 [모델별 비교 보고서](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/v1.1.2-beta.1/docs/NATIVE_NAM_CALIBRATION.md)에 기록했습니다. 합성 플럭·코드·다중 레벨 사인파로 측정했으며, 실물 동등성이나 이번 음색의 실제 DI 청음 승인을 주장하지 않습니다. NAM 가중치와 개인 캡처 오디오는 배포하지 않습니다.

## 기존 프로젝트와 업데이트

저장된 채널·노브 값과 파라미터 ID는 유지합니다. DSP 자체가 바뀌므로 기존 세션도 소리가 달라질 수 있습니다. 새로운 프리셋 설정과 출력 보정은 Factory/Signature를 다시 불러올 때 적용됩니다. 중요한 세션은 업데이트 전 렌더와 설정을 보관해 비교하세요.

Standalone에서 자동 업데이트 확인을 켠 사용자는 다음 확인 때 1.1.2를 안내받습니다. 자동 확인 간격은 최소 24시간이며, 즉시 확인은 SETTINGS → Manual / Bug report / Updates에서 가능합니다. 다운로드와 설치는 사용자가 진행합니다. VST3/AU에서는 릴리즈 페이지를 엽니다.

## 기존 한계와 검증

세 운영체제의 최종 소스 빌드·CTest·패키지 검사와 Windows의 설치·복구·제거 검사를 통과한 파일만 게시합니다. 1.1.1의 개발자 실사용 확인을 이번 음색에 대한 새로운 청음 승인으로 재사용하지 않습니다. Studio One·Cubase·Sonar의 모든 환경에 대한 호환성 보증은 아닙니다.

Transpose 개선은 계속 장기 과제입니다. 알고리즘 지연은 약 43–46 ms이며, 베이스 B현 −2반음에서 음의 몸통과 연속 어택이 뭉개질 수 있습니다. 이 패치는 피치 엔진을 변경하지 않습니다.

## 설치 및 파일

Windows는 DAW와 Chimera를 종료한 뒤 Setup에서 앱·VST3 경로를 선택하세요. 사용자 지정 VST3 폴더는 DAW 검색 경로에도 추가해야 합니다. 설치 후 SETTINGS에서 **Open Beta 1.1.2**과 build 값을 확인하세요.

| 운영체제 | 다운로드 |
| --- | --- |
| Windows x64 | [Setup](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SpectralForge-Chimera-1.1.2-beta.1-win64-Setup.exe) · [Portable ZIP](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SpectralForge-Chimera-1.1.2-beta.1-win64.zip) |
| macOS Apple Silicon / Intel | [Universal PKG](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SpectralForge-Chimera-1.1.2-beta.1-macos-universal.pkg) |
| Linux x86_64 | [DEB](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SpectralForge-Chimera-1.1.2-beta.1-linux-x86_64.deb) · [TAR.GZ](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SpectralForge-Chimera-1.1.2-beta.1-linux-x86_64.tar.gz) |

[오프라인 매뉴얼](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/MANUAL.html) · [SHA-256 체크섬](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.1.2-beta.1/SHA256SUMS.txt)

개인 Raven IR과 NAM 캡처는 공개 패키지에 포함하지 않습니다.

Windows는 퍼블리셔 코드서명 전이며 macOS는 Developer ID 서명·공증 전입니다. 기존 무료 공개 베타 정책을 유지하며, 향후 유료화와 무료 지원 종료 가능성이 있습니다.

## English summary

Open Beta 1.1.2 is the day-one high-gain and native-amp calibration patch. It strengthens high-gain defaults and presets, starts ZUTA on CH3, compares 145 distinct NAM files across 22 model families, applies bounded corrections to 22 families, and rebalances all 38 presets. Ironball remains retired. An exact EICH T900 reference remains missing. Supplied ENGL FE EL34, modified SVT-CL and 1998 SUNN references are incorporated with circuit/revision limits; secondary preamp and amplifier-variant comparisons are kept separate. Measurements use synthetic stimuli, not new recorded-DI or hardware-equivalence acceptance. Existing sessions retain parameter values but can sound different with the corrected DSP. Known transpose, signing and notarization limits remain.

[1.1.1 release notes](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.1.1-beta.1)

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
