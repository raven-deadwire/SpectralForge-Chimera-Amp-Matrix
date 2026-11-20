# Complete amplifier head artwork

`head-shell-00.png` through `head-shell-25.png` are AI-created **production source
art**, generated with the built-in ImageGen tool. They are not native UI captures
or evidence of host operation. The set covers all 26 stable catalog indices,
including the legacy, recall-only Iron Compact at index 16.

This full-object redraw supersedes the composition at
`9995669df7a0058497f214d7380534cd80ebadcb` in
[Draft PR #44](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/44).
That composition passed automated checks but was rejected in visual review:
upper corner volumes were missing and some bass heads had unnatural rear
extensions. The new artwork incorporates the roof, side walls, corners, original
handle arrangement and feet into one complete enclosure.

Each production PNG is a byte-identical copy of its final generated output, with
no post-generation cropping, resizing, repainting or alpha processing. Some final
generations used earlier generated variants as ImageGen edit references. Live
knobs, readable labels and channel state remain JUCE-rendered controls over the
blank control plates. Náströnd and Niflheimr retain their authored branding,
ornament and channel-window hardware.

The renderer applies **one uniform transform** to the complete PNG. It must not
split the roof from the front, stretch individual surfaces or synthesize extra
handles, feet or rear panels. `Source/AmpHeadShellMetadata.h` is authoritative for
the control coordinate frame, support baseline and channel-window landmarks;
the control frame is not a measurement of the physical front surface.

`provenance.json` records each model's identity reference, final output basename,
SHA-256, byte length, dimensions and measured alpha bounds. It preserves 22 final
prompts verbatim. Instructions for indices 6, 17, 24 and 25 are explicitly marked
as normalized specifications, not verbatim prompts. Requested dimensions in a
prompt are generation targets; measured output dimensions are recorded separately.

Source-art review alone does not establish native UI or Studio One acceptance.
Actual JUCE screenshots must be presented separately with their exact source
commit and CI run, including platform and capture context.

## Reference heads 26-30 (1.6)

`head-shell-26.png` through `head-shell-30.png` are **procedural** drawings by
`Tools/generate_reference_head_shells.py`, not AI-generated or photographic art.
- They show original Chimera fascias carrying the working names, with no manufacturer logo or trade dress.
- The script also writes their provenance entries and prints the metadata rows and reviewed plate fixtures.
- Replacing them with photographic art keeps the same rules: one complete RGBA shell per model and a blank control plate.
