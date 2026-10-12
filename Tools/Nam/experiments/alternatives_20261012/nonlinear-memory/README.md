# Active nonlinear-memory probe — 2026-10-12

**Outcome:** the frozen native amplifier retains measurable nonlinear history after more than one complete 6,347-sample A2 receptive field. A fixed finite-history A2 followed by a fixed linear POST filter cannot preserve that response exactly for all four traces of the constructed witness. This does **not**, by itself, establish a violation of the current full/active fidelity thresholds or explain all observed NAM ESR.

No fitting, candidate tuning, channel-control change, output-anchor change or release approval occurred.

## Frozen protocol

- Native source: `d93560445c78552768a0ed592e5ec62c3c648fb6` and the already verified native renderer.
- Before rendering, the complete stimulus design, script/binary/control/input hashes and sample ranges were saved in `plan.json`, SHA-256 **`c9ca398e8c17cd4286da522f66c9c8d51c6b9b776b6c208fe7436e97642b0121`**.
- Sample rate: 48,000 Hz; native amplifier oversampling: 4×; no FX/cabinet; original five captured channel settings and fixed per-channel reference scales.
- Conditioning: one second of `(sin55 + .35 sin110 + .15 sin165)/1.5`, at levels 0.65 and 0.005.
- Common silent gaps: 150, 250, 500, 750 and 1,000 ms. Every gap exceeds the A2 receptive field of 132.229 ms.
- Identical 200 ms probe notes at levels 0.03 and 0.3, plus 200 ms of following silence. The onset envelope is `(1-exp(-t/.003))*exp(-t/.080)` with a half-cosine release over 180–200 ms.
- Each history has a matched zero-probe control. Two zero controls are reused only within the same channel/gap.
- Total: **50 predefined cases, 150 unique native stimulus/setting renders**. The predeclared largest native whole-probe difference received four additional actual baseline A2-plus-frozen-POST renders as a numerical control.

`results.json` retains the original measured statistics and render hashes. `anchored-summary.json` is the authoritative anchored comparison and contains the quartet energy bounds. `delta-waveforms.npz` retains all 50 measured native difference waveforms and the actual player control.

## Why this isolates nonlinear history

For high/low conditioned history `A/B`, compare the same note `p` with a zero control `0`:

\[
\Delta=(y_{A,p}-y_{A,0})-(y_{B,p}-y_{B,0}).
\]

Within each history, the note and zero-control renders are identical until probe onset. At probe onset, histories A and B have already shared more than a complete receptive field of silence. A finite-history A2 therefore has identical incremental responses to the probe in both histories. A fixed linear POST filter preserves that equality; its inherited ringing cancels in the paired subtraction. In exact arithmetic, the class predicts `Delta_hat = 0`.

A nonzero native difference identifies history-dependent nonlinearity outside that class. The result can arise from sag, blocking/bias recovery, their interaction with other stages, or the final nonlinear output rail. It does not uniquely identify which state contributes. It also does not exclude a stateful or larger-history model, or other architectures outside the fixed-A2-plus-fixed-LTI-POST class.

## All five channels: largest whole-probe effect

For each channel, the largest 200 ms difference among the predefined cases occurred at **150 ms gap / 0.03 probe level**. All amplitudes below use the original channel reference scale.

| Channel | Delta RMS, 200 ms | Delta peak | Minimax individual RMS-error lower bound | Pooled four-trace ESR lower bound, same 200 ms |
|---|---:|---:|---:|---:|
| Fenrir | −46.7248 dBFS | −26.3101 dBFS | −58.7660 dBFS | 0.0022070644 |
| Surtr | −48.3295 dBFS | −27.3857 dBFS | −60.3707 dBFS | 0.0001325753 |
| Nidhoggr | −47.2066 dBFS | −25.5218 dBFS | −59.2478 dBFS | 0.0002205176 |
| Fimbulvetr | −45.2230 dBFS | −29.2397 dBFS | −57.2642 dBFS | 0.0003069314 |
| Ragnarok | **−38.7482 dBFS** | **−22.6814 dBFS** | **−50.7894 dBFS** | 0.0007937721 |

These are diagnostic-specific bounds over the constructed quartet, not the ordinary full-signal ESR of a delivered NAM. The 20 ms onset pooled ESR lower bounds were 0.00731375, 0.00079386, 0.00006370, 0.00178697 and 0.00409362 respectively. Those windows also remain separate from the product's own canonical fidelity windows.

The low-versus-high conditioning changed onset RMS level by −1.2545 dB for Fenrir, −0.5029 dB for Surtr, −0.1138 dB for Nidhoggr, −0.9868 dB for Fimbulvetr and −0.5607 dB for Ragnarok. Over the full probe, some level changes are much smaller despite a nonzero waveform difference. A single output-gain correction would not generally remove the measured history-dependent waveform changes.

## Persistence across the predefined gaps

The table shows native whole-probe Delta RMS for the 0.03 probe. The 0.3-probe results and onset/late/post-probe statistics are retained in the JSON; no extra input was selected or fitted.

| Channel | 150 ms | 250 ms | 500 ms | 750 ms | 1,000 ms |
|---|---:|---:|---:|---:|---:|
| Fenrir | −46.7248 | −56.3999 | −112.8281 | −171.2175 | −217.3689 |
| Surtr | −48.3295 | −63.4178 | −86.1198 | −109.9346 | −133.7697 |
| Nidhoggr | **−47.2066** | **−47.3061** | **−47.4551** | **−48.3105** | **−49.8039** |
| Fimbulvetr | −45.2230 | −53.2430 | −60.0381 | −69.3514 | −78.8299 |
| Ragnarok | −38.7482 | −48.0365 | −65.9102 | −82.7271 | −99.7410 |

Values are dBFS; they are not pass/fail classifications. In this witness, Nidhoggr retains a substantial difference after one second, consistent with the need to investigate its long nonlinear recovery. Consistency with the source's recovery constants is not causal attribution to one state.

## Actual A2-plus-POST numerical control

The predeclared largest native case was Ragnarok / 150 ms / level 0.03. The unchanged legacy Ragnarok A2 and its already-frozen legal native POST EQ were rendered through the actual pinned TONE3000 components for all four inputs.

| Quantity, whole 200 ms probe | Native teacher | Actual baseline A2 plus fixed POST |
|---|---:|---:|
| Anchored Delta RMS | **−38.7482 dBFS** | **−98.6939 dBFS** |
| Anchored Delta peak | −22.6814 dBFS | −87.5375 dBFS |

The native RMS difference exceeds this measured numerical-control residual by **59.9457 dB**. The exact-zero property concerns the mathematical finite-history/LTI class; floating-point arithmetic yields a small measured residual. One numerical control is not a uniform rounding-error theorem for all possible weights and filter settings.

The initial player statistics were retained in raw NAM output units (`Delta RMS = −72.9517 dB`). The summary applies the **same original Ragnarok output anchor, 0.05162876564505051 = −25.74216516 dB**, used for the native teacher. This is a units correction, with no rerender, normalization fitting or change to the model. The raw/anchored distinction is recorded explicitly in `anchored-summary.json`.

The tool emitted incidental callback timing counters, but this small structural probe is not a real-time performance qualification. No performance or release pass is claimed.

## Bounds and their limits

Let the four same-window reference vectors be `r_i`, their errors `e_i`, and use the same output anchor and sample range throughout. The exact-arithmetic class has `Delta_hat = 0`, so the signed sum of the four errors equals the measured reference difference up to sign.

Triangle inequality gives, for either RMS or the infinity norm,

\[
\max_i\|e_i\|\ge\frac{\|\Delta\|}{4}.
\]

Cauchy–Schwarz gives

\[
\|\Delta\|_2^2\le4\sum_{i=1}^{4}\|e_i\|_2^2,
\qquad
\frac{\sum_i\|e_i\|_2^2}{\sum_i\|r_i\|_2^2}
\ge\frac{\|\Delta\|_2^2}{4\sum_i\|r_i\|_2^2}.
\]

The reported pooled four-trace ESR lower bound uses the latter expression. It concerns a model's simultaneous approximation of the four traces, not a per-trace ESR average and not the ordinary product test score. No product threshold is substituted for these diagnostic-specific norms.

To calculate exact same-window quartet energies after the initial probe, 20 already-rendered traces were reconstructed for the five selected cases. Every reconstructed output was required to match its original SHA-256 byte for byte; all 20 did. The addendum was frozen in `window-energy-reconstruction-plan.json` before those reconstructions. **No new stimulus/setting conditions, tuning or candidate selection were introduced.** The original 150-condition results remain unchanged.

## Development implication

The supported POST EQ route can still help the separately demonstrated stationary quiet-tail problem. Preserving all of the native amp's active history exactly will require more than a fixed finite-history A2 followed only by a fixed LTI POST filter. This experiment does not establish that the measured nonlinear-history floor alone exceeds the existing full/active acceptance thresholds, nor that it accounts for the current 0.04–0.12-class full ESR. The existing bare-A2 Fimbulvetr quiet-window impossibility result remains a separate, stronger threshold-specific statement.

Release approval remains **false**. There was no threshold reduction and no shipment or publication of a failed model.
