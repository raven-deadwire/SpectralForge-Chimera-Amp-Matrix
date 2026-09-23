# Chimera Amp Matrix 1.0 test plan
Release gate: Ubuntu + macOS compile, CTest pass, VST3/Standalone artifacts produced; AU on macOS.
DSP smoke matrix: 44.1/48/96/192 kHz; Classic/Dual/Matrix; finite output.
Manual audio QA: guitar and bass DI, all eight amp archetypes, 3-band Matrix, Dual GG/GB/BB, cab bypass/cuts, state recall.
Known 1.0 test-build scope: component-inspired behavioral amp models, not component-exact emulations; user IR API exists in DSP and UI file browser is a post-smoke-test task.
