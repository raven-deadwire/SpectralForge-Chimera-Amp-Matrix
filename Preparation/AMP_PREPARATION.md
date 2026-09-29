# v3 — 앰프별 조절부와 별도 상태 뷰 (2026-09-28)

`amp_controls.py`가 기존 15개와 추가 대상 8개의 앰프 조절부를 개별 정의합니다. 채널별 노브와 실제 공유 EQ를 구분하며, `amp_controls.py`의 각 정의에 자료 상태와 누락 기능을 기록합니다. 상세 대조표는 전달용 ZIP에 별도로 포함합니다. **전체 23개가 검증된 실물 앰프/DSP라는 뜻이 아닙니다.**

시제품 상단 **AMP · 모델별 조절부**를 선택하세요. 헤드와 채널을 변경하고, Classic/Dual/Matrix의 레인별 설정을 편집할 수 있습니다. 설정은 별도 `.calab` 파일로 저장/복원합니다. 기존 페달 `.cbp`는 변경하지 않습니다. 앱의 두 탭은 독립적인 준비용 상태 뷰이며 실물 플러그인 UI나 오디오 라우팅은 아닙니다. 페달은 여전히 최대 다섯 개입니다.

- `ui/amp_state.js`: 검증·모델별/채널별/레인별 설정 은행·Undo/Redo·직렬화. 실제 MIDI/호스트 파라미터 없음.
- `ui/amp_editor.js`, `ui/amp_panel.html`: 제한 높이의 전용 패널, 공유/채널 컨트롤 표시, 별도 Input Trim/Level, 원문·검증 대기 표시.
- `tests/test_amp_catalog.py`, `tests/amp_state_test.js`, `tests/amp_browser_test.py`: 앰프 정의·원자적 상태 복원·브라우저 검사.

`python run_checks.py --browser /usr/bin/chromium`는 기존 준비 테스트와 앰프 테스트를 함께 실행합니다. Chromium 경로는 설치 환경에 맞춰 지정하세요. 연속 값은 노브 위치이며 소리가 바뀌지 않습니다. 현재 제품의 Gain/EQ를 바꾸거나 이 파일을 APVTS에 바로 연결하지 말고 리비전·동작·기존 사운드 호환성을 검증한 후 통합해야 합니다.

상세 모델별 제어 정의와 출처는 `amp_controls.py`, 검증 결과는 `VERIFICATION.md`를 참고하세요. 핵심 조절부 1차 자료 확인 11개, 일부 확인·리비전 대조 7개, 원본 패널 확인 대기 5개를 구분합니다. 생성된 `catalog.json`에도 같은 근거가 포함됩니다.
