# Chimera 형식 확장·OBS 프로젝트

사용자 승인: 2026-10-23 KST. 관리 이슈: [#129](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/129).
기준 소스: `11c350721d652bfba53bdfa24f201b6e70a29483`.
프로젝트 등록·도구 구현과 실제 호스트 승인·공개 배포는 별도 상태다.

## 우선순위와 산출물

| 트랙 | 현재 단계 | 다음 산출물 | 완료 기준 |
|---|---|---|---|
| AAX Native P0 | 기존 개발·서명 작업 유지, #97 | 서명된 Windows/macOS 설치 후보 | 일반 Pro Tools 로딩·자동화·세션 복원·PDC 실측 |
| OBS + atkAudio + VST3, #130 | 1단계 도구/절차 구현 | 바이너리/호스트 버전을 고정한 Windows 시험 기록 | 13개 검사와 실제 오디오·장시간 녹화 검토 |
| 전용 OBS 애드온, #131 | 사용자 승인된 독립 개발 범위·설계 | Windows 필터 시제품의 구조·라이선스 결정 | 공통 엔진·프리셋·장면 복원·지연·수명주기 검증 |
| CLAP | 후속 개발 | Windows/macOS/Linux 선택형 빌드 | 기존 파라미터·IR·상태 호환 및 실제 CLAP 호스트 시험 |
| LV2 | CLAP 다음 | Linux 선택형 빌드·설치 | LV2 식별자/상태/UI/호스트 검증 |
| AudioSuite·AUv3 | 후속 범위 검토 | 오프라인 처리 계약·iPad 성능 시제품 | tail/지연 또는 모바일 CPU·메모리·입출력 실측 |
| Android·HDX | 타당성 연구 | 기기/플랫폼 제약과 지원 가능 기능 | 실측 전 전체 기능·동일 성능을 약속하지 않음 |

전용 OBS 애드온은 atkAudio 실패 때만 수행하는 조건부 대안이 아니다.
호환 시험과 별도로 설계를 진행하며, 공통 테스트 입력·프리셋·관찰 결과를 재사용한다.
AAX와 기존 Transpose/EQ/베타 P0의 우선순위는 유지한다.
이 변경에서 기존 제품 버전·만료일·릴리스 판정·모델 수·장기 달력은 변경하지 않는다.
각 트랙의 실제 소스·빌드·실사용 증거가 확보된 뒤 버전과 공개일을 배정한다.

## 이번 착수분

- [OBS 시험 안내](OBS_COMPATIBILITY.md): 설치, 48 kHz 라우팅, 13개 검사, 측정 기록.
- `Tools/obs_testkit.py`: 합성 WAV 생성, 명시적으로 지정한 VST3 해시,
  제품 빌드 정보와 OBS/atkAudio 버전 기록, 수동 증거의 무결성 검사.
- `Tests/test_obs_testkit.py`: 채널/타이밍, 누락된 증거, 파일 변조·경로 이탈,
  1시간 조건 및 미실행 상태 유지 검사.
- `OBS compatibility test kit` CI: 세 OS에서 도구 검사 후 테스트 킷 업로드.
- [전용 OBS 애드온 설계](OBS_ADDON_PLAN.md).

현재 작업 환경에 OBS가 없으므로 실제 스캔·오디오·UI·녹화 검사는 미실행이다.
CI 이름과 결과는 도구 자체 검사이며 실제 OBS 호환 통과를 뜻하지 않는다.

## 공통 계약

1. DSP, 파라미터 ID, `.chimera` 프리셋, 개인/임베디드 IR, 베타 상태를 공유한다.
   형식 간 프리셋 호환이 DAW 인스턴스 자동 교체를 뜻하지는 않는다.
2. CPU·지연은 알고리즘, 호스트/어댑터 버퍼, 장치, 캡처/영상 동기를 구분한다.
3. 오디오 콜백에서 파일 I/O·다운로드·힙 할당/해제·UI 호출·잠금 대기를 피한다.
4. VST2 배포는 기존 계약 확인 없이 추가하지 않는다. OBS용 DLL은 별도 모듈이다.
5. 설치본 수를 늘리기 전에 서명·공증·업데이트·개인 파일 보존을 검증한다.
6. 원본 OBS 장면/프로필/전체 로그를 자동 수집하지 않는다. 검토한 증거만 명시적으로 선택한다.

## 작업 추적

- [#97 기존 AAX 포함 P0](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97)
- [#130 OBS VST3 호환 시험](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/130)
- [#131 전용 OBS 애드온](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/131)

실패는 실패 조건/소스/호스트 버전과 함께 해당 이슈에 추적한다.
Windows 시험 결과를 macOS/Linux 승인으로 복사하지 않는다.
