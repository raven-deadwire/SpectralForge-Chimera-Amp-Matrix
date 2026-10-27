# FabFilter 블랙박스 측정 키트 — 2026-10-22 KST

#97 P0-2(FabFilter 리버스엔지니어링)가 BLOCKED였던 이유는 **원본 제품의 실측 자료가 하나도 없었기 때문**이다. 기존 `MULTIBAND_PRO_MB_AUDIT_20261020.md` 등은 Chimera 자체 DSP와 FabFilter **공개 매뉴얼**을 비교했다. 이 키트는 같은 테스트 신호를 원본 플러그인과 Chimera에 통과시켜 **입출력 동작**을 수치로 비교한다.

- 측정 대상은 사용자가 DAW에서 렌더한 오디오뿐이다. FabFilter 바이너리·코드·자산을 실행·분석·디컴파일·재배포하지 않는다.
- 결과는 측정된 차이이며, 동등성·청감·수용검사 판정이 아니다.
- 범위: Pro-Q 4(EQ 응답·위상·지연), Pro-C 3(정적 곡선·어택/릴리스), Pro-MB(중립 크로스오버 응답·밴드 선택성·밴드 시간 동작), Pro-G(열림/닫힘 threshold·히스테리시스·range·hold/release·익스팬더 ratio), Pro-L 2(ceiling 준수·샘플/트루 피크·오버슈트·릴리스), Pro-DS(분할 대역 중립 응답·대역 선택성·시간 동작).

## 구성

| 파일 | 역할 |
|---|---|
| `Tools/fabfilter_kit.py` | `generate` 신호·manifest·렌더 가이드 / `chimera` Chimera 렌더 / `analyze` 분석 / `compare` 기준 대비 차이 |
| `Tools/FabFilterKit/ChimeraKitProbe.cpp` | 같은 신호를 실제 `GraphicalEQ`(Tone EQ 1밴드), `CommonDynamics` 클린 컴프레서, `MultibandDynamicsCore` 4밴드, `NoiseGate`, `CommonDynamics::limit` 출력 리미터, `DeEsserCore`로 처리 (256-sample 블록) |
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

- **Pro-MB (크로스오버 120 / 700 / 3500 Hz, 4밴드):** 중립 임펄스(Dynamic Phase, Minimum Phase) — 모든 밴드 Range 0에서 크기·위상·군지연. 밴드 2(120–700 Hz)만 −20 dB 4:1 10/100 ms knee 0 range 30으로 압축할 때 300 Hz(대역 안)·2 kHz(대역 밖) 정적 곡선과 300 Hz 스텝. 대역 밖 톤의 GR은 밴드 선택성(누설) 지표다.

- **Pro-G (1 kHz 톤):** 1 s 선행 후 −70.5 → −10.5 → −70.5 dBFS를 1 dB(0.5 dB 오프셋, 0.3 s) 계단으로 올렸다 내림 — 단계 끝 감쇠로 열림 threshold(올라갈 때 처음 열린 레벨), 닫힘 threshold(내려갈 때 아직 열린 가장 낮은 레벨), 히스테리시스, range. 정적 두 케이스는 각 단계가 정착하도록 hold 0 / release 20 ms. 스텝(−60 → −20 → −60 dBFS, hold 50 / release 100 ms)으로 열림 90 %와 닫힘 10/50/90 % 시간. 익스팬더 2:1은 부분 감쇠 구간의 기울기로 ratio를 구한다(열림/닫힘 값은 게이트에만 의미가 있다).

- **Pro-L 2 (ceiling −1 dB, lookahead 0, release 100 ms):** −12 → +12 dBFS 1 kHz 계단(1 dB × 0.4 s)의 단계별 GR·출력 샘플 피크·트루 피크(4× FFT 업샘플). −20 → +6 → −20 dBFS 스텝의 첫 5 ms 피크(오버슈트)와 릴리스 τ. fs/4·45° 사인(샘플 피크 −0.5, 트루 피크 +2.5 dBFS)으로 트루 피크 제한 여부 — 원본은 True Peak Limiting ON으로 렌더한다.
- **Pro-DS (Split Band, 6 kHz, −30 dB, range 12):** −60 dBFS 임펄스(threshold 아래)로 분할 대역의 선형 응답, 8 kHz(대역 안)·1 kHz(대역 밖) 정적 곡선, 8 kHz 스텝.

도달 시간은 조건이 **1 ms 이상 유지되기 시작한 시점**이다. 필터 기반 처리기의 스텝 순간 단일 샘플 과도값을 도달로 세지 않는다.

릴리스: 원시 63/90 % 도달 시간과 함께, 스텝 60 ms 이후 GR 감쇠의 지수 적합 시정수 `release_tau_ms`를 보고한다. 필터 기반 처리기(멀티밴드)는 입력이 급감한 직후 수십 ms 동안 필터 잔향이 새 입력보다 커서 원시 도달 시간이 의미가 없다(Chimera 밴드 2: 원시 63 % 1.35 ms, 적합 τ 102 ms). 비교에는 `release_tau_ms`를 쓴다.

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
| Comp 1/50 ms | RMS | attack 63 % 5.3 ms, release 63 % 66 ms, τ 50.0 ms |
| MB 중립 (모든 밴드 무처리) | 120/700/3500 | 크기 0.00 dB, **위상 0°**, 지연 0 — 원음과 동일 |
| MB 밴드 2, 300 Hz | −20 dB 4:1 | 적합 threshold −16.75 dB, **실효 ratio 3.00** |
| MB 밴드 2, 2 kHz (대역 밖) | 〃 | GR 없음(누설 0 dB) |
| MB 밴드 2 스텝 300 Hz | 10/100 ms | attack 63 % 15.2 ms, release τ 102 ms |
| Gate −40 dB range 40 | hold 0 / rel 20 | 열림 −39.5, 닫힘 −45.5 dBFS(히스테리시스 6 dB), 감쇠 40.0 dB |
| Gate 스텝 | hold 50 / rel 100 | 열림 90 % 0.6 ms, 닫힘 10/50/90 % 156 / 348 / 627 ms |
| Expander 2:1 range 40 | hold 0 / rel 20 | ratio 2.00 |
| Limiter −1 dB 계단 | sample peak | 출력 샘플 피크 최대 −1.00, 트루 피크 최대 −0.98 dBFS, +12 dBFS 입력에서 GR 12.9 dB |
| Limiter 스텝 +6 dBFS | release 100 | 첫 5 ms 샘플 피크 −1.00(오버슈트 없음), 트루 피크 −0.96, release τ 91 ms |
| Limiter 트루 피크 | sample peak | 입력 TP +2.50 → 출력 샘플 −1.00 / **트루 피크 +2.01 dBTP** |
| De-esser 중립 −60 dBFS | 6 kHz split | 크기 ±0.00 dB(분할 합 평탄) |
| De-esser 8 kHz | −30 dB 4:1 range 12 | GR 시작 ≈ −28 dBFS, **최대 7.8 dB**(range 12 미도달) |
| De-esser 1 kHz (대역 밖) | 〃 | GR 없음 |
| De-esser 스텝 8 kHz | 2 / 80 ms | attack 63 % 1.8 ms, release τ 83 ms |

이미 드러난 Chimera 특성(원본 비교 전):

1. **고역 bell 폭 증가:** 쌍선형 RBJ biquad라 10 kHz bell의 실측 Q가 설정 1.0 대비 1.45로 좁아지고 나이퀴스트 쪽이 찌그러진다(cramping). 원본 렌더에서 고역 응답이 설정 Q를 유지하면 이것이 첫 번째 적용 후보다(예: 매칭된 아날로그 프로토타입 계수 또는 오버샘플링).
2. **threshold 단위:** RMS 검출기라 사인 피크 기준 threshold가 3 dB 높게 측정된다. 원본이 피크 기준인지 RMS 기준인지는 원본 렌더로 판단한다.
3. **어택 시간:** 설정 attack은 GR 1-pole 시정수인데 10 ms RMS 평활이 더해져 실측 63 % 시간이 16 ms다. 원본의 attack 표기 의미를 렌더로 확인한 뒤 매핑을 정한다.
4. **멀티밴드 실효 ratio:** 밴드 2에 4:1을 걸어도 300 Hz 톤의 전체 출력 ratio는 3.0이다. 톤 일부가 이웃 밴드의 크로스오버 경계 영역에 걸려 압축되지 않는다. 원본의 밴드 경계(slope)와 비교해 같은 현상인지 본다.
5. **멀티밴드 중립 = 원음:** 무처리일 때 위상 회전 없이 원음과 동일하다(#111 최소위상 합성). Pro-MB Dynamic Phase의 "무처리 시 평탄한 위상"과 같은 관측이 나오는지 비교 대상이다.
6. **게이트 닫힘 시간:** 20 ms 피크 검출기 감쇠가 hold 앞에 더해져, hold 50 / release 100 ms 설정에서 10 % 닫힘이 156 ms다.
7. **리미터 트루 피크:** 샘플 피크만 제한하므로 인터샘플 피크가 ceiling을 3 dB 넘을 수 있다(+2.0 dBTP). 원본이 트루 피크를 −1 dBTP로 지키면 오버샘플링 검출이 적용 후보다. 로드맵은 측정된 Studio true-peak 옵션을 1.5로 두고 있다.
8. **디에서 최대 감쇠:** 6 kHz 분할 위의 8 kHz 톤도 일부 에너지가 LR4 저역 가지에 남아, range 12 dB를 걸어도 전체 출력 감쇠는 7.8 dB에 그친다. 원본의 Split Band 동작과 비교한다.

## 남은 일 / 경계

- 원본 렌더(Pro-Q 4, Pro-C 3) 미확보 — 이 키트로 수집해야 비교·적용 변경 도출이 가능하다.
- Pro-Q 4 Natural/Linear Phase, dynamic EQ, Pro-C 3의 다른 Style·lookahead·auto-release는 아직 케이스가 없다.
- 측정은 선형 EQ와 1 kHz 사인 기반 컴프레서 동작에 한정된다. 음악 신호·프로그램 의존 동작·청감 비교는 별도다.
