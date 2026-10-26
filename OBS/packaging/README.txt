SpectralForge Chimera OBS 필터 / OBS filter
==========================================

[한국어]

이 폴더는 Chimera Setup이 설치한 OBS Studio용 SpectralForge Chimera 필터입니다.
필터는 Chimera 제품 소리를 직접 담고 있지 않고, 같이 설치한 Chimera VST3를 불러옵니다.
GPL-2.0-or-later 오픈소스 모듈입니다(LICENSE.txt, 소스 위치는 SOURCE.txt).

사용
1. OBS를 다시 시작합니다.
2. 오디오 소스(마이크·오디오 인터페이스 입력 등)를 오른쪽 클릭 → 필터.
3. 오디오 필터 + → SpectralForge Chimera.
4. "Chimera 에디터 열기"로 소리를 만듭니다. 바꾼 소리는 장면에 바로 저장됩니다.

속성 창
- Chimera VST3 번들: 비우면 Setup이 기록한 VST3 위치, 그다음 표준 VST3 폴더를 찾습니다.
- 입력: 스테레오 그대로, 왼쪽/오른쪽 채널을 양쪽으로, 또는 합. 기타·베이스를 입력 1에 꽂았다면 "왼쪽 채널을 양쪽으로".
- Chimera가 처리할 수 없을 때: 기본은 무음입니다. 가공되지 않은 소리가 방송에 나가지 않게 합니다.
- 상태: Chimera 버전, 처리 지연, 필터 버전과 빌드·시험한 OBS 버전.
- 실시간 처리: Chimera 부하, 실시간보다 오래 걸린 구간, 입력 끊김, 처리하지 못한 블록.
  같은 값이 오디오 10분마다 OBS 로그(도움말 → 로그 파일)에 남습니다.

알아둘 점
- OBS는 플러그인 지연을 자동으로 보정하지 않습니다. 영상과 어긋나면 소스의 고급 오디오 속성에서 싱크 오프셋을 맞춥니다.
- 필터를 껐다 켜거나 입력이 1초 넘게 끊겼다 돌아오면 이전 소리의 꼬리(딜레이·리버브)를 지웁니다.
- 장면에는 필터당 약 0.7 MB의 Chimera 상태가 저장됩니다.

설치·업데이트·제거 (Windows Chimera Setup, macOS Chimera PKG의 "OBS Studio filter" 항목. 별도 macOS·Linux 패키지는 같이 들어 있는 INSTALL.txt)
- 설치하거나 업데이트할 때는 OBS를 먼저 종료합니다. OBS가 실행 중이면 Setup이 멈추고 종료를 요청합니다.
- Windows WMI 오류로 OBS 실행 여부를 확인하지 못하면 설치·제거를 차단합니다. WMI를 복구한 뒤 다시 시도하세요.
- Windows Setup을 다시 실행하면 이전 선택을 기억합니다. Windows에서 선택을 해제하면 Setup이 설치한 필터 파일만 지웁니다.
- macOS PKG의 선택 해제는 기존 필터를 제거하지 않습니다. 제거하려면 OBS 사용자로 "/Library/Application Support/SpectralForge/Chimera/OBS/install.sh" --uninstall 을 실행합니다.
- Chimera를 제거하면 이 폴더를 지웁니다. OBS 장면·프로필·다른 플러그인은 건드리지 않으며,
  장면에 저장된 Chimera 소리도 남습니다. 다시 설치하면 그 소리를 그대로 불러옵니다.

문제가 있을 때
- 필터 목록에 없으면: OBS를 다시 시작하고, 이 폴더의 bin\64bit\chimera-obs.dll이 있는지 확인합니다.
- "Chimera VST3가 로드되지 않음": Chimera VST3 설치 위치를 속성 창에 직접 지정합니다.
- 보고할 때는 OBS 로그의 "[chimera-obs]" 줄(필터 버전·OBS 버전 포함)을 함께 보내 주세요.

[English]

This folder is the SpectralForge Chimera filter for OBS Studio, installed by Chimera Setup.
It contains none of the product's DSP: it loads the Chimera VST3 installed alongside it.
It is an open-source GPL-2.0-or-later module (LICENSE.txt; source location in SOURCE.txt).

Use: restart OBS, right-click an audio source → Filters → + → SpectralForge Chimera, then
"Open Chimera Editor". Edits are stored in the scene at once. Leave the bundle path empty to
use the VST3 folder recorded by Setup. "When Chimera cannot process" mutes by default so
unprocessed audio never reaches the stream. The properties show the Chimera version, the
processing delay (OBS does not compensate it; use the source's sync offset), the filter
version, the OBS version it was built and tested for, and realtime counters that are also
written to the OBS log every 10 minutes of audio.

Close OBS before installing, updating or removing; Setup stops and asks if OBS is running.
If Windows WMI cannot verify OBS process state, installation/removal is blocked. Restore WMI and retry.
(On macOS, the Chimera PKG offers it as the "OBS Studio filter" choice; separate macOS and Linux
packages install with the steps in their INSTALL.txt.)
Removing Chimera deletes this folder only. OBS scenes, profiles and other plugins are not
touched, and the Chimera sound stored in your scenes is kept for a later reinstall.
When reporting a problem, include the "[chimera-obs]" lines from the OBS log.
