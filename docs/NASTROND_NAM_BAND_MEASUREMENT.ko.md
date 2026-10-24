# 실제 NAM 캡처로 Náströnd band 지표 측정하기

[English](NASTROND_NAM_BAND_MEASUREMENT.md)

[NASTROND_NAM_TOOLCHAIN.ko.md](NASTROND_NAM_TOOLCHAIN.ko.md)에서 다음 측정으로 남겨 둔 작업의 절차입니다.
매니페스트의 NAM 캡처 26개와 OriginalAmp 상태들을 렌더링하고, 옥타브마다 같은 비중을 주는
`band_spectrum_rms_db` 지표와 기존 v1 지표로 각각 매크로 방향을 audit해서 두 결과를 비교합니다.

결과는 관찰 기록일 뿐입니다. 하드웨어와 같은 소리라거나 오너 청음을 통과했다는 인증이 아닙니다.

## 저장소에 올리지 않는 것

NAM 가중치, 렌더링한 오디오, 렌더 캐시는 저장소 밖의 개인 폴더에 둡니다. 결과로 나오는
`comparison.json`과 `audit-*.json`에는 가중치나 오디오가 들어 있지 않으므로 증거로 커밋할 수
있습니다. 아래 경로는 예시입니다.

## 1. 준비

```sh
pip install numpy scipy

# NAM 렌더러: 매니페스트에 기록된 커밋(nam_core_commit)으로 빌드
git clone https://github.com/sdatkinson/NeuralAmpModelerCore.git ~/nam-core
cd ~/nam-core
git checkout 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target render
# -> ~/nam-core/build/tools/render

# OriginalAmp 렌더러: 측정할 소스에서 빌드
cd ~/SpectralForge-Chimera-Amp-Matrix
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ChimeraOriginalAmpRender
ctest --test-dir build -R ChimeraOriginalRendererTests --output-on-failure
```

## 2. NAM 받기와 렌더링

```sh
python Tools/nastrond_nam_tool.py fetch --out ~/private/nastrond-nam

python Tools/nastrond_nam_tool.py compare \
  --models ~/private/nastrond-nam \
  --nam-render ~/nam-core/build/tools/render \
  --original-render build/ChimeraOriginalAmpRender_artefacts/Release/ChimeraOriginalAmpRender \
  --states docs/evidence/nastrond-channels-20261005/channel-states.json \
  --out ~/private/m-main --workers 4
```

- `fetch`는 SHA256이 매니페스트와 맞는 파일만 저장합니다. 이미 있는 파일은 `VERIFIED`로 표시합니다.
- `--states`를 주면 고정된 17개 상태에 채널 상태 22개가 더해져 모두 39개가 됩니다.
- 렌더 결과는 `--out` 폴더에 캐시됩니다. 다시 실행하면 렌더러 바이너리, 프로브, 상태, 모델 중
  바뀐 것만 다시 렌더링합니다.
- 리포트를 확인하세요. `analysis_version`이 2이고, `source.tool_commit`이 빌드한 커밋이고,
  `source.tool_tree_dirty`가 `false`여야 합니다.

## 3. band 지표로 매크로 방향 audit

band 지표는 `analysis_version` 2 이상으로 만든 리포트에만 있습니다. 현재 DSP만 판정하려면 같은
리포트를 baseline, incoming, candidate 세 자리에 모두 넣습니다. 그러면 audit는 "각 매크로를
0에서 1로 올리면 대상 캡처에 가까워지는가" 하나만 묻습니다.

```sh
R=~/private/m-main/comparison.json

# 고정 v1 상태: CRUSH, IMPACT, ROT, BLOOM
python Tools/nastrond_nam_tool.py audit --metric band_spectrum_rms_db \
  --baseline $R --incoming $R --candidate $R \
  --out ~/private/m-main/audit-band.json

# 채널마다 해당 채널의 매크로 양 끝값
for c in surtr ragnarok nidhoggr fimbulvetr; do
  python Tools/nastrond_nam_tool.py audit --metric band_spectrum_rms_db --scope channel_$c \
    --baseline $R --incoming $R --candidate $R \
    --out ~/private/m-main/audit-band-$c.json
done
```

| scope | 매크로 | 대상 캡처 |
|---|---|---|
| `channel_surtr` | CRUSH | VH4 569473, 569474, 569477 |
| `channel_ragnarok` | IMPACT | Twin Jet 652436, 652437 |
| `channel_nidhoggr` | ROT | Granophyre 403977, 421075, 421074 |
| `channel_fimbulvetr` | BLOOM | Matamp 614727, 561845, 543854 |

`channel_fenrir`는 `No macro endpoint states in scope`로 멈추는데, 정상입니다. 이 채널의 매크로인
CLANK는 대상인 Meshuggah 캡처에 캐비닛이 포함돼 있어 앰프만으로 비교할 대상이 없습니다.
매니페스트에서 뽑은 대상 그룹은 `python Tools/nastrond_nam_tool.py groups`로 볼 수 있습니다.

어떤 매크로가 대상 캡처를 pluck 또는 chord에서 0.1 dB 미만으로만 개선하면 audit는 종료 코드 1로
끝나고 그 항목을 `failures`에 남깁니다. 도구 오류가 아니라 측정 결과입니다. JSON은 어느 경우든
기록됩니다.

## 4. v1 지표와 비교

같은 명령을 `--metric` 없이 실행해 `audit-v1*.json`으로 저장합니다(기본값은 `log_spectrum_rms_db`).
매크로마다 다음 값을 비교합니다.

```
groups.<macro>.measurements.candidate.<pluck|chord>.mean_scores_db
groups.<macro>.measurements.candidate.<pluck|chord>.per_reference[].endpoint_improvement_db
```

핵심은 IMPACT와 BLOOM의 결론이 바뀌는지입니다. 두 매크로의 효과는 주로 500 Hz 아래에 있는데,
v1 지표는 이 대역을 작게 반영합니다. 두 지표를 모두 기록하세요. 고정된 기존 증거에서 어느 한쪽이
다른 쪽을 대체하지 않습니다.

## 5. 선택: 소스 버전끼리 비교

예전 소스와 현재 소스를 비교하려면 커밋마다 `ChimeraOriginalAmpRender`를 빌드하고, 빌드마다 다른
`--out` 폴더로 `compare`를 한 번씩 실행합니다. 그 리포트들을 baseline, incoming, candidate 자리에
넣습니다. audit는 조건이 섞인 입력을 거부합니다.

- 세 리포트 모두 같은 NAM 렌더러 바이너리, 매니페스트, 프로브로 만들어야 합니다. 모든 실행에
  같은 `~/nam-core/build/tools/render`를 쓰세요.
- audit는 incoming과 candidate 사이에 기본 상태 음량이 0.001 dB 넘게 달라지지 않았는지 확인합니다.
  일부러 보이싱을 바꾼 경우라면 `--max-default-drift-db`로 허용치를 올리고, 그 사실을 증거에 적으세요.
- 5채널 개편 이전의 렌더러는 `channel` 키를 처리하지 못합니다. 채널 scope 비교는 채널이 있는
  빌드끼리만 하세요.

## 6. 증거 남기기

새 `docs/evidence/nastrond-band-<날짜>/` 폴더에 다음을 복사합니다.

- `comparison.json`
- `audit-band*.json`과 `audit-v1*.json` 파일
- `gain-summary.csv`. 다음 명령으로 만듭니다:
  `python Tools/nastrond_nam_tool.py gain-csv --comparison $R --out gain-summary.csv`
- 렌더러 테스트의 `ctest` 로그

결과는 다른 `NASTROND_*.md` 문서 옆에 짧은 보고서로 정리합니다. 모든 수치에 어느 지표인지 밝히고,
기존 차단 사항도 그대로 적으세요. Meshuggah 캡처에 캐비닛이 포함돼 있고, Granophyre 출처가 확인되지
않았고, 입력 레벨 절대 보정이 없고, 오너 DI 청음은 아직 하지 않았습니다.
