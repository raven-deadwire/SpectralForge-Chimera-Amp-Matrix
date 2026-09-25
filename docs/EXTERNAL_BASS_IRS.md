# External bass IR collection

The IR Library includes metadata and original-file hashes for the **Shift Line Bass IR Pack**. Its twelve WAV files are external downloads, not bundled assets. Missing files are marked **EXTERNAL / DOWNLOAD**, cannot be loaded, and do not increase the available-file count.

## Download and load

1. Open **IR LIBRARY → GET BASS IRS**. This opens the creator's official page: https://shift-line.com/irpackbass
2. Download the original Bass IR Pack from that page and extract its ZIP.
3. In Chimera, choose **ADD FOLDER** and select the extracted folder.
4. The library checks the original filenames and SHA-256 hashes. Matching rows become **INSTALLED**. Choose an IR and **LOAD INTO RIG**, or select it from the rig cabinet menu.

Chimera does not download files in the background or upload user IRs. Its Project IR slot embeds the selected WAV in the user's local preset/project; the four existing cabinet-source automation values remain unchanged. Follow the creator's terms when sharing presets containing third-party IRs.

| Compact name | Cabinet / capture type |
|---|---|
| EBS ProLine 4x10 | Bass cabinet |
| Hartke XL 4x10 | Bass cabinet, aluminium drivers |
| TC BC410 | Bass cabinet, ribbon blend |
| Ampeg Heritage B15 | Bass cabinet, ribbon microphone |
| Sunn 200s 2x15 | Bass cabinet |
| Mesa RR215 | Bass cabinet |
| Ampeg SVT-810E | Bass cabinet |
| EVM12L + DI | Speaker capture blended with low-frequency DI |
| Orange PPC212 + DI | Guitar cabinet adapted for bass; DI blend |
| V30 + DI | Speaker capture blended with low-frequency DI |
| Sunn 200s — Olympic II | Alternate bass-cabinet voicing |
| Ampeg SVT-810E — Olympic II | Alternate bass-cabinet voicing |

The 12 original WAVs were retrieved directly from the creator-linked archive for format/hash verification on 2026-09-25: https://shiftline-shared.s3.amazonaws.com/Shift_Line_Bass_IR_Pack.zip . All are mono PCM, 24-bit, 48 kHz, 1000 frames. This research copy is outside the repository and release artifacts. Filenames, full hashes and capture metadata are in `Source/IRReferenceCatalog.h`, `externalBassIRCatalog`. The pack contains ten cabinet/speaker choices (including three DI blends) and two alternate voicings; do not advertise twelve independently measured physical cabinets.

## Distribution boundary

The creator's official Terms of Use and archive Readme_EN.txt require permission for commercial use and software/hardware implementation. No such permission has been obtained for Chimera. The public release therefore contains metadata and an official source link only. The source page controls the download and applicable terms. References to products and microphones describe the creator's published capture information; no endorsement is implied.

The two embedded factory guitar IRs remain the separately licensed jesterdyne CC BY 4.0 files documented in THIRD_PARTY_NOTICES.md. The earlier thirteen-file personal collection remains a separate user-supplied add-on and is not included in public installers.
