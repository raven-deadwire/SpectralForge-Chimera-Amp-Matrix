# IR distribution for Open Beta 1.3

The public IR library starts with **two factory guitar IRs**. The original CAB
engine supplies the new guitar and bass cabinet designs without distributing
third-party captures. Additional WAV/AIFF files appear when the user imports
them or selects a personal library folder.

## Included factory recordings

Both source pages currently identify jesterdyne as the author and show
**Creative Commons Attribution 4.0 International**. Attribution, original
titles, source links and playback processing are retained in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
License: https://creativecommons.org/licenses/by/4.0/

| Stored source | File | Original source |
|---|---|---|
| 1 | guitar_v30_sm57.wav | https://freesound.org/people/jesterdyne/sounds/116735/ |
| 2 | guitar_jensen_sm57.wav | https://freesound.org/people/jesterdyne/sounds/116743/ |

The two embedded WAVs are unchanged from the reviewed source bytes:

- V30: 43,385 bytes; SHA-256 `ef8d258eee57b2f0fa1e678b4bdf9c8f535f03039172b0831dcc87b0e7d99bd2`.
- Jensen: 38,601 bytes; SHA-256 `894805dc8b60bd7e2b49dac582bc27a39c8d1f191fc110eb4d4145c0b63937bb`.

These records identify the included files and their published licensing;
they do not establish permissions for other recordings or assets.

## Excluded release material

The previous development catalog contained 26 third-party capture references,
12 external bass downloads and four private-import references. Product
redistribution approval for these collections is not recorded, so their
prepopulated entries and download prompts are omitted from this release.
Their recording files are not included.

Public packages also exclude research catalog directories, personal capture
packs, NAM weights and loose reference audio. Packaging checks the embedded IR
allowlist and rejects unexpected audio or capture archives in the staged
payload. The Windows publisher repeats the portable-payload check.

## Personal libraries and saved projects

Use **IR LIBRARY → OPEN IR** or **ADD FOLDER** to select your own WAV/AIFF files.
Imported files and user-authored metadata remain available. A factory preset
does not search for a named private capture or automatically replace its new
original CAB recipe with a local IR.

Existing source values remain 0 = Filters only, 1 = V30, 2 = Jensen and
3 = User IR. Saved projects, A/B states and presets retain their stored IR audio
and metadata. Release preparation does not delete files from personal libraries.
