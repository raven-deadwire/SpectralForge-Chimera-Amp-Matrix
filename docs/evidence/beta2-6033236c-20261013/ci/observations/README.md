# Exact-source CI observations

Product source `6033236cb81820a43679cbbd34be020804f9658d`, tree `7b23b503b807db36ec4f51491d6aa5f8c29e7b8c`.

All11 original product workflows and33 jobs completed successfully. All33 original job checkout logs contain this exact source SHA. This is fresh-source evidence; earlier c3/937 passes are excluded.

The original gate producer declared **BLOCKED**, with21 PASS,107 BLOCKED,0 NOT_APPLICABLE checks and21/63 hard gates passing. GitHub workflow success is not release eligibility. The candidate artifact and gate artifact remain bound to the same source/run in original metadata.

`raw/` contains14 unchanged final API response bodies: source-filtered run collection, all11 corresponding latest-job collections, and Product/Update artifact inventories. `summary.json` contains URLs, source/repository bindings,33 job checkout excerpts, exact native CTest receipts and candidate/gate metadata. The four native matrix/update full logs plus assembly/gate full logs are preserved under `logs/`. The exact product build workflow is preserved under `source/` to show the gate command and lack of continue-on-error on its verdict step.

The local executor remained offline; unchanged Python verification was not claimed as locally rerun. Root's separately sourced read-only remote audit has its own controller commit/run and is excluded from product11/33 counts. This observation bundle itself does not claim archive byte verification, installed DAW/listening acceptance, publication approval or release eligibility.
