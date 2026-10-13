# Fresh source object and presentation audit

Source: `6033236cb81820a43679cbbd34be020804f9658d`; tree: `7b23b503b807db36ec4f51491d6aa5f8c29e7b8c`.

This audit freshly retrieved four exact-source Git trees and the relevant current/donor source files through GitHub. The 19 assertions were evaluated in JavaScript in the connected tool environment. It is not a Python/C++ local execution or reuse of prior-source tests. The local executor remained offline.

`audit-summary.json` records every assertion, mappings, source blob identities and original current-source CI receipts. `raw/` preserves the unmodified tree API response bodies. `source-603`, `donor-022244e6` and `donor-d7450883` reference the exact original Git blobs. `audit-algorithm.js` is the evaluated function; its `load` inputs are the same raw trees/file contents and original validation-contract job log supplied with this evidence. Input source/ref identifiers are explicit in the summary.

The actual validation-contract CI log is included, with its original checkout proof in the summary. Passing test internals hidden by CTest `--output-on-failure` are not invented: later original artifact audits can expose those receipts. Runtime, installer lifecycle and release qualification evidence are tracked separately. No release authorization is implied.
