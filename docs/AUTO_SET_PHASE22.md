# Auto Set — phase 22 (Delay tempo sync)

Phase 22 lets Auto Set turn on the delay's tempo sync, following the host.
Phase 11 (`AUTO_SET_PHASE11.md`) adds a delay only with a sync tempo: onset
gaps do not establish a metrical tempo, so without DELAY SYNC it keeps the
delay off and says "select DELAY SYNC". When the host reports a tempo, this
step selects it for the player. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE21.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Delay sync to the host
tempo** step, on by default, listed before **Delay and reverb**.

- **When it runs:** inside the delay + reverb step (after the passes, before
  the Final EQ and output pass), first. It adds no listening time. With the
  delay + reverb step off or locked there is nothing to time, and the row
  says so.
- **What it sets:** **DELAY SYNC** on and **Follow host tempo** on.
- **What it leaves alone:** Chimera's own TEMPO, the delay time, feedback and
  mix (phase 11 sets the delay), the reverb.
- **The results:** a **Delay tempo sync** row, before the **Delay + reverb**
  row, shows the host tempo it synced to, or why it kept the settings. The
  delay + reverb row then shows the delay it added ("delay on: host tempo
  90.0 BPM ...").
- **Strength:** not applied. Sync is turned on or kept.
- **Lock → Delay tempo sync:** keeps both switches.
- **UNDO AUTO:** restores them.

## Rules (`analyseTempoSync`, `AutoSetSpace.h`)

- **Host tempo:** the tempo the host reported in the latest audio callback,
  40–240 BPM (the range phase 11 accepts). The processor now records it
  whether or not Follow host tempo is on (`reportedHostTempo`).
- **Turned on** only when all hold:
  - the delay is off, and DELAY SYNC is off (a delay with its own time, or a
    sync the player chose, manual/TAP or host, is kept);
  - the host reports a tempo;
  - phase 11 would add a delay at that tempo, from the playing the input
    pass heard: not bass, not metal, no phrase faster than 4 notes a second.
- The delay + reverb step then uses the host tempo just read (the processor
  reports a followed host tempo from its next callback).
- **Kept, with the reason:** locked; no playing heard; sync already on; a
  delay already on; no host tempo ("set Chimera's TEMPO or TAP and select
  DELAY SYNC"); bass, metal style or a fast phrase; sync edited while
  applying.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --sync`):

- **Rules (`syncRules`):** sparse clean guitar with a 90 BPM host tempo wants
  a delay (667 ms, mix 0.18). No tempo, 30 BPM, 300 BPM and NaN are not used.
  Bass, metal and a fast phrase (5 notes a second) want none. Silence reports
  no playing.
- **Controller (`controllerSync`), real processor with a host that reports a
  tempo:**
  - At 90 BPM, sync and Follow host tempo go on, the row comes before the
    delay + reverb row, and that step adds the delay at 90 BPM.
  - No host tempo: kept, the delay stays off and phase 11 still says "select
    DELAY SYNC".
  - Sync already on, a delay already on, and metal: kept.
  - The lock keeps the switches; with Delay + reverb locked the row says so.
  - UNDO AUTO restores them.
- **Full run (`controllerIntegration`, no host tempo):** twenty-three result
  rows, with the sync row (kept, no host tempo) before the delay + reverb row.
- **Locks (`controllerEveryLock`):** all twenty-five locks at four strengths.

- **Settled rendered timing (`syncTiming`, verification case DELAY-01):**
  - Controller rows and switches are checked at 100 BPM.
  - Existing 48 kHz measurements were 600 ms at 100 BPM, 500 ms after a
    change to 120 BPM, and 667 ms with no BPM (internal TEMPO 90), within 2 ms.
  - Correction to the original PR record: `TempoPlayHead` previously used
    `if (bpm > 0)`, so its purported NaN measurement was another missing-BPM
    measurement. It was not evidence that the processor received NaN.
    Availability is now independent of value, and the fixture asserts that
    an engaged BPM containing NaN actually reaches the playhead interface.
  - This older test deliberately settles before measurement. Its PASS must
    not be used as a transition-artifact or DAW playback qualification.

## Boundary, continuous-callback and state regressions

`Tests/AutoSetSyncRegression.h` is included in the full suite and `--sync`.
Use **`ChimeraAutoSetTests --sync-boundaries`** to run only the added coverage,
without rerunning the original settled-tempo/controller cases.

- **Rules:** 39.9, 40, 100, 120, 240 and 240.1 BPM exercise inclusive bounds.
  Invalid numeric values remain distinct from unavailable host information.
- **One continuous processor:** change BPM between audio blocks without
  resetting, preparing again, or discarding a warm-up render. Check effective,
  reported and followed tempo after every block; confirm internal TEMPO 90 is
  not overwritten. Render and check every sample for finiteness, including
  transition blocks. Each 2 s segment contains a burst at 300 ms, with an echo
  search extending 1700 ms after the direct onset, so the 40 BPM / 1500 ms echo
  fits. Settled echo timing has a +/-2 ms tolerance at 48 kHz.
- **Host availability:** 100 -> missing BPM -> 120; missing PositionInfo;
  detached playhead; real NaN, infinity, zero and negative BPM; recovery to a
  valid host tempo. The fixture asserts numeric payload delivery.
- **Live diagnostic signal:** a continuous 1 kHz tone survives 100 -> 120 ->
  100, a single missing-BPM block, repeated missing blocks, a 100 -> 120 ramp,
  and stopped/resumed transport with BPM still available. Test feedback 0 and
  0.3. Assert the tempo-selection contract and finite output; log peak and
  maximum adjacent-sample step across the complete stream.
- **State:** save with internal TEMPO 90 / host 100; restore before and after
  prepare, and over a used instance with different settings. Check restored
  parameters and echoes under current host 120 or no BPM. Also restore manual
  sync and free-time delay. Host BPM is reacquired, not saved as internal TEMPO.
  These tests inspect the first callback's tempo/finiteness and later echo;
  they do not certify that the first audible note survives a DAW's recall.

The continuous stream's PASS means **tempo selection, finite output and
settled timing**. The transient metrics are diagnostic, with no click,
pitch, echo-count or subjective quality threshold. They must not be reported
as artifact-free playback. The current EchoModule moves its read delay over
35 ms; pitch changes and re-reading/skipping old buffer content are possible.
Use the CI results/logs for the exact tested HEAD, not the original PR's local
PASS, when reporting these new cases. This revision changes tests/docs only.

## Expected boundary behavior (not new measurement results)

Internal TEMPO is 90; DELAY SYNC and Follow host tempo are on for DSP timing.
Auto Set's column assumes eligible sparse Clean/Rock guitar and both the delay
and DELAY SYNC initially off.

| Host input | Auto Set may select sync | Effective DSP BPM | Quarter-note echo |
|---|---|---|---|
| 39.9 | No | 40 (clamped) | 1500 ms |
| 40 | Yes | 40 | 1500 ms |
| 100 | Yes | 100 | 600 ms |
| 120 | Yes | 120 | 500 ms |
| 240 | Yes | 240 | 250 ms |
| 240.1 | No | 240 (clamped) | 250 ms |
| Missing BPM / position / playhead | No | 90 (internal) | 666.667 ms |
| NaN, infinity, zero, negative BPM | No | 90 (internal) | 666.667 ms |
| Stopped transport, valid BPM provided | BPM remains usable | Provided BPM | Corresponding interval |

The out-of-range policy difference is intentionally preserved: Auto Set rejects
it; an already-following processor clamps positive finite BPM to 40-240.
`reportedHostTempo()` and `currentHostTempo()` are zero outside that range even
when DSP uses a clamped tempo. A fallback can still be labelled HOST by the UI
because that label follows the switch; this test/docs change does not fix it.
Changing the DSP to fallback for 240.1 BPM would change existing audio from
250 ms to 666.667 ms with TEMPO 90 and requires a separate behavior decision.

## Studio One manual measurement checklist

**Status: not run by this automated suite.** Record Studio One version,
plugin format/build SHA, sample rate, device buffer and Dropout Protection.
Use a copy of the DEADWIRE 100 BPM Song; manual delay sync is independent of
Auto Set's refusal to add a new delay for Metal or bass.

1. Use 48 kHz, initially 256 samples. Disable audio-event timestretch and use
   seconds-based event positions so tempo edits do not move/stretch the probe.
2. Set Precision Delay (including no expansion-model override), feedback 0,
   mix 50%, DELAY SYNC on. Set internal TEMPO 90 before enabling HOST SYNC.
   Disable reverb, modulation and other processing that obscures transients.
3. Use a 10 ms / 1 kHz burst around -18 dBFS, separated by at least 3 s. In one
   uninterrupted playback, hold each Tempo Track segment at least 6 s:
   100 -> 120 -> 100 -> 40 -> 39.9 -> 40 -> 240 -> 240.1 -> 240 -> 100.
   Confirm these are steps, not unintended ramps. The 100/120 sections check
   recovery in this dynamic sequence, not a repeat of isolated fixed tests.
4. For settled intervals, place bursts at least 100 ms after each change.
   Measure direct-to-echo separation in the same output: samples / 48 = ms.
   Expected intervals are in the table; +/-96 samples equals +/-2 ms. Do not
   apply this steady-state criterion to a note already in the moving delay.
5. In a separate take, leave a sustained 1 kHz signal or actual guitar DI
   ringing across each 100 <-> 120 change. Test feedback 0 then 0.3. Preserve
   at least 1 s on either side; inspect/listen for clicks, pitch movement,
   duplicated/missing echoes and level spikes. Record timing and listening
   outcomes separately. Also test a deliberate gradual tempo ramp.
6. Capture explicit real-time export and offline export of the same sequence;
   do not substitute Automatic for explicit real-time processing. Compare
   with normal playback. Repeat the dynamic section at 64/1024 samples if the
   baseline works, noting the actual host-processing buffer settings.
7. Save, close Studio One completely, reopen and record the first playback as
   well as settled playback. Confirm switches, internal TEMPO 90, and 600 ms
   at host 100. Reuse the saved plugin state under host 120: expect 500 ms,
   not a frozen 600 ms. Check stop/resume, seeking to a different tempo, and a
   loop crossing 100/120. A cleared old delay tail is expected on state reset.
8. With HOST SYNC off and DELAY SYNC on, save/reopen: internal 90 yields
   666.667 ms. With DELAY SYNC off, the saved manual delay time must prevail.

Studio One's Stop button or turning HOST SYNC off does **not** inject absent
BPM, absent PositionInfo or NaN. Mark forced host-information-loss cases as
not validated in Studio One unless an instrumented host adapter actually
creates and logs them; the synthetic regression covers those inputs.

## Remaining validation boundaries

Real Studio One/VST3 playback and project lifecycle, first-note behavior,
actual DI and listening remain unvalidated here. The synthetic transport flag
is not a real DAW stop/resume or loop test. No claim is made about sample-exact
intra-block tempo changes, delay musical-phase locking through a ramp, other
sample rates, or other delay models. Auto Set chooses a quarter-note base
time; multi-tap/modulated models need their own output expectations.
