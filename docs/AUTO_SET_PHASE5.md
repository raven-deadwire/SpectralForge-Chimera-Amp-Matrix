# Auto Set — phase 5 (Gate 2 and Expander)

Phase 5 lets Auto Set set the newer gating stages: Gate 2, on the other side of
the rig from the main gate, and the separate Expander. Phases 1–4 are described
in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE4.md`. The rules are fixed, as in the
earlier phases.

## Use

The options button next to **AUTO SET** has a **Gate 2 and Expander** step, on
by default. It adds no listening time: it uses the input pass (pass 1). Play
normally and leave a short pause.

- **While the input pass listens,** every gate is held open. You hear the amp's
  own hiss in the pause; the progress view shows it as "Amp hiss: … dBFS".
- **The results** get a **Gate 2 + Expander** row after the input row. Its SHOW
  button opens the Gate page.
- **UNDO AUTO** restores all the gate, Gate 2 and Expander parameters, including
  the gate placement and mode.

## Measurement

The input pass now listens at two points together, as 10 ms windows:
- **Input:** after input gain, where the gates detect.
- **Rig:** the amp/cab sum, before any post-rig gate.

While it listens, the processor holds every gate open through a
non-parameter flag (`setAutoSetGateBypass`). This writes nothing to the host,
and the flag is cleared when the pass ends, Auto Set is cancelled or fails, or
the controller is destroyed.

The input windows decide when the player pauses. They use the same split as the
gate rule: quiet windows within 3 dB of the 10th percentile, playing windows
within 12 dB of the 90th. Windows next to a level change are skipped, so the
latency between the two taps (the plug-in latency, in windows) hardly matters.

From the rig windows:
- **rig noise** is the 90th percentile peak in the pause;
- **playing floor** is the 10th percentile peak while playing;
- **gap** is playing floor − rig noise.

At least 10 windows of each are needed. With no pause, nothing changes.

## Rules (`analyseGateStages`)

| Case | Result |
|---|---|
| Rig noise ≤ −66 dBFS and the style is not metal | **Off.** Gate 2 and the Expander are switched off; the phase 1 gate is enough. |
| Metal and gap ≥ 24 dB | **Tight.** Main gate after the rig, in Gate mode, with the phase 1 threshold and range. Gate 2 (therefore before the rig) in Expander mode, 2:1, 6 dB below the main threshold, range round(gap ÷ 2) clamped to 9–18 dB, attack 0.5 ms, hold 10 ms, release 80 ms. The Expander is off. |
| Otherwise | **Smooth.** The Expander takes the phase 1 gate threshold. Ratio: clean 1.5, rock 2, metal 2.5. Range: enough to bring the hiss toward −78 dBFS, round(rig noise + 78) clamped to 6–15 dB. Attack 2 ms, hold as the gate (at least 10 ms), release: clean 200, rock 120, metal 80 ms. It sits after the rig when the amp hisses or the main gate is already there. The main gate switches to Gate mode and closes only where the Expander's range is used up: threshold − (range ÷ (ratio − 1) + 3 dB), clamped to 9–24 dB lower. Gate 2 is off. |

**Why these choices.**
- Every stage detects on the clean input, so the rig's noise doesn't change
  *when* they open. It decides *how much* they must cut, and *where* the cut
  is useful: after the rig to remove the amp's hiss, before it to keep hiss out
  of a high-gain amp.
- The smooth case keeps note tails fading instead of chopping them. The tight
  case is the metal-style hard stop: the rig has headroom (gap ≥ 24 dB) to gate
  after it.

## Testing

`ChimeraAutoSetTests`:
- **Rules on synthetic windows:**
  - A quiet amp leaves both stages off.
  - Faint and strong hiss give a shallower and a full Expander range.
  - The clean, rock and metal ratios and releases, and the deeper main gate.
  - Metal with headroom gives the post-rig gate and a PRE Gate 2.
  - Metal with a narrow gap gets the Expander instead.
  - No pause leaves everything unchanged.
- **Capture:** the rig tap records windows alongside the input tap.
- **Controller on the real processor:**
  - The results list six rows, with Gate 2 + Expander second.
  - The applied stages match the analysis.
  - The input pass reports the amp's hiss.
  - UNDO AUTO still restores everything.

On the synthetic guitar DI (−72 dBFS noise) the default rig turns the pause into
about −12 dBFS of hiss. Auto Set then sets the Expander after the rig at the
gate threshold, 2:1 up to 15 dB, and the main gate 18 dB lower.

## Not yet validated

The −66 dBFS hiss limit, the 24 dB headroom for the tight case, the ratios and
the releases are starting values. They have not been tuned against real
instruments, amps or listening tests.
