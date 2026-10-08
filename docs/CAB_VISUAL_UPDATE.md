# CAB visual update preview

This preview adds cabinet, speaker-unit and microphone illustrations to the integrated CAB panel. The source combines the captured microphone catalog and original spatial CAB work from PRs #29–31 through the locally reviewed integration checkpoint `e74e286828730737efde3eaa75a50dafdb13bfb7`.

## User-visible behavior

- The original section shows the current Chimera Guitar 4x12 or Chimera Bass 4x10 cabinet, its corresponding 12-inch or 10-inch driver, and independent Mic A/B images.
- Changing the cabinet or either microphone through its control or host automation updates the corresponding image.
- Captured IRs use their existing cabinet metadata illustration and their catalog microphone identity. Unknown/mixed microphones retain a neutral IR indicator. A stored capture is visually dimmed while that slot uses an original response.
- The new panel is 1040 by 748 logical pixels; the visual contract also exercises 900 by 748. Existing control IDs, source values, parameters, independent slots and shared IR state remain.
- Twenty-seven PNG illustrations are embedded in the application: two cabinets, two drivers, three original-response microphones and twenty captured-microphone identities. They require no external image download or loose image directory after installation.

Artwork is presentation only. The twenty captured microphone identities remain distinct from the three implemented original responses. These images do not add capture data or change the original v1 acoustic definitions. Chimera Strike remains its own catalog identity; Detail Condenser is not Strike.

## Resource lifetime

A UI-owned shared image bank decodes and downsamples the embedded PNGs during creation. It retains cabinets at most 640 pixels, drivers 384 and microphones 320; the aggregate retained RGBA estimate must be at most 16 MiB. Drawing and audio processing do not open image files or decode PNGs. Last-owner destruction releases the bank before host graphics teardown.

## Update package

The preview retains product version 1.2.0 and records a distinct source-derived preview build identity. It carries the existing bounded Defender intelligence retry from `a11003f7ba924d220717ed34149fedb2ec25c401`. Exhausted updates still stop delivery before malware scan/transfer, and the actual installer verification and scan remain required.

The installer and portable artifact transports retain 20 MiB parts and exact byte/source hashes. Their bounded part capacity is expanded to eight to accommodate the embedded illustrations. Metadata preserves each part's size and SHA-256 and the full archive/Setup digest.

## Verification and remaining scope

The CAB tests verify all embedded resources, independent image changes, host automation, neutral unknown/mixed IR displays, geometry/layout, resource destruction and project recall. Existing state, original-CAB audio, 64 MiB serialized-state budget, legacy migration and callback CPU gates remain unchanged. Public CI screenshots use synthetic test IRs only.

Actual instrument-DI listening, full-plugin CPU on the target PC and DAW acceptance remain separate from a verified development installer. Existing local CPU failures from the parent implementation are retained in `docs/CAB_INTEGRATION.md`; they are not converted into a pass by the visual change. `release_approved=false`.

Asset prompts and generation provenance are recorded beside the PNGs in `Assets/Artwork/Cab`. Exact-source workflow results and final package hashes are recorded on the visual-update draft PR and in the delivered verification records.
