# Windows measured-process priority preflight

Manual controller only, based directly on immutable source
`781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333`. The only existing repository path
replaced by the controller is `.github/workflows/build.yml`; the other three
files are under this diagnostic directory. No product source is changed.

The workflow runs once on `windows-latest` with Python 3.12. It imports the
reviewed V2 launcher outside the separate source checkout, requiring SHA-256
`ececc24808409e63c838ab3fbfea6a5887a84f5a5d89dda8f6332b8a79daba6b`.
It performs three native controls, once each:

1. An ordinary unmeasured launch reports its own effective priority class.
   The control does not require that class to be NORMAL.
2. A measured launch must report NORMAL from both the frozen driver's real
   retained `Popen` handle query and the child's own `GetCurrentProcess` query.
   Returned and appended receipts must match; launch flags must include both
   NORMAL and CREATE_NEW_PROCESS_GROUP. The flag is not a separate observation
   of kernel process-group membership.
3. One intentional query failure first observes its actual owned live child's
   NORMAL class, then raises a labeled error. The unchanged V2 cleanup must
   terminate and reap that child while preserving driver exit 1. The test
   records the retained actual `Popen` object and requires a nonzero child exit;
   natural successful self-expiry cannot satisfy this cleanup control.

Every probe child creates no descendants and waits five seconds before normal
self-expiry. `run_logged` receives a ten-second timeout. Its existing taskkill
and cleanup waits retain the frozen thirty-second ceilings. There is no
subprocess adapter or mock; only the single intentional query exception is
injected. An emergency direct-child kill can preserve ownership but cannot
turn a failed control into a pass. Parent priority is queried before and after
every control and must remain unchanged.

The job has a five-minute outer limit and uploads small logs, exact launcher
bytes, source/controller identities and `verdict.json`, including on failure.
The only accepted marker is `PASS_NATIVE_LAUNCH_PREFLIGHT_ONLY`, with all three
controls passed. No CMake, DSP build, timing test, source push or retry is part
of this workflow. A pass establishes native launch/API compatibility on that
runner; it neither reproduces the prior audio wall stalls nor certifies a
release or proves their cause/cure.
