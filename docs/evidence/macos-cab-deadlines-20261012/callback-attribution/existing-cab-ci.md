# Existing CAB acceptance CI — run 38225982755

The existing [CAB realtime performance run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225982755)
completed successfully on all three operating systems for candidate source
`35617a2e0a5a599c5bd2b2a13113b6f234cd60a3`. This is the existing console acceptance
workflow. The new callback-attribution ABBA/BAAB workflow is separate and its
completion is not established by this receipt.

## Candidate gate results

Each candidate ran the five original console targets. All 66 original p99 gates
passed on each operating system, covering 105,600 measured callbacks per OS.
Individual wall deadline misses remain visible:

| GitHub job | Result | Original p99 gates | Individual wall misses | Misses at 96 kHz / 64 samples |
|---|---|---:|---:|---:|
| [macos-15, arm64](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225982755/job/114733484339) | Success | 66/66 | **41** | **18** |
| [ubuntu-22.04, x86_64](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225982755/job/114733484672) | Success | 66/66 | 0 | 0 |
| [windows-latest, AMD64](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225982755/job/114733484675) | Success | 66/66 | 0 | 0 |

The 96 kHz / 64-sample count includes six integration route rows and two separate
expanded-v2 extended-timing rows: 12,800 callbacks per OS. It is not limited to
the six integration rows displayed in the workflow's summary table.

Candidate receipts record the exact source and tree
`27e6ea93d1eb810a4ba9646cf6260371aec56cbe`, with clean source before and after
execution. Logs confirm the 144-route realtime equivalence checks, direct FIR
six-mic checks, exact residue reference checks, 50 ms fade, and zero latency.
The existing raw-evidence contract suite also passed 20 tests on every OS; that
suite is distinct from the new attribution verifier's 21 corruption tests.

## Separate reference and profiling runs

The fixed reference for this workflow is
`aaa1330dd5cf7d51faf1c45a7cef633b9b118458`, with three tests, 36 p99 routes, and
57,600 callbacks. Its p99 gates passed on all three systems; retained misses were
macOS 6, Linux 0, and Windows 2. This is not the released `7b20cd9…` frozen
reference used by the new attribution investigation.

The existing `ChimeraCabProfiling --expanded` diagnostic completed with exit 0
on all three systems. Each run contains 12 routes, 19,200 callbacks, 216 aggregate
profile rows, and 60 worker profile rows. Its individual wall misses were macOS
12, Linux 0, and Windows 0. It remains a separate aggregate diagnostic; it is not
the new callback-attribution probe and its timing does not replace candidate
acceptance.

## Independent artifact review and limits

All three completed job logs and artifact metadata were retrieved. The three
downloaded ZIPs matched GitHub's published SHA-256 digests. Each ZIP contained
108 entries, and all 107 files listed in its internal hash manifest matched
their SHA-256 values and sizes. The hash-list file itself is authenticated by
the ZIP digest. Source receipts, CTest XML, logs, and existing raw-sample
verification receipts were reviewed; full CSV semantics were not rerun locally.
The [machine-readable receipt](existing-cab-ci.json) preserves artifact IDs,
digests, source binding, job statuses, clock availability, and exact summaries.

The 41 macOS misses show why a successful p99-based run is not a zero-miss
guarantee. These sequential reference/candidate/profile runs do not demonstrate
an independently repeated performance effect. Windows thread cycles remain
cycles and are not converted to CPU duration. AU/VST3 hosting, CoreAudio device
callbacks, uninterrupted DAW playback, and editor interaction remain
**NOT_EXECUTED**. New attribution run
[38225976064](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225976064)
requires its own results and review.
