# OBS + atkAudio + Chimera VST3 시험 안내

2026-10-23 KST · [프로젝트 #129](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/129)
· [실행 #130](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/130)

**상태: 테스트 도구/절차 구현. 실제 OBS 호환은 미검증.**
Windows x64부터 시험하고 macOS Universal·Linux x86_64는 별도 기록한다.
아래 절차는 atkAudio VST3 호스트 경로가 기본이다. 전용 필터는 [전용 필터 세션](#전용-필터-세션)과 [별도 계획](OBS_ADDON_PLAN.md)을 따른다.

## 설치와 첫 로딩

1. 테스트할 Chimera 버전과 설치본 SHA-256을 확인하고 VST3를 설치한다.
   사용자 지정 경로를 선택했으면 그 경로를 기록한다.
2. [atkAudio 개발자 배포](https://github.com/atkAudio/PluginForObsRelease/releases)에서
   OS/아키텍처와 OBS 버전에 맞는 애드온을 설치하고 OBS를 재시작한다.
   시험 기록에는 `latest` 대신 실제 OBS·atkAudio 버전 번호를 쓴다.
3. OBS의 시험용 오디오 소스 → 필터 → **atkAudio Plugin Host**를 추가한다.
   `Open Plugin Settings` → `Options` → VST3 검색을 실행하고 Chimera를 로드한다.
   버전에 따라 메뉴명이 달라지면 사용한 호스트 타입/메뉴를 기록한다.
   `PluginHost2`는 별도 경로로 시험하며 이전 호스트 결과를 그대로 승계하지 않는다.
4. OBS·장치·호스트를 우선 48 kHz로 맞추고 실제 채널 수와 버퍼를 기록한다.
   장치 버퍼 128–256 samples는 출발점이며 OBS 필터 블록 크기와 같다고 가정하지 않는다.
5. 처음에는 Transpose Off, 한 인스턴스, Classic에서 신호를 확인한다.
   이는 초기 진단 조건이며 Dual/Matrix/Transpose를 지원 범위에서 제외하는 설정이 아니다.

| OS | 기본 설치 경로 |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3` |
| macOS | `/Library/Audio/Plug-Ins/VST3/SpectralForge Chimera.vst3` |
| Linux DEB | `/usr/lib/vst3/SpectralForge Chimera.vst3` |
| Linux 사용자 TAR | `~/.vst3/SpectralForge Chimera.vst3` |

OBS 기본 `VST 2.x Plug-in` 필터에 Chimera VST3를 넣거나 확장자를 바꾸지 않는다.
Mac은 OBS·atkAudio·VST3의 실제 실행 아키텍처(arm64/x86_64)를 함께 확인한다.

## 오디오 경로

- 기본 시험: 장치/미디어 소스 → atkAudio → Chimera → OBS 녹화 출력.
- ASIO 실연: atkAudio의 장치 I/O 기능에서 입력·모니터·OBS 출력 버스를 명시한다.
  Standalone ASIO 출력을 OBS 애플리케이션 캡처로 항상 얻을 수 있다고 가정하지 않는다.
- 대체 경로: DAW/Standalone 처리 출력 → 인터페이스 루프백 또는 가상 장치 → OBS.
  이 경로는 직접 VST3 호스팅과 별도 시험 기록을 사용한다.
- 처리된 출력과 원본 입력이 동시에 방송 버스에 들어오지 않게 소스 라우팅을 확인한다.
  녹화에 포함되는 경로와 연주자가 듣는 모니터 경로를 구분한다.
- 내장 `.wav`는 합성 채널/타이밍 확인용이다. 악기 DI 청취를 대체하지 않는다.

## 로컬 테스트 킷

Python 3.9 이상, 표준 라이브러리만 사용한다. 네트워크 호출·설치·OBS 설정 변경·로그 자동 수집은 없다.
검사할 VST3 번들과 해당 빌드의 `chimera-build-version.json` 또는
`source_sha`/`version`을 포함한 `build-manifest.json`을 준비한다.
빌드 정보를 공개판에서 확보하지 못했다면 담당 개발자가 정확한 설치본에 대응하는 정보를
제공해야 하며 저장소의 현재 HEAD를 설치본 SHA로 대신 적지 않는다.
이 메타데이터는 **입력한 빌드 정보**다. 도구는 바이너리로부터 소스 SHA를 역검증하지 않는다.
배포물/CI manifest와의 일치는 검토자가 별도로 확인한다.

Windows 예시(한 줄):

```powershell
python Tools/obs_testkit.py init --output obs-session-win --plugin "C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3" --build-info build-manifest.json --obs-version "실제 OBS 버전" --atkaudio-version "실제 atkAudio 버전 및 PluginHost 종류" --os-arch "Windows 11 x64 / 실제 빌드" --audio-path "실제 장치·드라이버·48 kHz·장치 버퍼·채널·모니터 경로"
```

새 폴더만 생성하며 기존 폴더를 덮어쓰지 않는다. `session.json`, `evidence/`,
`signals/`가 만들어지고 13개 검사는 모두 `BLOCKED`로 시작한다.
초기화는 VST3 파일을 실행하거나 호스트 검사를 수행하지 않는다.
심볼릭 링크가 포함된 번들은 현재 해시 수집기가 거부한다.

신호만 만들기:

```sh
python Tools/obs_testkit.py signals --output obs-signals
```

- `stereo-routing.wav`: 48 kHz/16-bit/8초. 1–2초 L 440 Hz, 3–4초 R 880 Hz,
  5초·6초 양 채널 단일 샘플 마커. 최대 레벨 약 −20 dBFS.
- `low-a-b-bursts.wav`: 48 kHz/16-bit/모노/8초. A0/B0 번갈아 0.5초 버스트와
  0.5초 무음, 5 ms 램프. 합성 신호만으로 저음 피치 품질을 승인하지 않는다.
- `signals.json`: 생성 파일 해시. 타이밍 비교는 녹화 시작 시각 대신 입력 마커와
  출력 마커를 정렬한다. 손실 압축 오디오의 인코더 지연을 분리하려면 PCM 기록을 함께 사용한다.

## 전용 필터 세션

atkAudio 대신 전용 OBS 필터([OBS/README.md](../OBS/README.md), [#131](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/131))를
시험할 때는 `--host chimera-obs`로 따로 세션을 만든다. atkAudio 세션의 결과를 옮겨 쓰지 않는다.
OBS 오디오 소스 → 필터 → **SpectralForge Chimera**를 쓰고, `--plugin`에는 필터가 불러오는 Chimera VST3를 준다.

```powershell
python Tools/obs_testkit.py init --host chimera-obs --output obs-filter-win --plugin "C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3" --build-info build-manifest.json --obs-version "실제 OBS 버전" --chimera-obs-version "필터 소스 커밋 또는 Setup 버전" --os-arch "Windows 11 x64 / 실제 빌드" --audio-path "실제 장치·드라이버·48 kHz·채널·모니터 경로"
```

필터는 오디오 10분마다 OBS 로그에 실시간 처리 줄(부하, 20 ms 구간 실시간 초과, 입력 끊김, 처리하지 못한 블록)을 남긴다.
1시간 녹화 뒤 OBS의 **도움말 → 로그 파일 → 로그 폴더 보기**에서 그 세션의 로그 파일을 골라 다음을 실행한다.

```powershell
python Tools/obs_testkit.py obs-log --session obs-filter-win/session.json --log "저장한 OBS 로그 파일" --output evidence/realtime-1h.json
python Tools/obs_testkit.py record --session obs-filter-win/session.json --case one_hour_recording --result PASS --duration-seconds 3600 --note "실제 환경·드롭아웃·시작/끝 싱크·OBS 통계 창 CPU/메모리" --evidence evidence/realtime-1h.json --evidence evidence/sync.txt
```

`obs-log`는 `[chimera-obs]` 실시간 줄만 복사하고 로그의 나머지(경로·계정·스트림 설정)는 저장하지 않는다.
전용 필터 세션에서 `one_hour_recording`을 PASS로 기록하려면, 한 필터가 처리한 오디오가 3600초 이상이고
처리하지 못한 블록이 0인 `obs-log` 기록이 증거에 있어야 한다. 실시간 초과·입력 끊김 수와 시작/끝 영상 싱크는 사람이 판단해 메모에 쓴다.
시작/끝 싱크는 녹화 처음과 끝의 박수·마커를 영상 프레임과 비교한다. 필터 처리 지연은 속성 창에 표시되며 OBS가 자동 보정하지 않는다.

## 반드시 수행할 검사

| ID | 절차와 기록할 결과 |
|---|---|
| scan_load | 새 OBS 세션에서 검색·로드·실제 출력 확인. 이름/버전/아키텍처 기록 |
| mono_stereo | L/R 신호와 모노 입력으로 채널 교차·손실·예상치 않은 게인 확인 |
| editor_lifecycle | UI 20회 열기/닫기, 배율/작은 화면 조작; 정지·크래시·제어 반영 확인 |
| preset_ir_recall | .chimera와 개인/임베디드 IR, A/B 상태 저장·재로딩 비교 |
| scene_restart | 장면 저장→OBS 종료→재시작; 프리셋/IR/모드/레벨 복원 비교 |
| classic_dual_matrix | 동일 DI로 세 모드, 게이트·EQ·CAB·PRE/POST 실제 출력 비교 |
| bypass | 재생 중 필터/Chimera 우회 전환, dry/wet 정렬·클릭·무음·레벨 변화 기록 |
| transpose_sync | Off/0/−2/+2에서 보고 지연·녹음 지연·영상/오디오 오프셋을 별도 기록 |
| multiple_instances | 서로 다른 두 소스/프리셋/IR의 독립성·CPU·상태 복원 확인 |
| one_hour_recording | 최소 3600초 녹화, 시작/끝 싱크·드롭아웃·CPU/메모리·설정 기록 |
| audio_restart | 입력 중단/재개·장치 재연결과 44.1↔48 kHz 변경 후 복구/상태 확인 |
| remove_source_exit | 재생 중 필터/소스 제거·복제 및 OBS 종료. worker/UI 해제·크래시 확인 |
| real_di_listening | 실제 기타·베이스 DI와 연주 모니터링. 저역·어택·잡음·청감 결과 기록 |

검사별 실제 결과를 기록한다. 미실행은 `BLOCKED`, 재현된 문제는 `FAIL`이다.
Transpose의 기존 약 43–46 ms 공개판 지연을 호스트 고장으로 혼동하지 않고,
새 후보의 ≤10 ms 목표 충족 여부는 해당 DSP 실측과 연결한다. OBS 버퍼 추가 지연이나
영상 싱크 조절로 알고리즘 지연이 줄었다고 주장하지 않는다.

녹음·스크린샷·검토한 로그 발췌를 `evidence/`에 직접 넣는다.
프로필 전체나 스트림 키·계정·사설 경로가 있는 로그를 넣지 않는다.
검사 기록 예시:

```sh
python Tools/obs_testkit.py record --session obs-session-win/session.json --case scan_load --result PASS --note "실제 수행 절차와 관찰 결과" --evidence evidence/scan-load.txt
python Tools/obs_testkit.py record --session obs-session-win/session.json --case one_hour_recording --result PASS --duration-seconds 3600 --note "실제 환경·드롭아웃·시작/끝 싱크 결과" --evidence evidence/soak-summary.txt
python Tools/obs_testkit.py report --session obs-session-win/session.json --plugin "C:\Program Files\Common Files\VST3\SpectralForge Chimera.vst3"
```

`record`는 수동 기록이다. 증거 내용·품질·진실성을 자동 판독하지 않는다.
`report` 종료값은 1=실패/손상, 2=미실행 잔여, 0=기록 완비·사람의 검토 필요다.
전부 PASS로 입력해도 `REVIEW_REQUIRED`, `host_compatibility_certified=false`를 반환한다.
증거 파일을 수정했다면 재검토 후 해당 검사를 다시 기록해야 한다.
업데이트로 VST3/호스트 버전이 바뀌면 새 폴더에서 시험하며 이전 PASS를 복사하지 않는다.

## 근거

- [OBS 기본 VST2 필터](https://obsproject.com/kb/vst-2-x-plugin-filter)
- [atkAudio 개발자 기능/설치 안내](https://github.com/atkAudio/PluginForObsRelease)
- [OBS 플러그인 안내](https://obsproject.com/kb/plugins-guide)
- [키메라 설치 경로](INSTALLATION.md)
