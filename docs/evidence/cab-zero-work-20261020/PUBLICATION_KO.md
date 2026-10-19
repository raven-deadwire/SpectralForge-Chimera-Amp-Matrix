# 측정 소스와 공개 Git 객체 연결

CLI push는 이 환경에 GitHub 자격 증명이 없어 실패했다. 연결된 GitHub API로
동일한 전체 tree를 게시했고, 로컬과 서버의 tree SHA를 비교하여 일치함을 확인했다.
작성자·시각·부모 metadata 때문에 commit SHA만 다르다. 다른 소스의 시험 결과를
승계한 것이 아니다.

| 구분 | 로컬 commit | 같은 전체 tree의 GitHub commit | tree SHA |
|---|---|---|---|
| 실제 측정 controller | `c5362688d90649d245753d644693b698b42670a4` | `17430853ebf5455662f2cfd78fd2c25b59ac557d` | `ca176fb3e60ef6b9348873727dd1f9e03ffa3688` |
| 결과·로그·감사 보존 | `bcb9f85c04fe88b5fa20591b547854fdb09932ad` | `71e93f40d137f5b69a92cc9ca3dc55b12e0b45f9` | `1d4ae14ab60adbf228cfaa22573753a8adac84d7` |

[측정 소스](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/17430853ebf5455662f2cfd78fd2c25b59ac557d/Tools/CabZeroWork) ·
[결과 소스](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/71e93f40d137f5b69a92cc9ca3dc55b12e0b45f9/docs/evidence/cab-zero-work-20261020)

이 후속 문서는 게시 연결만 추가한다. DSP·측정 바이너리·시험 임계값은 그대로이며,
새 macOS/Windows native CI 성공을 주장하지 않는다. 원시 증거 ZIP의 SHA256과
1,803개 내부 파일 manifest는 `archive.json`에 연결돼 있다. 최종 보고 commit 전체를
측정 대상으로 오해하지 않도록 실제 바이너리 hash는 `build.json`을 기준으로 한다.
