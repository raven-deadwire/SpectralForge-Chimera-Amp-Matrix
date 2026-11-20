# SpectralForge Chimera OBS 필터 — 테스트 빌드 안내

## 이 빌드는

OBS Studio의 오디오 소스에 **SpectralForge Chimera** 앰프·이펙트를 거는 필터입니다. 필터는 소리를 직접 만들지 않고, 같은 컴퓨터에 설치된 Chimera VST3를 불러와 처리합니다.

**공개 출시 전 테스트 빌드입니다.** 방송·녹화에 바로 쓰기 전에 먼저 테스트 장면에서 써 보세요. 문제가 생겨도 OBS 장면·설정·다른 플러그인은 보존되도록 설치기가 만들어져 있지만, 중요한 프로필은 미리 백업해 두기를 권합니다.

- 필터 버전: 0.1.0 (소스 `5bbc2715df`)
- 다운로드: [SpectralForge Chimera 1.4.1-beta.1 릴리스](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.4.1-beta.1) 페이지 아래 Assets에서 받으세요.
  - macOS: `chimera-obs-macos-universal-5bbc2715df.zip`
  - Linux: `chimera-obs-linux-x86_64-5bbc2715df.tar.gz`
  - 같은 페이지의 Chimera 1.4.1-beta.1 설치본(VST3 포함)도 함께 설치해야 합니다.
- 회신: [GitHub 이슈](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/new)로 남겨 주세요. 제목은 `[OBS 테스트] 운영체제 — 한 줄 요약` 형식으로 써 주세요. 이슈는 공개되므로, 로그에 보이는 사용자 이름·폴더 경로는 가린 뒤 올려 주세요.
- 기한: **2026년 12월 4일(금)**까지

## 테스트 대상 환경

| 운영체제 | 조건 | 빌드·시험한 OBS |
|---|---|---|
| **macOS** | macOS 12 이상, Apple silicon 또는 Intel Mac | OBS Studio 31.1.2 |
| **Linux** | x86_64, glibc 2.38 이상(Ubuntu 24.04 수준), **배포판 패키지로 설치한 OBS**, 에디터 창은 X11(Wayland에서는 XWayland) 필요 | OBS Studio 30.0.2 (Ubuntu 24.04) |

- Linux의 **Flatpak·Snap OBS는 지원하지 않습니다**(외부 플러그인 폴더를 읽지 않음).
- Intel Mac은 CI에서 Rosetta로만 시험했습니다. **실제 Intel Mac, macOS 12·13·14 사용자의 결과가 특히 필요합니다.**

## 먼저 필요한 것

1. **Chimera VST3 설치** — 필터가 이 VST3를 불러옵니다.
   - macOS: `/Library/Audio/Plug-Ins/VST3` 또는 `~/Library/Audio/Plug-Ins/VST3`
   - Linux: `~/.vst3`, `/usr/local/lib/vst3` 또는 `/usr/lib/vst3`
2. OBS Studio 설치 (위 표 참고)
3. Python이나 컴파일러는 **필요 없습니다.** 패키지에 설치 프로그램이 들어 있습니다.

## 받은 파일 확인 (선택)

받은 파일이 손상되지 않았는지 SHA-256으로 확인할 수 있습니다. 릴리스의 `chimera-obs-SHA256SUMS.txt`에도 같은 값이 있습니다.

| 파일 | SHA-256 |
|---|---|
| macOS `chimera-obs-macos-universal-5bbc2715df.zip` | `814423de083c6b3c08e032961a053f43afdf5b432647c0c6e8ab320b68286787` |
| Linux `chimera-obs-linux-x86_64-5bbc2715df.tar.gz` | `6faf089e8543b77dfa616698aa7ac5f8e836c5212dfd97257a9bffb37ff020c2` |

```sh
shasum -a 256 <파일>     # macOS
sha256sum <파일>         # Linux
```

## 설치

**설치·업데이트·제거하는 동안 OBS를 꼭 종료해 두세요.** OBS가 실행 중이면 설치기가 아무것도 바꾸지 않고 멈춥니다. 관리자(root/sudo) 권한으로 실행하지 마세요.

### macOS

1. OBS를 종료합니다.
2. 받은 zip을 풀고, 터미널에서 풀린 패키지 폴더로 이동합니다.
3. `./install.sh` 를 실행합니다.
   - `~/Library/Application Support/obs-studio/plugins/chimera-obs.plugin` 에 설치됩니다.
   - 이 모듈은 **Apple 공증 전(임시 서명)** 입니다. 설치기가 서명을 확인한 뒤, 포함된 파일에서만 다운로드 격리 표시를 지웁니다. 파일을 직접 플러그인 폴더에 복사하지 마세요.

### Linux

1. OBS를 종료합니다.
2. 받은 파일을 풀고(`tar -xzf chimera-obs-linux-x86_64-*.tar.gz`), 풀린 폴더로 이동합니다.
3. `./install.sh` 를 실행합니다.
   - `~/.config/obs-studio/plugins/chimera-obs` 에 설치됩니다.

## 사용

1. OBS를 시작합니다.
2. 오디오 소스(마이크·오디오 인터페이스 입력)를 오른쪽 클릭 → **필터**.
3. 오디오 필터 **+** → **SpectralForge Chimera**.
4. 필터 속성의 **"Chimera 에디터 열기"**로 Chimera 화면을 별도 창에서 엽니다. 창을 닫아도 소리는 유지되고, 편집 내용은 장면에 저장됩니다.
   **Linux:** 에디터 창은 X11이 필요합니다. OBS를 Wayland로 실행 중이면 XWayland가 있어야 열립니다. 창이 열리지 않으면 OBS 로그의 `[chimera-obs]` 줄에 이유가 남습니다.

알아둘 점
- 기타·베이스를 인터페이스 입력 1에 꽂았다면 속성의 입력을 **"왼쪽 채널을 양쪽으로"**로 바꾸세요.
- OBS는 플러그인 지연을 자동 보정하지 않습니다. 영상과 소리가 어긋나면 소스의 **고급 오디오 속성 → 싱크 오프셋**으로 맞추세요. 지연 값은 필터 속성의 "상태"에 나옵니다.
- Chimera가 처리할 수 없는 상황에서는 기본적으로 **무음**이 됩니다(가공 안 된 소리가 방송에 나가지 않도록).

## 확인해 주셨으면 하는 것

1. 설치가 오류 없이 끝나고 OBS 필터 목록에 **SpectralForge Chimera**가 나타나는지
2. 소리가 Chimera를 거쳐 나오는지, 끊김·잡음·지연 문제가 있는지
3. 에디터 창 열기·닫기·다시 열기, 창 크기 조절, 편집한 소리가 OBS를 다시 시작한 뒤에도 남아 있는지 (Linux는 X11인지 Wayland인지도 알려 주세요)
4. 녹화·방송을 30분 이상 했을 때 소리 끊김이나 OBS 멈춤이 있는지
5. 같은 패키지로 다시 설치(업데이트)했을 때 장면·설정이 그대로인지
6. 제거(`./install.sh --uninstall`) 후 OBS가 정상인지

## 보고할 때 함께 보내 주세요

- 운영체제와 버전, Mac이면 Apple silicon/Intel, Linux면 X11/Wayland
- OBS 버전과 설치 방식(공식 앱·배포판 패키지 등)
- **OBS 로그의 `[chimera-obs]` 줄** — OBS 메뉴 **도움말 → 로그 파일 → 현재 로그 보기**
  예: `[chimera-obs] module 0.1.0 (source 5bbc2715df), built and tested for OBS …, running OBS …`
- 설치·제거 중 문제가 있었다면 터미널에 나온 메시지 전체
- 소리 문제라면 사용한 오디오 장치와 샘플레이트, 버퍼 크기

## 알려진 제한

- 실제 OBS 앱에서의 필터 추가·녹화·상태 저장/복원·에디터 창 열기와 편집은 CI 자동 시험(Linux OBS 30.0.2, Windows·macOS OBS 31.1.2)으로 확인했습니다. 실제 PC에서 사람이 하는 조작, 실제 오디오 장치, 방송은 이번 테스트로 처음 확인합니다.
- macOS: Apple 공증 전(임시 서명).
- Linux: 에디터 창은 X11(Wayland에서는 XWayland) 필요. Flatpak·Snap OBS 미지원.
- 실제 Intel Mac과 macOS 12·13·14는 아직 확인 전입니다.
- 정전·저장장치 고장 같은 상황에서의 복구는 보장하지 않습니다.

## 제거

OBS를 종료한 뒤, 설치에 쓴 패키지 폴더에서 `./install.sh --uninstall` 을 실행합니다. 설치기가 넣은 파일만 지우며, OBS 장면·설정·다른 플러그인·Chimera VST3·직접 넣은 파일은 그대로 둡니다.

## 문제가 생기면

- **Linux에서 에디터 창이 열리지 않음:** OBS를 Wayland로 실행 중이면 XWayland가 설치·실행 중인지 확인하고, OBS 로그의 `[chimera-obs]` 줄을 보내 주세요.
- **필터 목록에 없음:** OBS를 완전히 종료 후 다시 시작하세요. Linux는 Flatpak·Snap OBS가 아닌지 확인하세요.
- **"Chimera VST3가 로드되지 않음":** 필터 속성의 "Chimera VST3 번들"에 VST3 위치를 직접 지정하세요.
- **설치기가 "OBS 실행 중"이라며 멈춤:** OBS를 완전히 종료한 뒤 다시 실행하세요.
- **설치가 중간에 끊김(터미널 강제 종료 등):** 같은 `./install.sh`를 다시 실행하면 이전 설치로 복구합니다. 복구가 안 된다는 메시지가 나오면, 표시된 `.chimera-obs-transaction` 폴더를 지우지 말고 그 메시지와 함께 알려 주세요. 이전 설치 백업입니다.
