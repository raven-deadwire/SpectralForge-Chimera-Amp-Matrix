# Room Core 1.3.3 — design freeze draft

Status: **design draft for the 2026-12-04 review** (roadmap week 1,
[DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md#133-room-core--november-30december-25-2026),
issue #46). Nothing here is implemented or measured yet. Numbers marked
*proposed* are budgets and defaults to confirm or change at the review; they
are not results. Source baseline: main `2105a90`.

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

**Return level.** The wet return is calibrated so that a lane at send 100 %
and return 0 dB produces a room whose long-term (late) energy is −12 dB
relative to the direct sound for pink noise (*proposed*, measured with the
test in §7); the user return trim then sets the balance.

**Latency.** The room adds no latency and no lookahead; it is a parallel
return. Reported latency and the DI alignment stay as they are.

## 3. Room off and transitions

- Room on/off is the shared bypass. **Off** stops new excitation immediately;
  the tail keeps playing (send-effect behaviour) until its output stays below
  −120 dBFS for 50 ms, then the room stops processing and costs nothing.
- **Room off since prepare** (every old project, and the default) runs no
  room code and makes no extra copies: the engine tap is skipped, so the
  direct path is bit-identical to the current product. This is the first
  acceptance test (§7).
- On → the return fades in over 20 ms (*proposed*) from an empty room.
- Size change: the old size keeps decaying while the new size builds up
  (crossfade the two returns over 50 ms, *proposed*); this needs two room
  instances only during the change. Absorption and sends are smoothed per
  sample, return per block with a 20 ms ramp.
- Mode change (Classic/Dual/Matrix) keeps the room running; lanes that stop
  existing simply stop sending.

## 4. Processing model (week 2)

Stereo, per size a declared virtual room so the reflections follow from
geometry rather than hand-tuned taps:

| Size | Declared room (L × W × H, m) | Early-reflection window | Late RT60 at 50 % absorption |
|---|---|---|---|
| Small | 3.5 × 3.0 × 2.5 | 0–25 ms | 0.35 s |
| Medium | 7 × 5 × 3 | 0–45 ms | 0.7 s |
| Large | 14 × 10 × 6 | 0–80 ms | 1.4 s |

(*proposed*; dimensions and RT60 values are design targets, checked by the
measurements in §7.)

- **Early reflections:** first- and second-order image sources of the declared
  room for a fixed source/listener placement, as a stereo tapped delay with
  per-tap gain from distance and absorption (12–18 taps per side).
- **Late decay:** an 8-line feedback delay network (Householder mixing,
  mutually prime lengths scaled by size), per-line gain set from RT60, and a
  one-pole low-pass per line whose cutoff falls as Absorption rises (damping).
  Absorption also scales RT60 between 0.4× (100 %) and 1.6× (0 %).
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

- **CPU:** room on, 48 kHz stereo, 128-sample blocks: ≤ 3 % of real time on the
  container used by `StabilityTests` (HYBRID transposer: ~8 %); room off: no
  measurable change. Measured with the same per-block method as the transpose
  cost test.
- **Memory:** allocated in `prepare` only. Large at 192 kHz: FDN 8 lines up to
  ~120 ms each plus a 90 ms early-reflection line, stereo, ×2 instances for the
  size crossfade: about 2 × (8 × 0.12 + 2 × 0.09) × 192000 × 4 B ≈ 1.8 MB.
- **Callback:** no allocation, locks or FFT in the audio thread (same rule as
  the dynamics capture); verified with the operator-new/malloc counter used by
  `StabilityTests`.

## 7. Acceptance tests for the design (to be written with the code)

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
