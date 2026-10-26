# Room Core 1.3.3 — design freeze draft

Status: **design draft for the 2026-12-04 review, with a working first
implementation** (roadmap weeks 1–2,
[DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md#133-room-core--november-30december-25-2026),
issue #46). The send bus, gate policy, parameters and room model are
implemented on `claude/room-core` and covered by `ChimeraRoomCoreTests`
(§7). The controls open from a ROOM entry in the cabinet room's title band
(a call-out, so the calibrated rig layout keeps its bounds); the EN/DE/KR
manual describes them. Numbers marked *proposed* are defaults to confirm
at the review; numbers marked *measured* come from that test or the model
probes. Source baseline: main `2105a90`.

## 1. Scope

One shared room environment: Size (Small / Medium / Large), Absorption,
Room on/off, a per-rig Room Send and a defined wet-return level. Actual early
reflections and late decay, state/automation/A-B, old projects open with the
room off. Controls live on the RIGS/CAB room surface in Classic, Dual and
Matrix. Independent early level, decay, diffusion, wet filters and width are
1.8 work and stay out.

## 2. Where the room is excited and returned

Current order (`ChimeraProcessor::process`, `Engine::process`):

```
INPUT → PRE gate/compressor/pedals → Engine: split → per lane: AMP → CAB → level·polarity·mute/solo·Dual blend → Σ
      → post-rig Gate → Gate 2 (after rig) → Expander (after rig) → Tone EQ → Compressor → Multiband → De-esser → POST → Utilities → Final EQ → trim/limiter
```

Room Core inserts one send bus and one return:

```
per lane i:   direct_i ──────────────────────────────┐
              direct_i × send_i ─→ room bus          Σ → post-rig gates ─→ (+) ─→ Tone EQ …
                                   room bus → same post-rig gate gains → Room → wet × return ─┘
```

**Excitation tap.** In `Engine::process` each lane's `work[i]` already carries
its AMP/CAB output after `levels[i]` (level, polarity, mute/solo) and the Dual
blend weight, immediately before `buffer.addFrom`. The tap reads that same
signal, so lane gain, mute, solo, polarity and Dual blend apply to the room
exactly as to the direct sound.

**Matrix LOW.** `work[0]` is a mix of the aligned DI and the AMP/CAB branch,
`dry·(1−mix) + ampcab·mix`. The tap takes only `ampcab·mix` (scaled by the
same lane gain). DI 100 % (`lowAmpMix` 0) and LOW AMP off (`mix` → 0) feed
nothing into the room. This is a structural zero, testable exactly.

**Gate and tail policy.** Pre-rig gating already shapes the excitation. For
every post-rig attenuation stage that is active (the post-rig Gate when
`gateAfterRig`, Gate 2 and the Expander when placed after the rig), the room
bus receives the *same per-sample gain* as the direct sum. `PostRigGate` and
`ExtraGate` already apply a precomputed per-sample envelope; a second-buffer
`apply(direct, roomBus)` uses it without a second detector. The wet return is
added **after** these stages and before the Tone EQ tap. So a closed gate
creates no new room input, and a tail already in the room rings out instead
of being cut by the gate.

**Return level.** Calibrated on white noise (steady-state energy is the
impulse response's energy): with one lane at send 100 % and return 0 dB, at
50 % absorption the early reflections and the late tail each carry −12.0 dB
of the direct energy (total wet −9.0 dB) for all three sizes. Absorption 0 %
gives about −4.4 dB total and 100 % about −18 dB. The uncalibrated image
sources had put the early block at +1.7 dB re direct. The return trim then
sets the balance.

**Latency.** The room adds no latency and no lookahead; it is a parallel
return. Reported latency and the DI alignment stay as they are.

## 3. Room off and transitions

- Room on/off is the shared bypass. **Off** fades the excitation out over
  20 ms; the tail keeps playing (send-effect behaviour) until its output stays
  below −120 dBFS for 50 ms, then the room stops processing and costs nothing.
  A hard send stop would replay its edge at every reflection tap (measured
  23× the room's own edge level without the ramp, §7).
- **Room off since prepare** (every old project, and the default) runs no
  room code and makes no extra copies: the engine tap is skipped, so the
  direct path is bit-identical to the current product. This is the first
  acceptance test (§7).
- On → the excitation fades in over 20 ms from an empty room, at the set
  return level (fading the return instead lets the later reflections of the
  hard send start through: 8.3× the room's own edge level).
- Size change: the send moves from the old size to the new one over 50 ms
  and the old size's output fades over 250 ms while the new one builds up;
  this needs two room instances only during the change. Crossfading only the
  two returns (the first implementation) cut the old room's input and
  started the new one's hard, which replayed at every reflection tap: 19–38×
  the room's own edge level. Sends and return are ramped per sample
  over 20 ms; absorption is smoothed over 20 ms and its reflection, line and
  damping gains move linearly across each block (stepping them per block was
  a zipper: 8.1× the room's own edge level on a 50 → 0 % jump).
- Mode change (Classic/Dual/Matrix) keeps the room running; lanes that stop
  existing simply stop sending. The engine resets on a mode change, so the
  direct sound itself switches with an edge (measured 70–175× its steady
  edge level on a held 41.2 Hz tone, with or without the room); the room
  repeats that edge. Smoothing the routing change is engine work outside
  Room Core.

## 4. Processing model (week 2)

Stereo, per size a declared virtual room so the reflections follow from
geometry rather than hand-tuned taps:

| Size | Declared room (L × W × H, m) | Early-reflection window | Late RT60 at 50 % absorption |
|---|---|---|---|
| Small | 3.5 × 3.0 × 2.5 | 0–20.4 ms (measured) | 0.35 s |
| Medium | 7 × 5 × 3 | 0–40.7 ms (measured) | 0.7 s |
| Large | 14 × 10 × 6 | 0–81.4 ms (measured) | 1.4 s |

(*proposed*; dimensions and RT60 values are design targets, checked by the
measurements in §7.)

- **Early reflections:** first- and second-order image sources of the declared
  room for a fixed source/listener placement, as a stereo tapped delay with
  per-tap gain from distance and absorption (12–18 taps per side).
- **Late decay:** an 8-line feedback delay network (Householder mixing,
  mutually prime lengths scaled by size), per-line gain set from RT60, and a
  one-pole low-pass per line whose cutoff falls as Absorption rises (damping).
  Absorption also scales RT60 between 0.4× (100 %) and 1.6× (0 %). The damping
  filter shortens the broadband decay too (measured 0.85–0.94 × target), so the
  line gains aim 1.12 × longer; measured RT60 is then 0.94–1.04 × target for
  every size and absorption at 44.1–192 kHz.
- No modulation or diffusion controls in 1.3.3 (1.8).

## 5. Parameters (*proposed*, version hint 15)

Appended after `appendExtraGateParameters` (hint 14). Old projects and A/B
slots lack them and restore these defaults, so the room is off.

| ID | Type | Range | Default | Automatable |
|---|---|---|---|---|
| `roomOn` | bool | off/on | off | yes |
| `roomSize` | choice | Small / Medium / Large | Medium | yes (crossfaded) |
| `roomAbsorption` | float | 0–100 % | 50 % | yes |
| `roomReturn` | float | −36 to +6 dB | 0 dB | yes |
| `roomSend1` … `roomSend3` | float | 0–100 % | 50 % | yes |

Factory presets keep their authored sound: the preset snapshot writes
`roomOn` off unless a preset is deliberately re-authored with a room.

## 6. Budgets (*proposed*, to be measured)

- **CPU:** budget ≤ 3 % of real time at 48 kHz stereo, 128-sample blocks
  (HYBRID transposer: ~8 %). *Measured* for one room model: 0.26–0.28 % at
  48 kHz and 0.52–0.53 % at 96 kHz for all three sizes (best of three 10 s
  runs on the development container); two run only during a size change.
  Room off runs no room code (zero processed samples, §7).
  *Measured* in the whole product (`ChimeraRoomCost`: default settings, all
  sends 100 %, a retriggered chord; off and on alternate every block, median
  of the per-run differences): the room adds 0.52–1.05 % of real time at
  48 kHz (product 15–37 %) and 1.31–2.06 % at 96 kHz (product 35–90 %) across
  Classic, Dual and Matrix and all sizes. The excess over the model alone is
  the send bus (per-sample sends, the post-rig gain copy and the return).
  Run-to-run spread is 0.3–0.8 % at 48 kHz on the development container, so
  the sizes are not separable at this resolution.
- **Memory:** allocated in `prepare` only. Large at 192 kHz: FDN 8 lines up to
  ~120 ms each plus a 90 ms early-reflection line, stereo, ×2 instances for the
  size crossfade: about 2 × (8 × 0.12 + 2 × 0.09) × 192000 × 4 B ≈ 1.8 MB.
- **Callback:** no allocation, locks or FFT in the audio thread (same rule as
  the dynamics capture); verified with the operator-new/malloc counter used by
  `StabilityTests`.

## 7. Acceptance tests (`ChimeraRoomCoreTests`)

Status: all items below pass on Linux (48 kHz unless stated). Not done:
opening the call-out in a native session, automation and recall in an
installed DAW, the Windows test installer and listening.

- Parameters append last at version 15 with the room off.
- Room off is bit-identical whatever the other room settings, with zero room
  samples processed.
- Matrix LOW: DI 100 % and LOW AMP off send exactly 0; the AMP share excites.
- Mute and send 0 send exactly 0; Dual blend 0.5 is −6.02 dB like the direct
  sound.
- Post-rig gate and expander: the room send changes by exactly the direct
  sound's amount (−15.2178 / −19.0332 dB, against a room-off twin).
- Model at 44.1–192 kHz: RT60 within 10 % of target (measured 0.94–1.04 ×),
  24 reflections per ear inside 20.4 / 40.7 / 81.4 ms, more absorption gives
  a darker late tail, the tail decays, total wet −9.0 dB at 50 % absorption.
- The host tail (`getTailLengthSeconds`) grows by the room's while it is on:
  two RT60s with 10 % margin, plus 150 ms (Medium, 50 %: 1.69 s); Room off
  reports none.
- Room off rings out within that reported tail (Medium: 1.25 s), then the
  room idles and the output is bit-identical to a processor without a room.
- Room on/off, send 0↔100 %, return 0↔−36 dB, absorption 0 / 100 / 50 % and
  size Small / Large / Medium on a held 41.2 Hz tone: the wet part's second
  difference (its edges) stays within twice its steady value (measured
  ≤ 1.47×). Without the 20 ms excitation ramp Room on reaches 8.3× and Room
  off 23×; per-block absorption gains 8.1×; a return-only size crossfade
  19–38×.
- Mode change Matrix → Classic: the room keeps processing and its tail; the
  lanes Classic does not have send nothing.
- No allocation in the callback with the room on in Classic, Dual and Matrix.
- Controls: the ROOM entry does not overlap a rig and shows ROOM OFF or the
  active size; the panel shows 1/2/3 sends for Classic/Dual/Matrix and each
  control moves only its own parameter; opening the room or the panel changes
  no parameter.
- State: room settings survive project save/restore; a project without room
  parameters opens with the room off and default settings, also into a
  processor whose room was on; A/B slots keep their own room; a factory preset
  turns the room off.

Original plan:

1. **Room off is bit-identical:** with `roomOn` off since prepare, processor
   output equals the output with every room parameter at any other value, and
   the room reports zero processed samples.
2. **LOW DI isolation:** Matrix with `lowAmpMix` 0 (or LOW AMP off): the LOW
   lane's room excitation is exactly zero; MID/HIGH still excite.
3. **Lane controls apply:** mute/solo/polarity/level/Dual blend change the
   excitation exactly as the direct contribution.
4. **Gate policy:** with the post-rig gate closed, room excitation is zero
   while an existing tail keeps decaying; Gate 2 / Expander after rig behave
   the same.
5. **Room behaviour:** impulse response per size shows reflections inside the
   declared window and an RT60 within ±20 % of §4; Absorption lowers RT60 and
   high-frequency decay monotonically.
6. **Transitions and state:** on/off, size changes and sends are click-free
   (no step above the tail's own envelope); state, A/B and automation recall;
   old projects restore with the room off.
7. **Real-time:** no allocation in the callback; CPU and memory within §6.

## 8. Open questions for the review

- Default send (50 %) and return (0 dB) once the calibration in §2 is measured.
- Whether `roomSize` should be automatable at all, or structural like the
  Dynamic EQ target assignment.
- Whether the tail should also be cut when the transport stops (host reset)
  or only on Room off ring-out.
