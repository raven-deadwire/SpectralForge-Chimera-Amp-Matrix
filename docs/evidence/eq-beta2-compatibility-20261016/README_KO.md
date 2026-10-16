# 공개 beta.2 → EQ 후보 호환성 검증 결과

**판정: 상태 파일·숫자 VST3 lane 계약 PASS, 내부 raw float 보존 검사 FAIL, 실제 DAW 수용 NOT_ASSESSED. Draft 유지 / release_approved=false.**

PR #59 기준 `7ef99333ce14315538ec4aed350d802ca0d78e23`. 후속 Draft: [PR #83](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/83). 제품 Source tree `d6de02d72f92d550ce131e8ac07109eb9ae99467`는 그대로다. 제품 C++·DSP·파라미터·프리셋·스키마·버전·배포 계약을 수정하지 않았다.

## 실제 실행 결과

| 검사 | 실제 결과와 범위 |
| --- | --- |
| 공개 beta.2 출처 | Linux deb 및 VST3의 공개 SHA-256 일치. 실제 배포 모듈을 ABI로 로드해 default / pattern / host-seeded A/B 상태 3종 생성 PASS |
| 최초 불러오기 | 세 fixture의 루트 4,967개 raw 값 모두 원본 저장값과 bit-identical. 점유 메타데이터 없음 → Tone/Final 각각 12개 슬롯 보존 |
| 저장 파일 및 역방향 | 삭제·재사용·자동화 완료의 3개 파일을 **후보 → beta.2 → 후보**로 로드/재저장. 총 6회 전체 XML 상태 의미 비교 PASS. 파라미터·삭제 점유·전역 A/B·USER_IRS/MIDI_MAP 보존. IR payload는 없는 fixture |
| 실제 VST3 자동화 | 공개 beta.2의 숫자 ParamID 4개로 후보 Tone 4/Final 8의 frequency/gain을 재생. 정확히 해당 4필드만 변경, 다른 값·비활성 B·점유 상태 보존 PASS |
| VST3 파라미터 계약 | 7,048개 wrapper 항목(제품 4,967개 포함)의 ID·순서·default·steps·flags 동일. EQ 122개 이름도 동일. **비EQ 표시명 1,078개는 다름**: 구버전 공개 브랜딩과 PR #59 개발 기반의 차이. 숫자 ID 불일치가 아니며 별도 목록 보존 |
| 새 CTest 로컬 | **2 PASS / 1 FAIL**, 5.83초. 무결성과 evidence unit test는 PASS. `ChimeraEQLegacyStateTests`는 아래 raw 값 차이를 탐지해 실패하며, 비교 실패 후에도 나머지 경로 증거를 수집한 뒤 nonzero 종료 |
| fixture별 엄격 raw 검사 | default PASS. pattern FAIL(Q 2회). host-seeded A/B FAIL(69회). 전체 **71회 관측 / 35개 고유 파라미터 / 최대 1 ULP** |
| 기존 Editing / Freeze | 최초 [3-OS CI 38865366800](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38865366800)에서 Windows·macOS·Linux 모두 각각 PASS. 기존 두 테스트의 소스는 후속에서도 그대로 |
| 실제 DAW | Studio One/Cakewalk/Cubase 네이티브 프로젝트, Read/Touch/Latch/Write, 호스트 Undo, 파일 대화상자 export/import, 설치본·실물 인터페이스·청음은 **NOT_ASSESSED** |

## 수용을 보류한 raw float 차이

파일의 저장값·호스트 정규화 값 보존과, 새 processor에서 DSP가 읽는 raw float의 bit 보존을 분리했다. 새 테스트는 raw 값이 원본 또는 기존 파라미터의 정확한 정규화 왕복값과 일치하는지 확인한다. 직접 편집 중 변경 허용 대상 외 필드는 계속 bit-identical이어야 한다. 허용 오차만으로 다른 raw 값을 통과시키지 않는다.

재사용 후 새 processor로 열면 예를 들어 Q가 다음처럼 달라진다.

- 저장 전/그 값의 재정규화: `0.7070000768`
- 새 processor의 raw 값: `0.7070000172`
- 두 값의 호스트 정규화: 동일한 `0.4563710988`

host-seeded A/B 전환 뒤 재열기에서는 Q·일부 frequency와 `delaymix`에도 1 ULP 차이가 관측됐다. 총 35개 고유 파라미터의 모든 관측에서 정규화 값은 동일했다. **이를 슬롯 이동·automation ID 변경·큰 음색 손실로 판단하지 않는다.** 또한 이 결과만으로 PR #59가 새로 만든 회귀라고 단정하지 않는다. 기존 JUCE/APVTS의 raw 표현과 정규화 값 관계인지, 엄격한 DSP 복원 수용을 위해 별도 수정이 필요한지는 후속 분석 대상이다. 현재 엄격 raw gate는 FAIL로 유지한다.

최초 beta.2 상태를 dirty parameter queue 후 다시 불러오는 별도 ABI 대조에서는 beta.2와 후보의 **직렬화된 4,967개 값 차이가 0**이었다. 이것은 내부 raw DSP 값 또는 실제 음향의 동일성을 증명하는 검사가 아니다.

[전체 raw 관측 JSON](raw-value-differences.json) · [실제 CTest 로그](local-ctest.log) · [JUnit](local-ctest.xml)

## 검사기 수정과 최초 실패를 보존

1. 최초 로컬/3-OS 검사기는 원본 그대로 보존한 `busattack`까지 불필요한 재정규화 값과만 비교해 오탐했다. 실제 ABI 재저장의 전체 파라미터 차이 0을 확인하고, **원본 bit 보존 또는 정확한 기존 정규화 왕복**을 인정하도록 검사기를 수정했다.
2. dirty 복원 및 A/B 전환 뒤 direct edit를 이전 raw baseline과 비교하던 오류도 수정했다. 복원 계약을 먼저 검사하고, 그 직후 상태를 직접 편집의 baseline으로 사용한다. 따라서 복원 시 정규화와 편집으로 인한 비의도적 변경을 혼동하지 않는다.
3. 위 수정 뒤에도 남은 71회 raw 차이는 지우지 않았다. 실패 수를 누적하고 process exit code 1 / CTest FAIL로 남긴다.
4. 최초 CI는 3개 OS 모두 **4 PASS / 1 FAIL**이며 전체 workflow failure다. Editing/Freeze·fixture integrity·5개 Python controls는 성공했고, 새 상태 검사는 `busattack` 오탐으로 조기 종료했다. 이는 수정된 검사기의 3-OS 성공 증거가 아니다. 후속 커밋 CI는 별도 실행한다.
5. 로컬 최초 링크 실패는 생성된 artwork 객체 3개가 0-byte였기 때문이다. 해당 중간 객체만 재생성해 정상 링크했다. 제품 자산·소스·빌드 플래그를 수정하지 않았다.

## 증거와 재현

- [검사 설계·fixture 구분·DAW 세션 실행 조건](../../EQ_LEGACY_COMPATIBILITY_20261016.md)
- [공개 beta.2 원본 component bytes·host-seeded 입력·로그·manifest](public-beta2-capture.zip)
- [단계별 `.chimera`, 후보/구버전 component bytes, ParamID 목록과 표시명 차이](roundtrip-states.zip)
- [바이너리·테스트 소스·로그 SHA-256](execution-summary.json)
- [숫자 lane 재생 로그](abi-replay.log) · [양방향 재저장 로그](reverse-roundtrip.log)
- 최초 CI 발췌: [Linux](initial-ci-Linux.log) / [Windows](initial-ci-Windows.log) / [macOS](initial-ci-macOS.log)

로컬 Ubuntu 24.04 x86_64 / GCC 13.3 / JUCE 8.0.8. 제품 바이너리는 `b07e49b425` revision으로 빌드했으며, 후속 검사기 소스와 바이너리의 실제 SHA-256을 별도로 기록했다. 테스트 수정으로 제품 Source tree가 달라졌다고 주장하지 않는다.

```sh
python Tools/eq_legacy_fixture.py verify --output Tests/fixtures/eq-beta2
cmake -S . -B build-legacy -DCMAKE_BUILD_TYPE=Release -DCHIMERA_EQ_DEVELOPMENT_PREVIEW=ON
cmake --build build-legacy --target ChimeraEQLegacyStateTests ChimeraGraphicalEQEditingTests ChimeraEQSnapshotFreezeTests --parallel 2
ctest --test-dir build-legacy -C Release -R '^(ChimeraEQLegacy.*|ChimeraGraphicalEQEditingTests|ChimeraEQSnapshotFreezeTests)$' --output-on-failure --no-tests=error
```

실제 VST3 재수집·숫자 lane·역방향 검사는 `.github/workflows/eq-legacy-state.yml`의 고정 digest/명령으로 재현한다. raw gate 실패가 이후 독립 ABI 증거 수집을 막지 않도록 구성하되, 실패한 step과 최종 workflow의 실패 판정은 유지한다. 모든 실제 DAW 수용 및 raw 차이 검토가 끝나기 전에는 merge/tag/공개 배포를 승인하지 않는다.
