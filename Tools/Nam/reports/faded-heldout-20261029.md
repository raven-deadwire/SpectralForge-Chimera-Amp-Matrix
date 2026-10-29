# Náströnd A2 held-out signals with a faded ending, 2026-10-29

[fenrir-quiet-20261028](fenrir-quiet-20261028.md) attributed Fenrir's failing
validation quiet window to the hard cut at the end of `common.signal`, where
a still-sounding note drops to digital zero within one sample. This test
checked that by fading the final note out instead. **The hard cut is not the
cause; that attribution was wrong.**

## Method

`evaluate_faded.py` regenerates the validation and test inputs, checks that
they equal the dataset's, applies a 5 ms half-cosine fade ending where the
closing 0.5 s of silence begins (240 samples change), renders the five native
channels with the dataset's renderer (same SHA-256), and scores the packaged
models with profile v2 metrics and release profile v3. Training data, models
and profiles are unchanged.

## Result

| Channel | Validation, v3 failures | Test, v3 failures |
|---|---|---|
| Fenrir | quiet: residual peak −54.5 dBFS (was −53.3) | active worst 0.662 |
| Surtr | none | none |
| Nidhoggr | none | none |
| Fimbulvetr | active worst 0.445 | none |
| Ragnarok | active worst 0.803 | none |

The fade did not fix Fenrir's window, and it made three more fail. The new
failures are the same region: the window starting at 23.51 s, just after the
input stops. With the fade its reference sits between −55 and −57 dBFS, just
above the −60 dBFS active threshold, so it is judged as an active window and
its relative error (0.45 to 0.80) breaks the worst-window limit.

## What the failing region is

After the input stops, the native channels keep sounding for more than 25 ms:
their latency and release continue the last note, at −15 to −25 dBFS for the
louder endings and falling to −55 to −65 dBFS for Fenrir's quiet validation
ending. The A2 models follow the louder part to within about 20 dB, but on the
low-level part of the release their error is about as large as the native
output itself (Fenrir validation: native −55 dBFS, error −60 to −62 dBFS from
10 to 15 ms). It is a real modeling error on very quiet decays, about 35 dB
below playing level, not an artifact of how the signal ends.

Whether the error at that level is audible is for listening to decide. The
faded evaluation is not adopted: it does not remove the failure and moves
other windows across the active threshold.
