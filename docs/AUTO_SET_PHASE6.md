# Auto Set — phase 6 (Dynamic EQ)

Phase 6 lets Auto Set place Dynamic EQ nodes on the Tone EQ. A node cuts a
narrow band only while that band rings louder than the rest: a note that
jumps out, or a cabinet resonance that a few notes excite. Between those
moments the sound is unchanged. Phases 1–5 are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE5.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dynamic EQ** step, on by
default. It adds no listening time: it uses the Tone EQ pass. Play across the
neck, including the notes that tend to boom or honk.

- With the **Tone EQ** step off, the Tone EQ pass still runs for the Dynamic
  EQ. It then leaves your Tone EQ curve as it is.
- **The results** get a **Dynamic EQ** row after the Tone EQ row. It lists each
  node: frequency, threshold, ratio and maximum cut. Its SHOW button opens the
  Tone EQ.
- **Strength** scales the maximum cut. Frequency, threshold, ratio and times
  stay the same.
- **Lock → Dynamic EQ** keeps the Dynamic EQ slots and skips the step. A
  **Tone EQ** lock skips it too, because the nodes live on Tone EQ bands.
- **UNDO AUTO** restores the slots and Tone EQ bands the step used. A newly
  selected Tone EQ/Dynamic EQ lock, or a newer edit to any part of an auto node,
  preserves the complete node and the EQ bypass state. Editing an EQ/slot while
  listening makes Phase 6 abstain; reentrant edits cannot leave a partial node active.

## Where the nodes go

- **Slots:** at most two of the four Dynamic EQ slots per run, and only slots
  that are unassigned. A slot already on a Tone or Final EQ band is kept, so
  your own dynamic nodes are never moved.
- **Bands:** Tone EQ bands that are off after the Tone EQ step and have no
  slot. A node is a 0 dB bell, so it only cuts while its band is above the
  threshold.
- **Not placed:** next to an existing Tone EQ dynamic node (within 2/3
  octave), or next to each other.
- **A re-run** keeps the nodes of the earlier run (they hold their slots) and
  can add up to two more. Use UNDO AUTO before re-running to replace them.
- **A bypassed Tone EQ:** when it holds your curve and the Tone EQ step did not
  rewrite it, the step keeps everything. Switching the Tone EQ on would make
  that curve audible. A bypassed Tone EQ with no band on is switched on for
  the nodes.

## Measurement

The Tone EQ pass captures the amp/cab output before the Tone EQ. This is where
the Dynamic EQ detector listens. Each third-octave band (50 Hz – 10 kHz) goes
through the same band pass as the detector (Q 3), read in 20 ms blocks. Only
blocks within 15 dB of the loud playing are used, and at least 50 are needed.

For each band, using block RMS (a pick click lights every band's peak, so
peaks would hide the ring):
- **loud** is the 95th percentile and **typical** the median;
- **excess** is the band's loud level minus the median loud level of its
  neighbours. The neighbours are 2/3 to 4/3 octave away, on both sides, so a
  broad tilt cancels.
- **relative jump** is P95 minus median of `band RMS dB - full RMS dB`,
  paired within each playing window. A normal note decay or common gain change
  must not count as a band-specific event.

## Rules (`analyseDynamicEQ`)

| | Rule |
|---|---|
| Searched range | guitar 80 Hz – 4 kHz; bass effective 80 Hz – 2.5 kHz. The configured bass lower bound is 50 Hz, but the first two centres (50/63 Hz) lack lower neighbours and are skipped. No claim of direct 50/63 Hz detection. |
| A band acts when | excess ≥ 4 dB, loud − typical ≥ 6 dB (it comes and goes; a band that never drops back is the Tone EQ's job), relative jump ≥ 6 dB, and loud within 24 dB of the loudest playing |
| Order | strongest excess first, up to the free slots (at most two) |
| Threshold | 80th percentile of the band's 20 ms peaks: it acts on the loudest fifth |
| Ratio | 2:1 below 6 dB excess, 3:1 below 9 dB, else 4:1 |
| Maximum cut | round(excess − 1), 3–9 dB, times strength |
| Q | 3 (node and detector) |
| Attack / release | below 250 Hz 20 / 250 ms; below 1.5 kHz 10 / 150 ms; above 5 / 80 ms. Release × 0.7 for metal, × 1.3 for clean |

## Testing

`ChimeraAutoSetTests`:
- **Rules (`dynamicEQRules`), at 44.1, 48 and 96 kHz:**
  - Silence reports no playing.
  - Notes of a two-octave pentatonic scale, evenly voiced, get no node. The
    largest excess at 48 kHz is under 1 dB.
  - The same notes through a +12 dB, Q 8 resonance at 250 Hz get a node at
    250 Hz. At 48 kHz its excess is 8 dB: threshold −19 dB, 3:1, up to 7 dB,
    attack 10 ms, release 150 ms.
  - Metal shortens the release to 105 ms.
  - An existing node at 240 Hz blocks the 250 Hz one, and no free slot means no
    node.
- **Controller (`controllerDynamicEQ`), real processor, fixtures fed to the
  capture taps:**
  - With the Tone EQ step off, the Tone EQ pass runs on its own and slot 1
    gets a 0 dB bell at 250 Hz matching the analysis. Only that band is
    enabled, the Tone EQ is switched on, and UNDO AUTO restores every
    parameter.
  - At 50% strength the maximum cut halves and the threshold is unchanged.
  - When all four slots are assigned, the Tone EQ and slots are unchanged and
    the row says so.
  - A user curve behind a bypassed Tone EQ stays bypassed and unchanged.
  - A Tone EQ lock skips the pass and reports both steps in order. A Dynamic
    EQ lock keeps only this step and assigns no slot.
- **Full run (`controllerIntegration`):** seven result rows. Dynamic EQ is the
  fourth, and the nodes match the analysis. The user's own node on Tone band 2
  is kept.
- **Locks (`controllerEveryLock`):** all eight locks at four strengths,
  including Dynamic EQ.

## Not yet validated

The 4 dB excess, the 6 dB come-and-go limit, Q 3, the ratios, the cut range
and the times are starting values. They have been checked on synthetic notes,
not on real guitars, basses, cabinets or listening tests.

## DI and application regressions

See [the reproducible DI protocol](AUTO_SET_PHASE6_DI_REGRESSION.md) for the nine
planned takes, fixed amp/cab captures, missing-input checks and before/after WAVs.
`ChimeraAutoSetTests --dynamic-regression` exercises low A0/B0, high-gain sustain,
common-envelope dynamics, positive ring controls, actual GraphicalEQ gain reduction,
strength, user curves, linked node ownership and Undo. Synthetic controls do not
stand in for real DI, real bass IR or listening acceptance.
