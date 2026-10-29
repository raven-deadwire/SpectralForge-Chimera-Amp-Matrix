# External sidechain and SYNTH output bus

Status: **implemented on `claude/sidechain-synth-out`**, stacked on
`claude/synth-three-osc`. Covered by `ChimeraSidechainTests` and the updated
inventory tests.

## Owner's decision (2026-10-29)

The owner chose "external sidechain + a synth-only output":
- host buses for an external key into the compressor, the gates and the
  multiband;
- a separate output for the SYNTH;
- an update of the plugin bus contract.

## Buses

| Bus | Direction | Channels | Default |
|---|---|---|---|
| Input / Output | main | mono or stereo, input = output (unchanged) | on |
| **Sidechain** | input | off, mono or stereo | **off** |
| **Synth** | output | off or stereo | **off** |

- **Defaults:** both new buses start disabled, so hosts and sessions that do
  not ask for them see the plugin as before. The layout tests accept 12
  combinations and reject a mono SYNTH bus and an asymmetric main bus.
- **Format notes:** VST3 and AU expose them as an auxiliary input and an
  auxiliary output. In AAX they appear only where the host offers sidechain
  and multi-output instances.
  - This build checks the processor-side layout contract only.
  - Hosts and formats have not been tried. The Windows AAX build and the
    Pro Tools / Logic / Reaper / Cubase checks are still to do.
- **Channel sharing:** input and output buses share the callback's channels,
  so the sidechain arrives in the same channels the SYNTH bus is written to.
  - The processor copies each chunk's key into its own buffer before the SYNTH
    bus is written.
  - The test feeds a key on those shared channels with the synth off: the
    SYNTH bus stays exactly silent.

## Keys

Version hint 28 adds four parameters, appended after the SYNTH bank:
`synthOutput`, `scKeyComp`, `scKeyGates` and `scKeyMultiband`.

| Switch | Detectors that listen to the sidechain |
|---|---|
| Compressor page, EXT KEY (`scKeyComp`) | The clean compressor. Its detector high-pass still applies |
| Gate / Expander page, EXT KEY (`scKeyGates`) | The Gate before or after the rig, Gate 2 and the Expander |
| Multiband page, EXT KEY (`scKeyMultiband`) | Every band. The key goes through its own copy of the crossovers, in the same L/R or M/S domain |
| POST Console VCA, EXTERNAL SIDE-CHAIN | Console VCA's detector. This released switch was marked "unavailable" and is now connected |

- **Console VCA automation:** the EXTERNAL SIDE-CHAIN switch keeps its
  released "not automatable" flag (`ChimeraGateProcessorTests` checks the
  released contract). It is set from the panel or the session.
- **Gain path:** only the detector changes. The gain still applies to the
  signal at the stage's usual place.
- **Fallback:** with no sidechain connected, every switch falls back to the
  detector's own input. Both "switch on, no sidechain" and "sidechain
  connected, switch off" leave the output identical, to 0.01 dB.
- **Mono key:** a mono key feeds both detector channels.
- **Timing:** the key is used as it arrives. It is not delayed by the rig's
  latency. Stages after the rig (compressor, multiband, Console VCA) therefore
  see the key early by the plugin latency (42 samples at the defaults,
  0.9 ms) — a small look-ahead. The post-rig gates keep their existing
  alignment of the gain to the rig output.

## SYNTH output

`synthOutput` sits on the SYNTH page above the scope, which shows whether the
host has the bus on.

| Choice | Guitar mix | SYNTH bus |
|---|---|---|
| Main mix (default) | The synth at its insert point | Silent |
| Synth bus | No synth | The synth |
| Main + synth bus | The synth at its insert point | The synth |

- **Bus level:** the bus carries the synth after its own level and safety
  stage. It does not pass through the rig, POST, Final EQ, the output gain
  or the limiter.
- **No bus:** with the SYNTH bus off in the host, "Synth bus" keeps the synth
  in the mix, so nothing goes silent unexpectedly.
- **Switching:** a change of choice crossfades over 20 ms.
- **Shared behaviour with the main output:** the recall fade, the beta-expiry
  mute and bypass (silence on the bus) also apply to the SYNTH bus.

## Measurements (`ChimeraSidechainTests`, 48 kHz)

- **Clean compressor:**
  - Setup: threshold 0 dBFS, ratio 20, main −30 dBFS, key +12 dBFS.
  - The output falls from −26.20 to −34.87 dBFS (−8.67 dB).
  - With the switch alone, or the sidechain alone: −26.20 dBFS, unchanged.
- **Gates** (main −30 dBFS):

  | Gate | Open | Silent key | −10 dBFS key |
  |---|---|---|---|
  | Before the rig | −26.2 | −79.4 | −26.2 dBFS |
  | After the rig | −26.2 | −120.2 | −26.2 dBFS |

  - The Expander after the rig (threshold −40 dBFS, range 24 dB) goes from
    −26.2 to −50.0 dBFS with a silent key.
  - Before the rig, the amp's compression hides most of a 24 dB pre-rig cut,
    so the test measures the Expander after the rig.
- **Multiband:**
  - Every band at threshold −10 dBFS, ratio 20. Main has 220 Hz and 1.5 kHz
    at −30 dBFS each; the key is +12 dBFS at 1.5 kHz.
  - 1.5 kHz drops by 14.1 dB; 220 Hz changes by 0.07 dB.
- **Console VCA:**
  - A −40 dBFS signal under its threshold, with a 0 dBFS key.
  - EXTERNAL SIDE-CHAIN gives −11.5 dB re the internal detector.
  - Switched on with no key: 0.00 dB.
- **SYNTH output** (silent guitar, MIDI A3):

  | Choice | Main | Bus |
  |---|---|---|
  | Main mix | −20.9 | silent |
  | Synth bus | silent | −14.9 dBFS |
  | Main + synth bus | −20.9 | −14.9 dBFS |

  - The bus is 6 dB above the mix because it skips the default −6 dB output
    gain.
  - No SYNTH bus with "Synth bus" selected: main −20.9 dBFS (fallback).
  - The bypassed bus is silent.
- **Callback:** every key switch on and both buses on, with a key and MIDI —
  0 allocations, finite output. A mono sidechain keys the gate the same as a
  stereo one (−79.4 dBFS).

## Not done

- **Hosts:** checks in real hosts, including AAX sidechain in Pro Tools,
  Logic's sidechain menu and multi-output routing.
- **Pedals:** keyed detectors for the PRE pedal compressor and the POST
  dynamics models other than Console VCA.
- **Key handling:** a key filter or monitor, and a key level meter.
- **Latency:** delaying the key by the plugin latency for exact alignment
  after the rig.
