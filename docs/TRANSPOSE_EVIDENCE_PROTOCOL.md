# Transpose quality/latency — listening & measurement evidence protocol

Goal: produce the evidence `docs/TRANSPOSE_LONG_TERM.md` requires before the transpose
quality/latency mode can enter release scope or be advertised — specifically the
**isolated-DI, bypass-vs-transpose** comparison, for all three modes
(CLASSIC ~43 ms / LOW-LAT ~21 ms / TIGHT ~11 ms), with the deferral's core question
answered: *does the smaller FFT window trade away enough low-register quality to matter, and
is the latency it buys worth it?*

Capture **objective measurements** and **blind listening** so the decision rests on evidence,
not impression. Record everything so a second person can reproduce it.

---

## 0. Fix the variables first (write these into every result file)

- Plugin build: exact commit SHA (SETTINGS ▸ Beta/Updates shows it) + OS/arch.
- Host + sample rate + block size (test at least **48 kHz** and **96 kHz**; block 64 and 256).
- Signal chain: **isolated DI only** — AMP off, CAB off, every pedal/rack/EQ/dynamics off,
  doubler off. Transpose is the only active block. This is what makes artifacts audible in
  isolation instead of being masked by an amp/cab.
- Fixed input gain; no normalization between conditions except the explicit loudness-match
  step (§4). Same input files for every pass.
- `formantPreserve` state recorded per pass (default off — test both off and on, since it is
  the other opt-in knob and interacts with window size).

Prefer **offline render/bounce** for the objective half so passes are bit-reproducible; use
**live playing** only for the latency-feel half (§5), which can't be judged offline.

## 1. Test material

Objective (rendered):
- Sine **sweeps** 20 Hz–10 kHz, and steady **single tones** at musically relevant pitches.
- **Low register is the stress case** — low E2 ≈ 82 Hz, drop notes, 5-string B0 ≈ 31 Hz for
  bass. The deferral is specifically about low-register quality, so weight it.
- **Impulsive attacks** (clicks, hard pick/palm-mute transients) for time smearing / pre-echo.
- **Dyads and 4–6 note chords** for polyphony stress (phase-vocoder warble shows here).
- **Sustained chords** for steady-state timbre / formant / phasiness.

Subjective (rendered + live): real musical passages — clean arpeggios, low chugs, a lead line,
a sustained chord — played once and reused across all conditions.

## 2. Condition matrix

For each input item, render:

| Shift (semitones) | bypass | CLASSIC | LOW-LAT | TIGHT |
|---|---|---|---|---|
| 0 (transparency check) | ref | ✓ | ✓ | ✓ |
| −12 (octave down — worst case) | ref | ✓ | ✓ | ✓ |
| −5, −7 | ref | ✓ | ✓ | ✓ |
| +5, +7, +12 | ref | ✓ | ✓ | ✓ |

"bypass" = transpose OFF (the reference the doc asks you to compare against). Down-shifts stress
the low register hardest; keep −12 central to the judgement.

## 3. Objective measurements (per rendered file)

1. **Actual added latency** per mode: feed an impulse, find output delay, confirm it matches
   the reported ~43/21/11 ms and that the host receives correct PDC (plug-in delay
   compensation). A wrong report here is a straightforward bug, independent of taste.
2. **Pitch accuracy**: FFT a sustained shifted note; fundamental should land within a few cents
   of target. Compare modes.
3. **Low-register integrity** (the crux): for −12 on a low note, measure energy below ~120 Hz,
   fundamental-vs-harmonic balance, and THD vs the CLASSIC reference. Quantify how much body
   TIGHT/LOW-LAT lose relative to CLASSIC.
4. **Transient smearing / pre-echo**: measure attack-time spread and pre-ring per window size
   (expected to worsen as the window shrinks — quantify it).
5. **Formant / spectral envelope**: LPC or cepstral envelope of a sustained note, with
   `formantPreserve` off vs on, per mode. Confirms the true-envelope path holds formants and
   shows how window size affects it.
6. **Transparency null at shift 0**: if CLASSIC at 0 is meant to be (near-)transparent vs
   bypass, null them and report residual. Flags any unintended coloration from the path itself.
7. **Artifacts/aliasing**: single-tone in, look for in-band spurious partials / warble energy.

Emit each as CSV/JSON + a plot. A note on tooling: `Tools/capture_cabinet_ir.py` and ESS/Kirkeby
deconvolution characterize **linear, time-invariant** responses — transpose is neither, so do
**not** use ESS to "measure" it. Use direct note/sweep analysis and null tests instead.

## 4. Loudness-match before any listening

Measure integrated loudness (LUFS) or RMS of each pass and apply a static gain so every
compared pair is level-matched to within ≈0.3 dB. **Skipping this invalidates the listening
test** — the louder clip almost always "wins." Log the applied gains.

## 5. Blind listening (subjective)

- **Rendered A/B/X**: randomize/relabel the loudness-matched clips; the listener does not know
  which is which. For each attribute rate CLASSIC vs LOW-LAT vs TIGHT (and each vs bypass):
  low-end body, clarity, warble/artifacts, transient feel, overall preference. Repeat trials;
  if you can, run proper ABX for "can you even tell them apart."
- **Live playability** (this is why the latency modes exist, and it can't be rendered): play
  through each mode live, note monitoring latency (buffer + plugin) and how "tight"/playable
  each feels for leads vs the quality cost. Document the monitoring chain and buffer.
- Capture multiple listeners if possible; if it's just you, say so plainly — one honest
  evaluator is fine as long as it's recorded as such.

## 6. Decision criteria (set these BEFORE listening)

The deferral's question is a trade, so write the thresholds up front, e.g.:
- LOW-LAT/TIGHT are acceptable to **offer** if, at the shifts you care about, the measured
  low-register loss and the blind-preference gap vs CLASSIC stay within a bound you commit to
  now (pick it), for the latency each buys.
- A mode may **not** be advertised as low-latency transpose until its latency report is verified
  (§3.1) and its artifact/quality cost is documented here — exactly what the doc defers.
- Record the outcome per mode: ship / offer-with-caveat / keep-feature-branch-only.

## 7. Store the evidence (reproducible)

Put raw and derived artifacts under a dated folder (mirror the preset precedent, e.g.
`evidence/transpose-modes-YYYYMMDD/`): input WAVs, rendered condition WAVs, measurement
CSV/JSON, plots, the loudness-match log, blind-test response sheet + the label key, and a
README stating build SHA, host, sample rates, buffers, monitoring chain, and who listened.
Then update `docs/TRANSPOSE_LONG_TERM.md` to point at it and record the §6 decision.

> Note on committing artifacts: the preset-maintenance scope gate only runs on `main` pushes
> now (PRs are exempt after the recent workflow change), and `evidence/preset-dynamics-eq-*`
> is the only evidence path it whitelists — a new `evidence/transpose-*` folder is fine on a
> feature PR but would need the owner's scope decision before it rides a preset-maintenance
> publication. For a plain feature branch it's just normal files.
