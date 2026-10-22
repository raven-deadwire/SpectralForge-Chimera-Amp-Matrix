# FabFilter 블랙박스 측정 키트 — 2026-10-22 KST

#97 P0-2(FabFilter 리버스엔지니어링)가 BLOCKED였던 이유는 **원본 제품의 실측 자료가 하나도 없었기 때문**이다. 기존 `MULTIBAND_PRO_MB_AUDIT_20261020.md` 등은 Chimera 자체 DSP와 FabFilter **공개 매뉴얼**을 비교했다. 이 키트는 같은 테스트 신호를 원본 플러그인과 Chimera에 통과시켜 **입출력 동작**을 수치로 비교한다.

- 측정 대상은 사용자가 DAW에서 렌더한 오디오뿐이다. FabFilter 바이너리·코드·자산을 실행·분석·디컴파일·재배포하지 않는다.
- 결과는 측정된 차이이며, 동등성·청감·수용검사 판정이 아니다.
- 1차 범위: Pro-Q 4(EQ 응답·위상·지연)와 Pro-C 3(정적 곡선·어택/릴리스). Pro-MB·Pro-G·Pro-L 2·Pro-DS는 같은 구조로 케이스를 추가한다.

## 구성

| 파일 | 역할 |
|---|---|
| `Tools/fabfilter_kit.py` | `generate` 신호·manifest·렌더 가이드 / `chimera` Chimera 렌더 / `analyze` 분석 / `compare` 기준 대비 차이 |
| `Tools/FabFilterKit/ChimeraKitProbe.cpp` | 같은 신호를 실제 `GraphicalEQ`(Tone EQ 1밴드)와 `CommonDynamics` 클린 컴프레서로 처리 (256-sample 블록) |
| `Tools/test_fabfilter_kit.py` | FabFilter 없이 분석기 검증: 알려진 RBJ bell·64-sample 지연·피크 컴프레서를 되찾는지, PCM24/float WAV, 누락·샘플레이트 불일치 처리 |
| `.github/workflows/fabfilter-kit.yml` | 프로브 빌드, 자체 테스트, Chimera 기준 측정 산출물 |

## 사용 순서

```sh
python Tools/fabfilter_kit.py generate --out kit --rate 48000
# kit/RENDER_GUIDE.md 의 설정대로 DAW에서 렌더 → renders-fabfilter/<case id>.wav (32-bit float, 48 kHz)
cmake --build build --target ChimeraKitProbe
python Tools/fabfilter_kit.py chimera --kit kit --probe build/ChimeraKitProbe_artefacts/Release/ChimeraKitProbe --out renders-chimera
python Tools/fabfilter_kit.py compare --kit kit --reference renders-fabfilter --candidate renders-chimera --out compare.json --plots plots
```

렌더 규칙: 플러그인 1개만 삽입, dither·정규화·마스터 처리·페이드 없음, PDC 켜기, 파일 전체 길이, `renders-fabfilter/INFO.txt`에 플러그인 버전·OS·DAW 기록. 파일이 없는 케이스는 `missing`으로 보고하며 값을 만들어 넣지 않는다.

## 케이스와 측정값

- **EQ (임펄스 2^17, 8192 위치, 0.25):** H(f)=Y/X, 1/24옥타브 격자 크기, IR 피크 위치(지연), 지연 보정 위상, 1 kHz 군지연. 형태 지표: 최대/최소 이득과 주파수, half-gain(dB) 대역폭 Q, −3 dB 주파수, 셸프 중간점.
- **컴프레서 정적 (1 kHz 사인, 피크 −40…0 dBFS, 2 dB × 0.6 s):** 단계별 정상상태 GR → threshold/ratio/knee 격자 적합(RMS 오차 포함).
- **컴프레서 스텝 (−40 → −4 → −40 dBFS):** 해석신호 envelope 비율로 GR(t), 어택 10/63/90 %, 릴리스 63/90 % 시간.

레벨은 **사인 피크 dBFS** 기준이다. 검출기 방식(RMS/peak)에 따른 차이가 그대로 드러나도록 변환하지 않는다.

## Chimera 기준 측정 (main `23e710e`, 48 kHz, FabFilter 렌더 없음)

| 케이스 | 설정 | Chimera 측정 |
|---|---|---|
| Bell 1 kHz +6 dB Q1 | type 0 | +6.00 dB @ 987 Hz, half-gain Q 0.99, 지연 0 |
| Bell 10 kHz +6 dB Q1 | type 0 | +6.00 dB @ 9.95 kHz, **half-gain Q 1.45**, 20 kHz에서 +0.30 dB |
| Bell 100 Hz −6 dB Q4 | type 0 | −5.97 dB, Q 4.00 |
| Low shelf 100 Hz +6 | type 1 | 20 Hz +5.99 dB, 중간점 100 Hz |
| High shelf 8 kHz +6 | type 2 | 20 kHz +6.00 dB, 중간점 8.0 kHz |
| Low cut 12 dB/oct 100 Hz | type 3 | −3 dB 100 Hz, 20 Hz −28.0 dB |
| High cut 12 dB/oct 10 kHz | type 4 | −3 dB 9.99 kHz, 20 kHz −27.0 dB |
| Notch 1 kHz Q4 | type 5 | −3 dB 882–1134 Hz (Q≈3.97) |
| Comp −20 dB 4:1 knee 0 | RMS | 적합 threshold **−17 dB(사인 피크)**, ratio 4.00, knee 0 |
| Comp −20 dB 4:1 knee 12 | RMS | −17 dB, 4.00, knee 12 |
| Comp 10/100 ms | RMS | GR 9.75 dB, attack 63 % **16.0 ms**, release 63 % 115 ms |
| Comp 1/50 ms | RMS | attack 63 % 5.3 ms, release 63 % 66 ms |

이미 드러난 Chimera 특성(원본 비교 전):

1. **고역 bell 폭 증가:** 쌍선형 RBJ biquad라 10 kHz bell의 실측 Q가 설정 1.0 대비 1.45로 좁아지고 나이퀴스트 쪽이 찌그러진다(cramping). 원본 렌더에서 고역 응답이 설정 Q를 유지하면 이것이 첫 번째 적용 후보다(예: 매칭된 아날로그 프로토타입 계수 또는 오버샘플링).
2. **threshold 단위:** RMS 검출기라 사인 피크 기준 threshold가 3 dB 높게 측정된다. 원본이 피크 기준인지 RMS 기준인지는 원본 렌더로 판단한다.
3. **어택 시간:** 설정 attack은 GR 1-pole 시정수인데 10 ms RMS 평활이 더해져 실측 63 % 시간이 16 ms다. 원본의 attack 표기 의미를 렌더로 확인한 뒤 매핑을 정한다.

## 남은 일 / 경계

- 원본 렌더(Pro-Q 4, Pro-C 3) 미확보 — 이 키트로 수집해야 비교·적용 변경 도출이 가능하다.
- Pro-Q 4 Natural/Linear Phase, dynamic EQ, Pro-C 3의 다른 Style·lookahead·auto-release는 아직 케이스가 없다.
- 측정은 선형 EQ와 1 kHz 사인 기반 컴프레서 동작에 한정된다. 음악 신호·프로그램 의존 동작·청감 비교는 별도다.
