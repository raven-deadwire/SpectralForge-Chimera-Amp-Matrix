# Auto Set — phase 43 (muted and soloed rigs and lanes in the rig and lane passes)

Phases 33 to 42 listen to one Dual rig or one Matrix lane alone and set that
head. A rig or lane that is **muted**, or **not soloed while another is**,
plays nothing: its pass heard silence after the cabinet and either reported
"no playing heard" or judged the head from the band going in. Phase 43 keeps
such a rig or lane, says why, and does not spend a listening pass on it. There
is no new step or option, and no rule changes.

## What changes

- **Before the run:** for each rig (Dual Blend, phases 33 to 36; Dual
  Crossover, phases 37 to 39) and each lane (Matrix MID and HIGH, phases 40 to
  42), Auto Set reads MUTE and SOLO as the engine does: a lane is silent when
  it is muted, or when any lane of the routing is soloed and it is not. A
  silent rig or lane gets **no pass**; each of its result rows that is on
  reads "kept (this rig is muted)", "kept (another rig is soloed)", "kept
  (this lane is muted)" or "kept (another lane is soloed)".
- **The other rig or lane is still listened to and set.** In the Matrix, the
  LOW lane's SOLO counts: with only LOW soloed, MID and HIGH are both kept.
- **While listening:** if a rig or lane is muted, or silenced by a solo, by
  the time its pass ends, its rows are kept with the same reason.
- **A locked step still reads "kept (locked)".**
- These steps are all off by default; nothing changes for the default run.

## Not changed

The Dual and Matrix passes of earlier phases (levels, blend, crossovers, cuts,
IRs, mics) keep their own handling of silent lanes. The rules, the captures
and the audio thread are untouched.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --lane-mute`):

- **Controller (`controllerLaneMute`, real processor):** Dual Blend: rig 1
  muted (no pass for it, the reason on its row, rig 2 set); rig 2 soloed; both
  soloed (both set); every rig step on (the reason on each of the muted rig's
  four rows); muted while its pass listens; with the Amp lock the row reads
  locked. Dual Crossover: rig 2 muted, the reason on its three rows. Matrix:
  HIGH muted; LOW soloed (both lanes kept, no pass); MID soloed (HIGH kept).

## Not yet validated

The tests feed the taps directly, so they check that the controller reads
MUTE and SOLO, not that the engine's rig tap is silent for such a lane (the
engine's mute rule is read from `ChimeraDSP.h`, not exercised here). No real
DI or listening test.
