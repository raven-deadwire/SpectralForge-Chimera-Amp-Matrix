# Ravenforge cabinet room update preview

This preview separates the original cabinet editor from the captured IR loader and adds a shared Ravenforge room for Dual and Matrix rigs. The source combines the captured microphone catalog and original spatial CAB work from PRs #29–31 through the locally reviewed integration checkpoint `e74e286828730737efde3eaa75a50dafdb13bfb7`.

## User-visible behavior

- Dual opens two rigs in the same room; Matrix shows LOW, MID and HIGH. Clicking a cabinet opens that rig's enlarged cabinet editor; ROOM returns to the overview. Classic opens its single cabinet directly.
- The RIGS page defaults to the room in Dual/Matrix. RIG CONTROLS opens the existing amplifier controls; CABINET ROOM returns. The room is created lazily so opening the default PRE page does not decode additional cabinet artwork.
- The room uses the project's Ravenforge workbench and emblem language: blackened wood, dark metal, aged bronze, restrained amber lighting and a winged raven relief. Equipment is rendered separately and follows the actual selected amp and cabinet source.
- The CABINET view places the selected amp head above a large Chimera Guitar 4x12 or Bass 4x10 cabinet, with four matching 12-inch or 10-inch driver images. Mic A/B sit in front of the selected physical speaker.
- Drag an active original microphone to choose its physical unit and centre-to-edge position. Shift-drag changes grille distance. Existing unit, position and distance controls remain available and host automation updates the scene. Each drag uses paired host gestures; leaving the scene closes them.
- CABINET and IR LOADER are presentation tabs. Opening either tab, selecting a room rig, returning to the room or switching to amp controls preserves all audio parameters, retained IRs and A/B state.
- IR LOADER keeps the captured-IR cards and their existing cabinet metadata and catalog microphone identity. Unknown/mixed microphones retain a neutral IR indicator. A stored capture is visually dimmed while that slot uses an original response. Original Mic A/B explicitly choose their original response or the retained capture; they are not mute buttons.
- The focused panel is 1040 by 748 logical pixels; the visual contract also exercises 900 by 748. The room workspace adds a 44-pixel navigation bar and uniformly fits both focused views into the space available when the operating system constrains the popup. Cabinet selection, rear/tweeter controls and blend remain visible in the tested 1000 by 657 and 1000 by 696 windows. JUCE applies the same transform to drawing and pointer coordinates. Existing control IDs, source values, parameters, independent slots and shared IR state remain.
- Twenty-seven equipment PNGs and one separate room background are embedded in the application: two cabinets, two drivers, three original-response microphones and twenty captured-microphone identities. They require no external image download or loose image directory after installation.

Artwork is presentation only. The twenty captured microphone identities remain distinct from the three implemented original responses. These images do not add capture data or change the original v1 acoustic definitions. Chimera Strike remains its own catalog identity; Detail Condenser is not Strike.

## Resource lifetime

A UI-owned shared image bank decodes and downsamples the embedded PNGs during creation. It retains cabinets at most 640 pixels, drivers 384 and microphones 320. A separate shared scenery bank bounds the room image within 1152 by 576 while preserving aspect ratio; the combined equipment/scenery RGBA estimate stays below 16 MiB. Drawing and audio processing do not open image files or decode PNGs. Last-owner destruction releases the banks before host graphics teardown.

## Update package

The preview retains product version 1.2.0 and records a distinct source-derived preview build identity. It carries the existing bounded Defender intelligence retry from `a11003f7ba924d220717ed34149fedb2ec25c401`. Exhausted updates still stop delivery before malware scan/transfer, and the actual installer verification and scan remain required.

The installer and portable artifact transports retain 20 MiB parts and exact byte/source hashes. Their bounded part capacity is expanded to eight to accommodate the embedded illustrations. Metadata preserves each part's size and SHA-256 and the full archive/Setup digest.

## Verification and remaining scope

The CAB tests verify all embedded resources, independent image changes, host automation, neutral unknown/mixed IR displays, physical geometry, mouse/Shift dragging, gesture lifetime, room rig counts, navigation without host notifications, resource destruction and project recall. Existing state, original-CAB audio, 64 MiB serialized-state budget, legacy migration and callback CPU gates remain unchanged. Public CI screenshots use synthetic test IRs only. The room is a navigation view, not a new room-acoustics engine.

Native popup checks include the actual monitor-constrained window bounds and every visible essential control. A separate 1000 by 657 workspace exercises microphone hit targets and dragging through the panel transform, cabinet/IR tabs, room return and unchanged serialized state. This covers the clipping observed in the initial native screenshots rather than relying only on full-size offscreen panels.

Native Linux UI tests require a functioning display and window manager. Both the CAB workflow and the full build workflow run their unchanged CTest selections through `Tools/run_linux_ui_tests.py`, which starts Xvfb and Openbox, verifies window-manager readiness and preserves the test command's exit status. The native fixture fails explicitly when no display or native window handle exists. Bare Xvfb does not provide the window-manager protocols expected by this JUCE version; software-only screenshots do not establish native dialog acceptance.

The [initial room-source CAB run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37799408951) at `6b19464ea9de7dc527a62a8e8f9d133d96cd4812` passed all eight Windows tests and the native CAB/UI tests on macOS. Its macOS Original CAB integration gate failed at 96 kHz / 64-frame stereo: wall p99 781.167 us and thread-CPU p99 785.25 us against a 666.667 us block, with 32/1600 deadline misses. Benchmark and CAB DSP sources were unchanged from `8d95f0704d0f5f30c7b720a0674fc4cc1ac25e20`, and this isolated target does not link the new artwork/editor. Those facts do not establish the cause of the timing difference or waive the failure. The Linux desktop correction does not change the CPU gate; later source runs retain their own independent results.

Actual instrument-DI listening, full-plugin CPU on the target PC and DAW acceptance remain separate from a verified development installer. Existing local CPU failures from the parent implementation are retained in `docs/CAB_INTEGRATION.md`; they are not converted into a pass by the visual change. `release_approved=false`.

Asset prompts and generation provenance are recorded beside the PNGs in `Assets/Artwork/Cab`. Exact-source workflow results and final package hashes are recorded on the visual-update draft PR and in the delivered verification records.
