# Chimera release gate: BLOCKED

- Profile: `beta_1_3`
- Commit: `babfecda995141ef3001fc6f9c04c05409ff7ad6`
- Checks: 2 PASS / 126 BLOCKED / 0 NOT_APPLICABLE
- Hard gates: 2 / 63 PASS

| Stage | Status | PASS | BLOCKED | N/A |
|---|---|---:|---:|---:|
| A1 Installer consistency | BLOCKED | 0 | 6 | 0 |
| A2 Signature definition | BLOCKED | 0 | 8 | 0 |
| A3 Signature APVTS recall | BLOCKED | 0 | 9 | 0 |
| A4 Factory compatibility | BLOCKED | 0 | 5 | 0 |
| A5 Hartke IR import | BLOCKED | 0 | 8 | 0 |
| A6 Marshall IR import | BLOCKED | 0 | 8 | 0 |
| A7 IR chain of trust | BLOCKED | 0 | 7 | 0 |
| A8 IR fallback policy | BLOCKED | 0 | 4 | 0 |
| A9 Signature benchmark calculator | BLOCKED | 0 | 9 | 0 |
| A10 Signature hard and secondary gates | BLOCKED | 0 | 6 | 0 |
| A11 Benchmark JSON | BLOCKED | 0 | 9 | 0 |
| A12 Automated regression tests | BLOCKED | 0 | 7 | 0 |
| A13 CI final state | BLOCKED | 2 | 5 | 0 |
| A14 Documentation sync | BLOCKED | 0 | 7 | 0 |
| B1 Same-DI fixture | BLOCKED | 0 | 1 | 0 |
| B2 Signature renders | BLOCKED | 0 | 1 | 0 |
| B3 Audio measurements | BLOCKED | 0 | 1 | 0 |
| B4 Signature hard gates | BLOCKED | 0 | 4 | 0 |
| B5 Signature secondary gates | BLOCKED | 0 | 1 | 0 |
| B6 Signature score verdict | BLOCKED | 0 | 1 | 0 |
| B8 Listening A/B | BLOCKED | 0 | 1 | 0 |
| B10 Audio save/restore roundtrip | BLOCKED | 0 | 1 | 0 |
| B11 Windows host validation | BLOCKED | 0 | 1 | 0 |
| B12 Studio One 8 validation | BLOCKED | 0 | 1 | 0 |
| B13 30 minute soak | BLOCKED | 0 | 1 | 0 |
| B14 Release candidate gate | BLOCKED | 0 | 1 | 0 |
| E1 E670FE implementation and legacy compatibility | BLOCKED | 0 | 3 | 0 |
| E2 E670FE reference and sound acceptance | BLOCKED | 0 | 3 | 0 |
| I1 Combined 1.1.1 synthetic integration | BLOCKED | 0 | 4 | 0 |
| I2 1.1.1 real-input and host acceptance | BLOCKED | 0 | 3 | 0 |

## Blocking path
- `A1.04` — NOT_EXECUTED: Required validation was not executed
- `A1.05` — NOT_EXECUTED: Required validation was not executed
- `A1.06` — NOT_EXECUTED: Required validation was not executed
- `A3.01` — NOT_EXECUTED: Required validation was not executed
- `A3.06` — NOT_EXECUTED: Required validation was not executed
- `A3.07` — NOT_EXECUTED: Required validation was not executed
- `A3.08` — NOT_EXECUTED: Required validation was not executed
- `A4.01` — NOT_EXECUTED: Required validation was not executed
- `A4.03` — NOT_EXECUTED: Required validation was not executed
- `A4.04` — NOT_EXECUTED: Required validation was not executed
- `A4.05` — NOT_EXECUTED: Required validation was not executed
- `A5.01` — NOT_EXECUTED: Required validation was not executed
- `A5.05` — NOT_EXECUTED: Required validation was not executed
- `A5.08` — NOT_EXECUTED: Required validation was not executed
- `A6.01` — NOT_EXECUTED: Required validation was not executed
- `A6.05` — NOT_EXECUTED: Required validation was not executed
- `A6.08` — NOT_EXECUTED: Required validation was not executed
- `A7.01` — NOT_EXECUTED: Required validation was not executed
- `A7.02` — NOT_EXECUTED: Required validation was not executed
- `A7.03` — NOT_EXECUTED: Required validation was not executed
- `A7.04` — NOT_EXECUTED: Required validation was not executed
- `A7.05` — NOT_EXECUTED: Required validation was not executed
- `A7.06` — NOT_EXECUTED: Required validation was not executed
- `A7.07` — NOT_EXECUTED: Required validation was not executed
- `A8.01` — NOT_EXECUTED: Required validation was not executed
- `A8.02` — NOT_EXECUTED: Required validation was not executed
- `A8.03` — NOT_EXECUTED: Required validation was not executed
- `A12.01` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.02` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.03` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.04` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.05` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.06` — TEST_FAILURE: Missing/failed evidence from: windows
- `A12.07` — TEST_FAILURE: Missing/failed evidence from: windows
- `A13.01` — TEST_FAILURE: Missing/failed evidence from: windows
- `A13.04` — TEST_FAILURE: Missing/failed evidence from: windows
- `A13.05` — TEST_FAILURE: Missing/failed evidence from: windows
- `A13.06` — TEST_FAILURE: Missing/failed evidence from: windows
- `A13.07` — TEST_FAILURE: Evidence infrastructure error: assembly: Cannot read JSON release-inputs/Chimera-release-evidence-assembly/evidence.json: [Errno 2] No such file or directory: 'release-inputs/Chimera-release-evidence-assembly/evidence.json'
- `B4.H1` — NOT_EXECUTED: Required validation was not executed
- `B4.H2` — NOT_EXECUTED: Required validation was not executed
- `B4.H3` — NOT_EXECUTED: Required validation was not executed
- `B4.H4` — NOT_EXECUTED: Required validation was not executed
- `B6.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B10.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B11.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B13.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B14.RESULT` — NOT_EXECUTED: Required validation was not executed
- `E1.DSP` — TEST_FAILURE: Missing/failed evidence from: windows
- `E1.STATE_UI` — TEST_FAILURE: Missing/failed evidence from: windows
- `E1.LEGACY` — TEST_FAILURE: Missing/failed evidence from: windows
- `E2.TD_EQ` — NOT_EXECUTED: Required validation was not executed
- `E2.CALIBRATION` — NOT_EXECUTED: Required validation was not executed
- `E2.SAME_DI` — NOT_EXECUTED: Required validation was not executed
- `I1.PITCH` — TEST_FAILURE: Missing/failed evidence from: windows
- `I1.GATE` — TEST_FAILURE: Missing/failed evidence from: windows
- `I1.STATE_GAIN_GR` — TEST_FAILURE: Missing/failed evidence from: windows
- `I1.LIVE_REMOVE` — TEST_FAILURE: Missing/failed evidence from: windows
- `I2.GATE_DI` — NOT_EXECUTED: Required validation was not executed
- `I2.UI_AUDIO_TIMING` — NOT_EXECUTED: Required validation was not executed
- `I2.LIVE_REMOVE_DAW` — NOT_EXECUTED: Required validation was not executed
- `A1.01` — NOT_EXECUTED: Required validation was not executed
- `A1.02` — NOT_EXECUTED: Required validation was not executed
- `A1.03` — NOT_EXECUTED: Required validation was not executed
- `A2.01` — NOT_EXECUTED: Required validation was not executed
- `A2.02` — NOT_EXECUTED: Required validation was not executed
- `A2.03` — NOT_EXECUTED: Required validation was not executed
- `A2.04` — NOT_EXECUTED: Required validation was not executed
- `A2.05` — NOT_EXECUTED: Required validation was not executed
- `A2.06` — NOT_EXECUTED: Required validation was not executed
- `A2.07` — NOT_EXECUTED: Required validation was not executed
- `A2.08` — NOT_EXECUTED: Required validation was not executed
- `A3.02` — NOT_EXECUTED: Required validation was not executed
- `A3.03` — NOT_EXECUTED: Required validation was not executed
- `A3.04` — NOT_EXECUTED: Required validation was not executed
- `A3.05` — NOT_EXECUTED: Required validation was not executed
- `A3.09` — NOT_EXECUTED: Required validation was not executed
- `A4.02` — NOT_EXECUTED: Required validation was not executed
- `A5.02` — NOT_EXECUTED: Required validation was not executed
- `A5.03` — NOT_EXECUTED: Required validation was not executed
- `A5.04` — NOT_EXECUTED: Required validation was not executed
- `A5.06` — NOT_EXECUTED: Required validation was not executed
- `A5.07` — NOT_EXECUTED: Required validation was not executed
- `A6.02` — NOT_EXECUTED: Required validation was not executed
- `A6.03` — NOT_EXECUTED: Required validation was not executed
- `A6.04` — NOT_EXECUTED: Required validation was not executed
- `A6.06` — NOT_EXECUTED: Required validation was not executed
- `A6.07` — NOT_EXECUTED: Required validation was not executed
- `A8.04` — NOT_EXECUTED: Required validation was not executed
- `A9.01` — NOT_EXECUTED: Required validation was not executed
- `A9.02` — NOT_EXECUTED: Required validation was not executed
- `A9.03` — NOT_EXECUTED: Required validation was not executed
- `A9.04` — NOT_EXECUTED: Required validation was not executed
- `A9.05` — NOT_EXECUTED: Required validation was not executed
- `A9.06` — NOT_EXECUTED: Required validation was not executed
- `A9.07` — NOT_EXECUTED: Required validation was not executed
- `A9.08` — NOT_EXECUTED: Required validation was not executed
- `A9.09` — NOT_EXECUTED: Required validation was not executed
- `A10.01` — NOT_EXECUTED: Required validation was not executed
- `A10.02` — NOT_EXECUTED: Required validation was not executed
- `A10.03` — NOT_EXECUTED: Required validation was not executed
- `A10.04` — NOT_EXECUTED: Required validation was not executed
- `A10.05` — NOT_EXECUTED: Required validation was not executed
- `A10.06` — NOT_EXECUTED: Required validation was not executed
- `A11.01` — NOT_EXECUTED: Required validation was not executed
- `A11.02` — NOT_EXECUTED: Required validation was not executed
- `A11.03` — NOT_EXECUTED: Required validation was not executed
- `A11.04` — NOT_EXECUTED: Required validation was not executed
- `A11.05` — NOT_EXECUTED: Required validation was not executed
- `A11.06` — NOT_EXECUTED: Required validation was not executed
- `A11.07` — NOT_EXECUTED: Required validation was not executed
- `A11.08` — NOT_EXECUTED: Required validation was not executed
- `A11.09` — NOT_EXECUTED: Required validation was not executed
- `A14.01` — NOT_EXECUTED: Required validation was not executed
- `A14.02` — NOT_EXECUTED: Required validation was not executed
- `A14.03` — NOT_EXECUTED: Required validation was not executed
- `A14.04` — NOT_EXECUTED: Required validation was not executed
- `A14.05` — NOT_EXECUTED: Required validation was not executed
- `A14.06` — NOT_EXECUTED: Required validation was not executed
- `A14.07` — NOT_EXECUTED: Required validation was not executed
- `B1.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B2.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B3.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B5.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B8.RESULT` — NOT_EXECUTED: Required validation was not executed
- `B12.RESULT` — NOT_EXECUTED: Required validation was not executed
