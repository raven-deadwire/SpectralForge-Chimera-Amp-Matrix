# Owner release approval — October 16, 2026 (Asia/Seoul)

After receiving Windows test R3, the owner approved release conditional on fixing the tuner dock button lighting while its workspace was unopened. That presentation defect is corrected by deriving the active colour from workspace visibility, not the restored detector parameter. No audio algorithm or acceptance threshold changes in this final correction.

The approved candidate includes the preceding HEAD/CAB separation, LOW channel selector with rotary band tone/compression/blend directly below it, disabled mic routing, and tuner auto-start on opening with a separate mute option. Focused Windows UI regressions cover these behaviours. Source-specific platform and package evidence is still required by the publisher; this approval does not assert that macOS timing, physical listening or DAW checks passed.

Prior fd1f222 macOS focused run 38875904343 failed the unchanged settling limit (316.972334 ms, limit 290 ms); Product run 38875904365 also failed. These failures are retained. Publication approval is granted; publication qualification is not yet complete. Existing expiry and lifecycle policy remain unchanged.
