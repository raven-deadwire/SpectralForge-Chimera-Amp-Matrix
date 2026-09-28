# Preparation verification — 2026-09-28

Executed locally, not a production release test.

| Test group | Result |
|---|---|
| C++20 standalone state cases | 20 passed |
| Python audit/catalog cases | 32 passed (22 asset, 10 catalog) |
| JavaScript state cases | 22 passed, including 1,000 deterministic state operations and native-file import |
| Chromium UI cases | 14 passed |
| Return import: JS state → C++ | Passed |
| Native AddressSanitizer + UndefinedBehaviorSanitizer | Same 20 native cases passed; no sanitizer findings reported |
| CTest | 1 registered preparation executable passed |

Environment: Linux x86-64, glibc 2.41; Python 3.13.5; GCC 14.2.0;
CMake 3.31.6; Node 22.16.0; Chromium 144.0.7559.96.

Browser tests used an offline HTML document injected into Chromium. They covered
five-card capacity (including a direct attempt to create a sixth), replacing a
model, two-control Distortion+ layout, six JB-2 controls/mode labels, per-instance
stored CC association, duplicate/delete/Undo/Redo, LOW membership disclosure,
mode-view stability, preparation export/import, rejecting a six-instance file,
and one-row/no-horizontal-overflow layouts at 1280, 1024, 900 and 640 pixels.

The UI was visually inspected at 1280px using board and JB-2 editor screenshots.
No claim of pixel-perfect production artwork is made. Browser file-URL navigation
was blocked by the environment policy, so the tests used `page.set_content`; no
browser policy was disabled. This is not a Windows file-launch test.

All NAM and audio test payloads were synthetically generated. No original NAM
capture was loaded into the NAM runtime, no real guitar/bass DI was evaluated,
and no downloaded IR was approved for redistribution. `metadata_ready` is not a
hardware accuracy certificate or an executable model validity certificate.

Not run: production JUCE build, Windows VST3/standalone, Studio One/Cubase/Sonar,
real MIDI I/O, APVTS/project migration, actual A/B DSP recall, phase/latency/CPU
benchmarks of the full plugin. The existing release and production source are
unchanged; this branch only adds `Preparation/` source files.

Reproduce with `python run_checks.py --browser /path/to/chromium` after installing
the documented prerequisites. Machine-readable results and screenshots are
created in `reports/`; generated files are ignored by Git.
