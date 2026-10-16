# CAB/IR JUnit truncation regression

Unmodified raw files from GitHub Actions run 38886446510, attempt 1,
Linux job 116925636448, source 4beb5a27ec11d8b2f6783ef5db4acc7bba422a60.
Artifact 12040082339 (`CAB-IR-component-linux`).

All three native suites passed; the old consumer correctly refused to claim
A5.05/A6.05 because CTest truncated their JUnit stdout at 1024 bytes. The separately
hashed full LastTest.log includes all 20 boundary assertions. The consumer now
requires both matching receipts and reuses release_evidence.contract_passed for
completed-section validation. These historical files are parser fixtures only;
they do not certify a later product source/run or authorize publication.
