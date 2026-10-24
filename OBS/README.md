# SpectralForge Chimera OBS 필터 (`chimera-obs`)

[#131](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/131)
· [개발 계획](../docs/OBS_ADDON_PLAN.md) · [경계 검토](../docs/OBS_ADDON_BOUNDARY.md)

OBS 오디오 필터 목록에 **SpectralForge Chimera**를 직접 추가하는 OBS Studio 모듈이다.
모듈은 Chimera DSP를 포함하지 않고, 사용자가 따로 설치한 **Chimera VST3**를 불러와
실행한다(구조 2, 소유자 결정 2026-10-23).

**상태: 개발 시제품. Windows Chimera Setup의 선택 항목으로 같이 설치할 수 있음(서명 없음). 실제 OBS Studio 화면·에디터·방송 검증 미실행.**

## 라이선스

이 디렉터리(`OBS/`)는 **GPL-2.0-or-later**([LICENSE](LICENSE))다. 저장소의 나머지
Chimera 제품 소스·DSP·모델·IR·아트는 기존 독점 조건(`COPYRIGHT.txt`)을 유지하며
이 모듈에 링크되지 않는다. VST3 호스팅은 MIT 라이선스 Steinberg VST3 SDK 3.8을 쓰고
JUCE는 쓰지 않는다.

## 구조

| 파일 | 역할 | 스레드 |
|---|---|---|
| `src/plugin-main.cpp` | OBS 필터 등록, 설정·장면 저장, 속성 창 | OBS UI·오디오·소멸 스레드 |
| `src/audio-bridge.hpp` | OBS planar 오디오 ↔ 엔진 실시간 경계, 입력 매핑, 실패 정책 | 오디오(할당·잠금 없음) |
| `src/realtime-stats.hpp` | 실시간 처리 통계(부하·실시간 초과·입력 끊김·미처리 블록) | 오디오(쓰기), 아무 스레드(읽기) |
| `src/chimera-host.*` | VST3 번들 식별·로딩, 버스/처리 준비, 상태, 파라미터 | UI(로딩), 오디오(처리) |
| `src/editor-window-*.cpp` | Chimera 에디터 창(Windows HWND). 다른 OS는 미지원 표시 | UI |
| `tools/dll_exports_def.py` | Windows용 `obs.dll` export → `.def` (import lib 생성) | 빌드 |

- 이름이 `SpectralForge Chimera`이고 제작사가 `RavenForge Luthier Intelligence`인 VST3 클래스만 로드한다(범용 VST3 호스트 아님).
- 경로를 비우면 Setup이 기록한 VST3 위치(`data/default-vst3-path.txt`, UTF-8)를 먼저 쓰고, 없으면 표준 설치 위치를 찾는다:
  Windows `%CommonProgramFiles%\VST3`, Linux `~/.vst3`, `/usr/local/lib/vst3`, `/usr/lib/vst3`.
- 플러그인 로딩·해제·에디터는 항상 OBS UI 스레드에서 한다. `create`/`update`가 다른 스레드(장면 로딩, obs-websocket)에서 와도 UI 스레드로 넘긴다.
  OBS는 필터를 소멸 스레드에서 없애므로, 해제는 UI 스레드에 **기다리지 않고** 넘긴다(종료 중 교착 방지).
- 장면에는 DAW와 같은 VST3 component/controller 상태를 base64로 저장한다. Chimera가 없는 PC에서 장면을 열어도 저장된 상태를 지우지 않는다.
- 소리를 바꾸면 그 상태를 장면 설정에 바로 써 넣는다(에디터를 연 동안 500 ms마다·창을 닫을 때, 스크립트로 값을 바꿀 때).
  OBS는 소스 복제와 필터 복사·붙여넣기 때 `save`를 부르지 않고 설정만 복사하기 때문이다.
- 설정으로 들어온 상태가 마지막에 적용·저장한 상태와 다르면 실행 중인 필터에 적용한다
  (필터 설정 붙여넣기, 스크립트·obs-websocket의 프리셋 적용).
- 장면 설정에는 형식 버전 `chimera_obs_schema`(현재 1)를 기록한다. 더 새 모듈이 저장한 장면은 상태를 적용하지도 덮어쓰지도 않는다.
- Chimera가 없거나 처리 실패 시 기본은 **무음**, 선택 시 원음 통과. OBS 오디오 형식이 바뀌면 UI 스레드에서 다시 준비한다.
- OBS는 꺼진 필터를 처리하지 않으므로, 필터를 다시 켜면 Chimera에 남은 이전 소리의 꼬리(딜레이·리버브·캐비닛)를 지운다
  (`enable` 신호 → 다음 블록에서 VST3 `setProcessing(false/true)` → Chimera `reset()`). 이 동작은 `reset()`을 구현한 Chimera 빌드가 필요하다.
- 입력이 1초 이상 끊겼다가 다시 들어오면(장치 정지·재연결, 미디어 일시정지) 같은 방식으로 이전 꼬리를 지운다.
  타임스탬프가 5 ms를 넘게 어긋난 짧은 끊김은 통계에만 센다.
- OBS에는 플러그인 지연 보정이 없다. 처리 지연은 속성 창에 표시만 하며 자동 보정하지 않는다.

## 실시간 처리 통계 (D단계)

필터마다 오디오 스레드에서 할당·잠금 없이 센다. Chimera가 처음 소리를 처리한 블록부터 센다
(OBS 시작·Chimera 로딩 중 무음 구간은 빼고, Chimera를 찾지 못한 필터는 오디오 0초로 남는다). 속성 창의 **실시간 처리** 항목과 **실시간 통계 초기화** 버튼,
스크립트용 proc, OBS 로그로 볼 수 있다.

| 항목 | 뜻 |
|---|---|
| Chimera 부하 평균 / 최대 | Chimera 처리 시간 ÷ 오디오 시간. 최대값과 실시간 초과는 오디오 20 ms 구간(OBS 오디오 한 틱 정도)마다 잰다 |
| 실시간보다 오래 걸린 구간 | 20 ms 분량을 처리하는 데 20 ms보다 오래 걸린 구간 수. 드롭아웃 위험 신호 |
| 입력 타임스탬프 끊김 | 앞 블록에서 예상한 시각과 5 ms 넘게 어긋난 횟수. 그중 1초 이상 끊긴 뒤 재개는 따로 센다 |
| 처리하지 못한 블록 | Chimera가 없거나 준비 중·오류로 무음 또는 원음 통과한 블록 수 |

OBS 로그에는 오디오 10분마다 한 줄, 필터를 제거할 때 전체 합계 한 줄이 남는다.
OBS를 종료할 때 합계 줄이 남는지는 실제 OBS 앱에서 확인하지 않았으므로, 1시간 기록은 10분 줄로 판단한다.

```
[chimera-obs] 'Guitar DI': realtime: 0:10:00 of audio, 28125 blocks, Chimera load average 3.1% peak 12.4%, 20 ms windows over real time 0, input gaps 0 (resumed after silence 0), unprocessed blocks 0
```

1시간 녹화 기록은 이 줄로 남긴다. 테스트 킷 `obs-log` 명령이 사용자가 고른 OBS 로그에서 이 줄만 증거 폴더로 옮긴다
([OBS_COMPATIBILITY.md](../docs/OBS_COMPATIBILITY.md#전용-필터-세션)). OBS 전체 CPU·메모리는 OBS의 통계 창으로 따로 기록한다.

## 스크립트 인터페이스

필터 소스의 proc handler로 Chimera 파라미터를 읽고 쓸 수 있다(OBS UI 스레드에서만. 다른 스레드는 `ok = false`).
값은 0–1 정규화 값이고, 바꾼 값은 장면 설정에 바로 반영된다. 파라미터 ID는 Chimera VST3의 파라미터 ID다.

```
void chimera_set_parameter(in int id, in float value, out bool ok)
void chimera_get_parameter(in int id, out float value, out bool ok)
void chimera_get_stats(out float audio_seconds, out int blocks, out float average_load, out float peak_load,
                       out int overruns, out int gaps, out int restarts, out int unprocessed)
```

`chimera_get_stats`는 읽기 전용이라 어느 스레드에서 불러도 된다. 부하 값은 비율이다(0.25 = 25%).

## 빌드

독립 CMake 프로젝트다. 상위 제품 CMake와 섞지 않는다.

```bash
# Linux (Ubuntu 24.04: libobs-dev 30.0.2)
cmake -S OBS -B build-obs -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_OBS_TEST_BUNDLE="$PWD/build/ChimeraAmpMatrix_artefacts/Release/VST3/SpectralForge Chimera.vst3"
cmake --build build-obs
xvfb-run -a ctest --test-dir build-obs --output-on-failure   # libobs가 X 디스플레이를 연다
```

장시간 시험 길이는 `CHIMERA_OBS_SOAK_SECONDS`(오디오 초, 기본 120)로 바꾼다. 예: `CHIMERA_OBS_SOAK_SECONDS=3600`.

Windows는 libobs 개발 패키지가 없으므로, 대상 OBS 릴리스 태그의 `libobs` 헤더와 그 릴리스 `obs.dll`에서
만든 import library로 빌드한다(`-DCHIMERA_OBS_LIBOBS_INCLUDE_DIR`, `-DCHIMERA_OBS_LIBOBS_LIBRARY`).
정확한 절차는 [`tools/Build-WindowsModule.ps1`](tools/Build-WindowsModule.ps1)이다(OBS 31.1.2, 해시 고정, 정적 MSVC 런타임).
OBS 추가 기능·제품 빌드·업데이트 후보 워크플로가 모두 이 스크립트를 쓴다.
VST3 SDK는 `v3.8.0_build_66`을 자동으로 받으며 `-DCHIMERA_VST3_SDK_DIR`로 기존 사본을 지정할 수 있다.

## 설치 (Windows)

Chimera Windows Setup의 작업 선택 화면에서 **OBS Studio용 SpectralForge Chimera 필터도 설치**를 고른다
(VST3 선택 시 표시, OBS가 설치된 PC의 첫 설치에서는 기본 선택, 다시 설치하면 이전 선택 유지).
OBS를 종료한 뒤 설치한다. 다시 설치할 때 선택을 해제하거나 Chimera를 제거하면 Setup이 설치한 필터 파일만 지운다.
설치 폴더에는 사용 안내 `README.txt`(한국어/영어, 원본 [`packaging/README.txt`](packaging/README.txt)),
`LICENSE.txt`(GPL), 대응 소스 위치와 모듈 버전 `SOURCE.txt`가 같이 들어간다.

배포 동작(E단계):
- OBS(`obs64.exe`)가 실행 중이면 필터 설치·업데이트·선택 해제를 하기 전에 Setup이 멈추고 OBS 종료를 요청한다.
  무인 설치는 종료 코드 7로 끝나고, 아무 파일도 바꾸지 않는다. 필터가 설치된 상태에서 OBS가 실행 중이면 제거도 거부한다.
  OBS가 실행 중인지 알 수 없으면(WMI 사용 불가) 막지 않는다.
- Setup은 OBS 장면·프로필(`%APPDATA%\obs-studio`)과 다른 플러그인 폴더를 읽거나 바꾸지 않는다.
  장면에 저장된 Chimera 소리는 제거 후에도 장면 파일에 남고, 다시 설치하면 그대로 불러온다.
- 모듈 버전은 `CMakeLists.txt`의 `project(chimera-obs VERSION ...)`이며, 소스 커밋과 함께 DLL 버전 정보·OBS 로그·속성 창에 나온다.
  로그 예: `[chimera-obs] module 0.1.0 (source 0123456789), built and tested for OBS 31.1.2, running OBS 31.1.2`.
  실행 중인 OBS의 주 버전이 빌드·시험한 버전과 다르면 로그 경고와 속성 창 안내를 남긴다(로딩은 막지 않음).
- 서명: `Build-WindowsInstaller.ps1 -Sign`은 단계 폴더의 모든 `.exe`·`.dll`·`.vst3`에 서명하므로 `chimera-obs.dll`도 포함된다.
  공개 서명 인증서가 아직 없어 현재 배포본은 서명 전이다.

개발용으로는 CI 산출물 `chimera-obs-windows-x64-*`를 다음 위치에 풀고 OBS를 다시 시작한다.

```
%ProgramData%\obs-studio\plugins\chimera-obs\bin\64bit\chimera-obs.dll
%ProgramData%\obs-studio\plugins\chimera-obs\data\locale\*.ini
```

Chimera VST3는 평소처럼 `%CommonProgramFiles%\VST3`에 설치되어 있어야 한다. 오디오 소스 → 필터 →
`SpectralForge Chimera`. 서명·자동 업데이트는 계획 E단계다.

## 검증 기록

| 항목 | 결과 | 환경 |
|---|---|---|
| 실시간 경계 단위 시험·변형 시험 | 통과(GCC ASan/UBSan·TSan, Clang 경고 0) | Linux |
| VST3 호스트·libobs 필터 시험 ASan/UBSan | 통과 | Linux GCC |
| 실제 Chimera 1.3.2 VST3 로딩·식별, 파라미터 7162개 | 통과 | Linux x86_64, 이 커밋에서 빌드한 VST3 |
| 블록 크기 독립성(1–1024 혼합 vs 1024) | 최대 차이 3.0e-8 | 같은 환경 |
| 기본 상태 처리 지연 | 42 samples(48 kHz 약 0.88 ms) | 같은 환경 |
| 상태 왕복(바이트 동일), 연속 파라미터 복원, 모노, 로딩/해제 5회 | 통과 | 같은 환경 |
| libobs 모듈 로더로 로드 → 실제 필터 체인 출력 = 직접 VST3 출력 | 최대 차이 0 | libobs 30.0.2 (헤드리스) |
| 입력 매핑 실시간 변경, 장면 저장/복원, Chimera 없음 무음/원음 | 통과 | 같은 환경 |
| 소멸 스레드 → UI 스레드 해제 5/5, libobs 할당 누수 0 | 통과 | 같은 환경 |
| 편집한 상태가 소스 복제(`obs_source_duplicate`)로 따라감, 복제본과 원본 독립 | 통과 | 같은 환경 |
| 설정으로 들어온 상태 적용, 더 새 형식 장면은 적용·덮어쓰기 안 함, UI 외 스레드 호출 거부 | 통과 | 같은 환경 |
| 위 상태 시험의 변형 시험(동기화·적용·버전 검사를 각각 제거하면 실패) | 3/3 검출 | 같은 환경 |
| 다시 켠 필터의 꼬리 제거(무음 100 ms RMS 0.0074 → 0), 호스트 `requestReset` 0.0079 → 0 | 통과, 신호 연결 제거 시 실패 | 같은 환경 |
| Windows 코드 경로 교차 컴파일(MinGW, OBS 31.1.2 헤더·`obs.dll` export) | 빌드 성공 | 실행 안 함 |
| 실시간 통계 단위 시험(끊김 분류, 20 ms 구간 부하·초과, 주기 보고, 초기화, 동시 읽기) | 통과(TSan 포함) | Linux |
| 1초 넘게 끊긴 입력 재개 시 꼬리 제거(무음 100 ms RMS 0.0074 → 0), 20 ms 끊김은 꼬리 유지·통계만 | 통과 | libobs 30.0.2 (헤드리스) |
| 장시간: 오디오 120초(CI 기본), 무작위 블록 1–1024 frames 11,244개, 10초마다 입력 매핑 변경 | 미처리 0, 출력 유한, 같은 구간 시작/끝 출력 차이 0(지연·상태 누적 없음) | 같은 환경 |
| 장시간 600초(`CHIMERA_OBS_SOAK_SECONDS=600`), 블록 56,321개 | 시작/끝 출력 차이 0, 상주 메모리 +2.9 MiB(60초 실행 +2.6 MiB와 같은 수준, 누수 없음) | 같은 환경 |
| OBS 44.1 kHz 모노, 48 kHz 5.1, 48 kHz 스테레오에서 새 필터 준비·처리, 3번째 이후 채널은 그대로 통과 | 통과 | 같은 환경 |
| Chimera 직접 처리 60초(무작위 블록) 20 ms 구간 실시간 초과 | 0회(2회 실행), 블록당 최대 약 6 ms | Linux x86_64, 4코어 |
| Setup이 기록한 VST3 위치 파일(BOM·CRLF 포함)로 경로 없이 로딩, 파일 처리 제거 시 실패 | 통과 | libobs 30.0.2 (헤드리스) |
| Windows MSVC 빌드·시험(OBS 31.1.2 `obs.dll`) | CI에서 실행 | 결과는 CI 기록 |
| Setup: OBS 실행 중 설치·선택 해제 중단(종료 코드 7)·제거 거부, OBS 장면 파일·다른 플러그인 보존, DLL 버전 정보 | CI(`Verify-WindowsInstaller.ps1`)에서 실행 | 결과는 CI 기록 |
| Setup 필터 선택 설치·복구 시 선택 유지·사용자 지정 유니코드 VST3 경로 기록·선택 해제 시 제거·제거 시 삭제, 정적 런타임 | CI(`Verify-WindowsInstaller.ps1`)에서 실행 | 결과는 CI 기록 |

장면 하나에 필터당 상태가 약 0.7 MB(base64) 저장된다(기본 상태 531,818바이트).

헤드리스 시험은 오디오를 실시간보다 빠르게 넣으므로 그 안의 부하·실시간 초과 수치는 기록만 하고 판정에 쓰지 않는다
(같은 환경에서 평균 부하 약 20–25%, 20 ms 구간 실시간 초과는 실행마다 0–5회). 시험의 메모리 검사는 AddressSanitizer 빌드에서는 건너뛴다. 실제 판정은 OBS 앱의 1시간 녹화 로그로 한다.

**알려진 한계**

- 레벨 미터는 OBS 자체 오디오 믹서 미터(필터 적용 후)를 쓴다. 속성 창에는 실시간 미터를 넣지 않았다.
- 개인 IR 파일이 없을 때의 처리는 Chimera 제품 동작을 따른다. 모듈은 상태를 그대로 전달할 뿐이다.
- 에디터 편집의 장면 반영(500 ms 타이머)은 Windows 에디터 코드에 있으며 실제 OBS 앱에서는 아직 확인하지 않았다.

- OBS는 실행 중에 출력 샘플레이트·채널을 바꾸지 않는다(설정 변경은 재시작 후 적용). 실행 중 형식 변경 대응은
  실시간 경계 단위 시험으로만 확인했다. libobs는 소스가 살아 있는 동안 오디오를 다시 설정하면 그 소스의 변환을 갱신하지 않기 때문이다.
- 필터는 OBS 타임스탬프를 바꾸지 않는다. 처리 지연(기본 상태 42 samples, 0.88 ms)은 영상과의 싱크에 그대로 더해지며,
  필요하면 OBS 소스의 싱크 오프셋으로 맞춘다.

**미실행**: OBS Studio 앱 안에서의 필터 메뉴·속성 창·에디터 창(Windows 에디터 코드는 실행 검증 전),
실제 오디오 장치·DI 입력, 실제 OBS 1시간 녹화와 시작/끝 영상 싱크 측정, OBS 32.x, macOS 빌드. 이 항목은 계획 C–F단계와
[#130](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/130)의 수동 검사로 기록한다.
