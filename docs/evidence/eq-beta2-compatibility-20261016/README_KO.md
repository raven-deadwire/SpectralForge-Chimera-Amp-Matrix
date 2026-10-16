# 공개 beta.2 → EQ 후보 호환성 증거

기준 PR #59: `7ef99333ce14315538ec4aed350d802ca0d78e23`. 제품 Source tree는 변경하지 않는다.

현재 확정한 실행:

- 공개 Linux beta.2 deb 및 VST3 SHA-256 검증 PASS.
- 공개 VST3 실제 로드·parameter queue·state 저장: default / pattern / host-seeded A/B 세 경우 PASS.
- 4,967개 제품 값, 122개 EQ 파라미터가 포함되고 occupancy metadata가 없는 실제 파일 확보.
- fixture 및 parameter/lane 목록 무결성 검사 PASS.
- 증거 검사기의 5개 negative/unit controls PASS.

후보 C++ 실제 fixture 회귀, 기존 Editing/Freeze 재실행 및 candidate VST3 lane replay는 실행 결과가 확보된 뒤 아래에 추가한다. 현재 항목을 PASS로 간주하지 않는다. 3-OS CI는 별도 `EQ released beta.2 state compatibility` workflow로 수행한다.

실제 DAW 세션, Read/Touch/Latch/Write, DAW Undo, UI 파일 대화상자 export/import, 물리 인터페이스·청음: **NOT_ASSESSED**. `release_approved=false`.

[검사 설계·fixture 출처·실제 DAW 실행 조건](../../EQ_LEGACY_COMPATIBILITY_20261016.md)
