# 1.9 expanded modulation and routing polish

Status: **implemented on `claude/modulation-1-9`**, stacked on
`claude/drag-order-ux`. It covers the roadmap's 1.9 "expanded modulation and
routing polish". The 1.7 two-filter modulation
([FILTER_MODULATION_1_7.md](FILTER_MODULATION_1_7.md)) deferred these items
to 1.9:
- a second LFO;
- LFO phase locked to the host bar, and a retrigger;
- an envelope-follower sidechain;
- filter drive and further filter types.

Covered by `ChimeraModulationTests` and the inventory tests.

## Filter modulation

Ten parameters are appended at version hint 30. They are automatable, and
their defaults keep the 1.7 sound sample for sample (tested).

| Parameter | Range | Default | What it does |
|---|---|---|---|
| `fltLfo2Shape` | Sine, Triangle, Square, Saw down, Random | Triangle | Second LFO, shared by both filters |
| `fltLfo2Rate` | 0.05–20 Hz | 0.25 Hz | Free rate |
| `fltLfo2Sync` | Free, 1/1 … 1/8 D | Free | Tempo sync, as LFO 1 |
| `fltA_lfo2`, `fltB_lfo2` | ±4 octaves | 0 | Each filter's LFO 2 amount |
| `fltLfoLock` | off / on | off | Synced LFOs follow the host's bar position while the transport plays |
| `fltLfoRetrigger` | off / on | off | Both LFOs restart on each attack (when not locked) |
| `fltEnvSource` | Own signal, Sidechain | Own signal | The envelope follower's input |
| `fltA_drive`, `fltB_drive` | 0–100 % | 0 % | Soft saturation ahead of each filter |

- **Random shape** (on both LFOs): a new value each cycle, through the same
  2 ms smoothing as the square.
- **Cutoff:** `cutoff × 2^(LFO amount × LFO 1 + LFO 2 amount × LFO 2 + envelope amount × env)`.
- **Bar lock:** a synced LFO's phase is the host position (in quarter notes)
  divided by its cycle length, so it lands on the same beat on every pass.
  - Free-rate LFOs, a stopped transport and a host without a position keep
    free-running.
  - The processor passes the position at each chunk's start; samples within
    a chunk advance from it at the host's tempo (not Chimera's tempo setting,
    which may differ when "Follow host tempo" is off).
  - The phase counts from song position 0, not from the last bar line, so in
    3/4 a 1/1 cycle (four quarter notes) does not restart on each bar.
- **Retrigger:** an attack restarts both LFOs, and Random draws new values.
  An LFO locked to the bar is not restarted; the other one still is.
  - The detector is a fast follower (1 ms attack / 30 ms release) rising 4 dB
    above a slow one (30 / 300 ms), from above −40 dBFS. It re-arms once the
    fast follower settles within 1.2 dB of the slow one.
  - It follows the envelope's source (own signal or sidechain) and is
    independent of the envelope's attack and release settings.
- **Sidechain envelope:** the follower reads the plugin's Sidechain input
  ([SIDECHAIN_SYNTH_BUS.md](SIDECHAIN_SYNTH_BUS.md)), channel by channel. A
  mono key feeds both channels. Without a connected sidechain it uses the
  signal itself.
- **Drive:** `tanh(d·x) / tanh(d)` with `d = 10 × drive`, which is unity at
  full scale and approaches the bypass continuously (0 % is exact bypass).
  Quiet signals gain up to `d / tanh(d)` (+20 dB at 100 %). Drive is
  smoothed over 20 ms.
- **Type changes:** a filter's state-variable stages keep following the input
  while it is a Comb, and the Comb's delay keeps the recent input otherwise,
  so changing the type does not replay stale audio.

### Filter types

| Type | Behaviour |
|---|---|
| Low-pass, High-pass, Band-pass, Notch | As in 1.7 |
| **Comb** (new) | A feedback comb tuned to the cutoff: peaks at the cutoff's multiples. Resonance sets the feedback (up to 0.97), and the peaks stay at unity gain. Slope does not apply. The delay is allocated in `prepare()` (up to 1/10 s). |
| **All-pass** (new) | Flat magnitude with a phase turn at the cutoff: a phaser notch when mixed with the dry signal (Mix 50 %). 24 dB/oct uses two stages. |

## Routing polish

The placement menu of a movable block (see
[SIGNAL_ORDER_1_4.md](SIGNAL_ORDER_1_4.md)) gains **Order presets**: named
orders for its region. The current one is ticked. Each preset is one order
command, so it is one undo entry.

| Region | Presets |
|---|---|
| Tone / dynamics | Released order; Dynamics before Tone EQ; Colour last (saturation and filter after the dynamics); De-ess first |
| POST | Released order; POST EQ before dynamics; Modulation after the delay and reverb; Reverb into delay |

## Page

- **Modulation view:** the FILTER page's Filter A / Filter B selector gains a
  third view, **Modulation**. It holds LFO 2 shape and sync, LOCK TO BAR,
  RETRIGGER and the envelope source, in the side column the filter units use.
- **Bottom row:** LFO 2 rate joins it.
- **Filter units:** each unit adds LFO 2 amount and drive; Mix / Output move
  down one row.
- **Spectrum:** keeps its height. A first layout took a row from the spectrum,
  and the common-dynamics UI test (graph at least 150 px) failed it.

## Measurements (`ChimeraModulationTests`, 48 kHz)

| Check | Result |
|---|---|
| Defaults | The 1.9 fields at defaults (and LFO 2 at amount 0) give the 1.7 output sample for sample |
| LFO 2 sine 4 Hz, 2 octaves | 7 level cycles in 1.8 s (7.2 expected) |
| Random 8 Hz | The level changes by up to 30.5 dB between cycles. Largest second difference 0.019 (smoothed steps) |
| Bar lock, 1/4 at 120 BPM | At beat 10.25 the phase follows the position exactly (to 1e-9), also after 40 blocks; unlocked it free-runs from 0 |
| Retrigger, bursts every 370 ms at 3 Hz | Phase 20 ms into each burst spans 0.008 with retrigger, 0.55 without |
| Sidechain envelope | With a loud key, 2 kHz passes 30.8 dB more through LP 250 Hz + 3 oct. With Sidechain chosen but no key: identical to own signal |
| Drive | 220 Hz at −6 dBFS: harmonic 3 at −174 dB (0 %), −10.7 dB (100 %) |
| Comb 500 Hz, 80 % | 1 kHz peak 18.0 dB above the 750 Hz trough; peak gain 0.000 dB |
| All-pass 1 kHz | Magnitude flat to 3e-7 dB (100 Hz–6 kHz); at Mix 50 %, 1 kHz −134 dB, 4 kHz −0.55 dB |
| Order presets | Applied as one undo step; re-applying the current preset changes nothing; every preset is a valid order |
| Allocation | None while processing |

Found while testing:
- The first retrigger detector used the envelope follower. With its 150 ms
  release it never re-armed between notes 370 ms apart. The detector is now
  separate, with a 4 dB rise.

## Not done

- **Review notes (10/30):**
  - Random ignores the LFO stereo phase: both channels step to the same value.
  - With Sidechain chosen, a host that enables the sidechain bus but routes
    nothing to it gives a silent envelope (the plugin cannot tell an unrouted
    bus from a silent key).
  - Filter types and LFO shapes were added to the 1.7 choices. 1.7 is not
    released, so no automation depends on the old 0–1 mapping.

- **Modulation:** per-voice or per-band modulation, and a general modulation
  matrix for the common processors.
- **Envelope sidechain:** a band-limited key.
- **Placements:** the "advanced whole-wet compressor placement" of the signal
  editor specification (a placement after POST).
- **Checks:** listening tests, CPU in a DAW, and bar lock in hosts (tested
  here with synthetic positions).
