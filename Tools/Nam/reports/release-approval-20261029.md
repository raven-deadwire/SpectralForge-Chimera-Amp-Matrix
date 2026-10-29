# Náströnd A2 TONE3000 release approval, 2026-10-29

The owner reported listening acceptance complete for all five channels and
approved the release. `approve_release.py` recorded it:

- It recomputes release profile v3 from the package's own validation and
  test reports and refuses approval unless every channel passes both.
  All five pass ([fenrir-quiet-pass-20261029](fenrir-quiet-pass-20261029.md)).
- It changes metadata only. Each `.nam` gets status `RELEASE_APPROVED` and
  the approval record. Its audio through the player engine is checked to be
  bit-identical to the approved model's, and each preset is rebuilt with the
  player's writer and checked to restore the same audio.
- After approval the package passed the same verification again: hashes, A2-Full
  gate, held-out test metrics unchanged, preset restore exact.

The approval is under v3, the A2 release profile. The reference-fidelity
profile v2 is still not met (high-gain full ESR 0.018 to 0.022 against 0.01),
and the package README says so.

Release zip SHA-256: `ea96c4aec88baa0b23f6a7af00c14d1efe46bbb79e5a3fcef621c7f713afd957`

Model and preset hashes:

```
cb9ca1e0746b9def5dcff1a1380d581e5b2600efc26a23f69660a2c50fc15138  NAM/Nastrond-Fenrir.nam
95a429cb093ea8121d7d88ee49932bab97253298b00726627d674cd058862c33  NAM/Nastrond-Fimbulvetr.nam
d4c8ff2b4deb69cbf0362b1cecca8f8b27b6adbebdb72c1c6d776449198f1b0c  NAM/Nastrond-Nidhoggr.nam
3257e1b409c041ed3adc049271762666163fade80ded21ae975d09f51003bb13  NAM/Nastrond-Ragnarok.nam
6429bb514b478ca04adfbf4d35c989aee55d8f436837914d0f31a6f027f1d7e3  NAM/Nastrond-Surtr.nam
a852edf378d19337458c10b368d0c98f123c1a952531e78b90826f45f16b65e0  TONE3000-Presets/Nastrond-Fenrir.t3kpreset
66e34a1cacd94f9f3389cab38e9be14856759480ccc99bb5b807a1b30d5ee3c4  TONE3000-Presets/Nastrond-Fimbulvetr.t3kpreset
ef19a5cb3b7f81b72c4503bb762cec35ebddab3df4af697776a263131d366ef2  TONE3000-Presets/Nastrond-Nidhoggr.t3kpreset
6f870c93966ddbb8f20fb1822c1b20f5f931ae3032991c0e39814dd5839ed9b6  TONE3000-Presets/Nastrond-Ragnarok.t3kpreset
bc81434bbaff847b0a3a7c9da8156ea075a026206b86dca9ec81cff6cb5531e4  TONE3000-Presets/Nastrond-Surtr.t3kpreset
```

The listening acceptance is recorded as the owner stated it; the method and
material of the listening were not specified.
