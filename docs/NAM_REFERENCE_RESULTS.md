# NAM reference results

Eight fixed amp-head captures were rendered by the official NAM Core at 48 kHz, A2 full size, using the same deterministic synthetic DI as Chimera. The core commit is `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`. Raw NAM weights and T3K IRs are not included.

Three broad output filters (100 Hz shelf, 500 Hz peak, 2 kHz shelf) were fitted on the single-note fixture. They are part of amp voicing in all modes, independent of the user EQ. A fixed 6.02 dB output trim keeps headroom. A different chord/fundamental/amplitude fixture was held out and then rendered through the compiled C++ implementation.

| Chimera voice | NAM reference | Single-note error, before -> after | Held-out chord error, before -> after |
|---|---|---|---|
| Glass | Fender Twin Reverb 65 RI | 17.84 -> 9.66 dB | 15.58 -> 7.88 dB |
| Brit Edge | Marshall JTM45 RI / Volume5 LOCUT Presence5 | 13.97 -> 6.52 dB | 13.54 -> 4.76 dB |
| Tight 515 | Peavey6505 1992 Lead / 4,7,3,7,1,5,8 / Suhr Reactive Load | 16.86 -> 6.05 dB | 15.92 -> 5.54 dB |
| Wide Rect | Mesa Dual Rectifier MW Red Modern / Gain2:00 MV2 P3 T6 M4 B3 | 13.10 -> 5.90 dB | 16.29 -> 6.35 dB |
| Liquid Lead | Mesa MarkIV Lead / Petrucci1 | 14.55 -> 6.60 dB | 14.85 -> 5.83 dB |
| Iron Tube | Ampeg SVT-VR Classic Normal | 2.05 -> 1.54 dB | 3.74 -> 2.89 dB |
| Solid Punch | GK RB800 Gain3 / EQ clock 5,2,11,2 | 7.50 -> 3.49 dB | 9.08 -> 4.71 dB |
| Modern Bass | B7K Ultra + Aguilar DB751 (not isolated B7K) | 9.85 -> 5.02 dB | 11.83 -> 6.93 dB |

The metric is RMS log-power-spectrum difference across 65 Hz-8 kHz Welch bins above -50 dB relative to the reference spectral peak, after whole-render RMS matching. It is not NAM training ESR, a loudness score, or a hardware-fidelity pass. All eight held-out spectral scores improved; remaining nonlinear dynamics, phase, transient and level differences are substantial for several voices. The broad fit is specific to these capture settings and synthetic input levels, not evidence across every hardware knob position.

Input calibration uses each file's input_level_dbu relative to a common 11.5 dBu digital-source convention. Missing metadata leaves unity digital gain and is marked unknown. File metadata takes precedence over rounded website descriptions (e.g. Fender 18.18 vs page 18.1; JTM45 12.2418 vs page 12.4). There is no measured calibration for the user's audio interface, so absolute physical input equivalence is not established. Output is RMS matched; polarity/phase differences are retained. Correlation lags are constrained to +/-128 samples and are diagnostic, not plugin latency or delay calibration.

The modern bass reference is B7K Ultra **plus Aguilar DB751**, not an isolated pedal or Darkglass head. The Iron Tube reference lacks input calibration metadata. Some capture control details are incomplete. These limitations are retained in the source records rather than filled with guessed settings.

Post-change A/B renders use the same V30/SM57 IR for both sides of each guitar comparison and the same Ampeg 8x10/MD421 IR for both sides of each bass comparison. Separate A-NAM and B-Chimera-matched WAVs avoid conflating the plugin's A/B snapshots with stereo channel assignments. Leading IR delay is retained. NAM audio output redistribution is permitted by the displayed T3K terms; the data files are excluded.

Reproduce with Python numpy/scipy, the supplied Chimera Render CLI, a locally built official NAM render executable, and the exact user-downloaded files listed in reference/manifest.json:

```sh
python Tools/validate_nam.py --models /path/to/your/nam-files --manifest docs/reference/manifest.json --nam-render /path/to/NAM/render --chimera-render /path/to/ChimeraRender --out /path/to/results --ir /path/to/guitar.wav --bass-ir /path/to/bass.wav
```

Tests use generated DI, not a human performance. Guitar/bass listening and calibrated real playing dynamics remain acceptance work; this build must not be advertised as a verified hardware replica. Exact source hashes and per-case measurements are in reference/nam-before.json and nam-after.json.
