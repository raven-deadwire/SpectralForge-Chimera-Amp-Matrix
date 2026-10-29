# Multiband stereo link and M/S — 1.5

Status: **implemented on `claude/multiband-stereo`** (based on `main`).
Roadmap: 1.5 "separately gated M/S". This is the stereo half of the Pro-MB
audit gap in `docs/MULTIBAND_PRO_MB_AUDIT_20261020.md`, which lists external
sidechain, the free detector band and M/S as not implemented. External
sidechain, free detector bands and six bands remain open. Covered by
`ChimeraMultibandDynamicsCoreTests`.

## Controls

The new parameters append after every released parameter, at version hint 18.

| Parameter | Range | Default |
|---|---|---|
| `mbStereoMode` | Left / Right, Mid / Side | Left / Right |
| `mbLink` | 0–100 % | 100 % |
| `mbBandN_channels` (N = 1–4) | Both, Mid / Left, Side / Right | Both |

- **Mid/Side:**
  - The stage splits, detects and synthesises M = (L + R) / 2 and S = (L − R) / 2.
  - It returns L = M + S and R = M − S.
- **Link:**
  - Each channel's detector level is blended towards the louder channel's in amplitude: `link · max + (1 − link) · own`.
  - At 100 % both channels follow the louder detector: the original linked behaviour.
  - At 0 % each channel (or M and S) follows its own.
  - The amplitude blend is a Chimera choice; Pro-MB's law is not published.
- **Band channels:** a band set to one channel leaves the other at unity gain, without makeup, so that channel passes unchanged.
- **UI:** the multiband page's top bar has `L/R`/`M/S` and a `% link` bar. Each band has its channel choice beside the mode. The labels read Mid/Side or Left/Right to match the current mode.

## Defaults are the original processing

- Detectors, gain states and the minimum-phase synthesis schedule are now per channel.
- While the two channels' band gains are equal, one schedule is computed and copied to the other. With the defaults, that is always the case.
- A one-off comparison compiled the `main` core and this core side by side and fed both the same stereo noise and tones. It covered Peak and RMS detection, all four modes with makeup, 60 % parallel mix, an on/off toggle, and ragged blocks. All 4 × 144,000 samples were bit-identical.

## Changing the stereo mode

A change while running works in three steps:

1. The stage fades to its input over 10 ms.
2. It swaps the domain and feeds the new domain silently for 30 ms, so its filters settle.
3. It fades back in over 10 ms.

There is no discontinuity: the output's largest second difference across the change is 0.0134386, against 0.0134383 for the input itself. A version that swapped at once measured 0.115, and the test fails on it.

The cost is that for about 50 ms the band processing is absent, so a heavily compressed signal briefly rises to its unprocessed level. When the stage is off, the mode changes at once.

## Measurements (`ChimeraMultibandDynamicsCoreTests`)

Band 3 is set to downward compression (−40 dB threshold, 4:1); the other bands are bypassed.

- **Link**, with a −12 dBFS 1 kHz tone on the left and a −48 dBFS tone on the right:
  - Left is reduced by 10.25 dB.
  - Right is reduced by 10.25 dB at 100 %, 8.42 dB at 50 % and 0 dB at 0 %.
- **M/S**, with a 1 kHz mid tone and a 1.5 kHz side tone, both at −12 dBFS:
  - A band set to Mid reduces the mid tone by 10.6 dB. The side tone is unchanged (1e−7 dB).
  - A band set to Side reduces the side tone by 14.5 dB. The mid tone is unchanged (7e−8 dB).
- **M/S at unity:**
  - The output equals the input within 2.8e−17.
  - It is not bit-exact. Rounding occurs where one channel is orders of magnitude below the other, so their sum does not fit a double.
- **Mode change:** see above.
- **No allocation** in the callback.

## Not done

- **External sidechain:** needs a sidechain input bus and host layouts, which is a separate gate.
- Free detector bands, six bands, and band output pan.
- A crossfade between both domains during a mode change, which would avoid the 50 ms without processing.
- Listening tests.
