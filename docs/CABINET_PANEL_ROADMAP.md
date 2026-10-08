# Cabinet control panel roadmap

Status: **PLANNED**, added at the owner's request on 2026-10-08 KST.
Delivery target: **1.3.x CAB update**, alongside the Niflheimr cycle.
This document specifies future work; it does not describe an implemented panel
or an acquired factory IR collection.

## Product decision

Replace the single-file IR loader as the main cabinet-editing surface with a
dedicated **CAB** panel: choose a cabinet, place two microphones, blend their
signals, and manage user IRs. Reuse and extend the existing convolution backend.

The current published baseline is
[Open Beta 1.2](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1),
published on 2026-10-05. The 1.1.1, 1.1.2 and 1.2 releases are historical completed
releases; their remaining documented limitations are follow-up work. At planning
time main is 15427d5717d30f566de27bd3dcc0eafa459a5563. Niflheimr and improved
personal IR management are in [PR #24](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/24),
then at bd617bde00c26ffee3f741862058c909b6b4835c; they are not assumed merged.

CAB is a shared feature, separate from the AMP/PRE/POST/SYNTH model-count
contract. The existing 1.3 Niflheimr / 1.4–1.10 expansion sequence and final
132-model target continue. Cabinet types, microphone types, IR positions and
presets are tracked separately and do not inflate the 132 count.

## Target schedule and work ownership

Dates are internal planning targets in Asia/Seoul, not promised release dates.
Read the latest branch, release and evidence before each work session; advance
completed work rather than waiting for an obsolete start date. Fixes and
Niflheimr work already in progress continue alongside CAB.

| Target window | Non-Astra development | Astra / evidence review | Completion result |
|---|---|---|---|
| 2026-10-08–10-11 | Panel/parameter specification, cabinet/mic inventory, asset manifest and acquisition feasibility | Official reference boundaries, coordinate/gain/timing requirements | Reviewed specification and per-asset missing-input list |
| 2026-10-12–10-25 | Two-slot backend, worker/cache design, old-state migration, CAB page and USER IR view | Gain, stereo, phase and LOW DI design; interpolation prototype | Internal two-mic/IR build; synthetic fixtures are labelled as fixtures |
| 2026-10-26–11-08 | Connect qualified measured grids; cabinet/mic browser, metadata and factory starting presets | Grid-node accuracy, motion continuity, same-DI comparison and callback cost | CAB alpha using only available, qualified assets |
| 2026-11-09–12-04 | Integrate with the current Niflheimr line, package verified factory assets and update EN/DE/KR manuals | Guitar/bass listening, target-host lifecycle, full state/A-B and CPU/latency acceptance | 1.3.x candidate with an explicit shipped asset inventory |
| Before 1.4 PRE Expansion A, or later if a required input is missing | Complete the accepted CAB scope or name the remaining delivery dependency | Preserve unverified items as pending | Publish only through the existing release process |

The development automation remains daily at 15:00 KST; the Astra automation
remains weekdays at 17:00 KST. This CAB workstream participates in both queues.
A missing grid, redistribution permission or real-DI recording blocks that
specific asset/acceptance result; it does not halt independent UI, migration,
backend or fixture work. Do not reclassify synthetic fixtures as factory captures.

The content target below is 12 cabinets and 8 microphone types. Stage the
working build with qualified combinations first. Record the actual shipped
count separately; a target row is never evidence of an installed audio asset.
At the first checkpoint, identify a viable supplier/creator or existing licensed
grid for each target and revise content dates if none is available.

## CAB panel scope

| Area | First delivery |
|---|---|
| Navigation | RIGS / CAB / PRE / POST. A compact CAB summary on each rig opens that lane directly. Global utilities remain available. |
| Lane context | Classic RIG; Dual A/B; Matrix LOW/MID/HIGH. Preserve a cabinet per lane before merge. |
| Cabinet selection | Guitar / Bass / User IR categories, with cross-instrument selection allowed. Identify the actual enclosure and speaker separately. |
| Mic A and Mic B | Independent mic or custom IR, enable, solo/mute, level, polarity and pan/balance. Two-mic blending works with a mono input too. |
| Position / Distance | Dragging and numerical controls reference measured coordinates. Expose only supported coordinates/ranges for the selected capture bank. |
| Blend / output | Constant-sum A/B blend, per-mic trim and cabinet output. Identical unity signals should not gain 6 dB at the midpoint. B off retains A's unity output. |
| Phase tools | Per-mic polarity and explicit fine delay; any automatic alignment is an optional, documented operation. |
| Cabinet filtering | Preserve lane low/high cuts and bypass. Preserve existing filter placement for legacy states. |
| Amp association | Offer matching cabinet suggestions plus Cabinet Lock/Unlink; an amp change must not silently replace an explicitly locked selection. |
| USER IR | OPEN IR, ADD FOLDER, drag/drop, Guitar/Bass/Unspecified assignment and correction, persistent REMOVE FROM LIST. |
| State | DAW project, .chimera, A/B, automation and MIDI retain all lane, slot, asset and control identities. |
| Presentation | Original Chimera artwork; a concise selected-reference caption follows the existing product naming policy, without a REFERENCE: prefix. |

USER IR keeps the original file and embedded project audio when removed from
the list, following PR #24. Do not restore GET BASS IRS or ADD PERSONAL ZIP to
the primary workflow. Unknown imports remain Unspecified.

A single external WAV/AIFF does not provide a measured position/distance grid.
Keep its physical mic controls inactive; allow level, pan/balance, polarity,
delay, cuts and blend. A metadata label such as SM57 does not create other mic
responses. Previously embedded V30/SM57 and Jensen/SM57 captures retain their original
fixed audio and identities for legacy recall. New multi-position banks receive
separate identities.

Room, angle, rear-mic, Sub Kick, and continuously adjustable tweeter/port layers
are follow-up scope. A bass cabinet's actual captured horn setting must still
be recorded at first delivery. A separate Tweeter/Port control needs suitable
independent captures and an explicit phase/gain contract.

## Factory cabinet development target: 7 guitar + 5 bass

All rows below are **capture/asset targets**, not hardware-clone certification.
The roles and starting combinations are Chimera design choices to audition.
Manufacturer sources establish the named hardware specifications, not the
sound or redistribution rights of an as-yet unacquired Chimera capture.

Working descriptors are inventory labels, not finalized product names.
The 57/421/906/121/160/87/20/112 shorthand maps to the microphone table below.

### Guitar

| ID | Working descriptor | Physical reference / capture target | Intended coverage | Initial mic set |
|---|---|---|---|---|
| G01 | Modern V30 412 | MESA Rectifier Standard OS Straight 4×12, rear-mounted Celestion V30, closed back [G1] | Main modern metal, death/groove and low-tuned Náströnd comparison | 57, 421, 121, 160 |
| G02 | British 75 412 | Marshall 1960A 4×12, Celestion G12T-75 [G2] | A distinct classic/modern metal rhythm and power-metal lead alternative | 57, 906, 121 |
| G03 | Greenback 412 | Marshall 1960AX 4×12, Celestion G12M-25 Greenback [G3] | Mid-focused crunch and lead; existing lower-gain amps | 57, 121, 87 |
| G04 | Dense V30 412 | Orange PPC412 4×12 V30, closed back [G4] | Orange Heavy, sludge/doom and dense rhythm comparisons | 57, 421, 160 |
| G05 | American Open 212 | Fender '65 Twin Reverb cabinet section, 2×12 Jensen C12K, open back [G5] | Clean, ambient and modulation-friendly coverage | 57, 906, 87 |
| G06 | Karnivore 412 | Documented custom 4×12 with Eminence Karnivore drivers [G6] | Requested high-gain, down-tuned metal option | 57, 421, 160 |
| G07 | Raven 412 | Documented custom 4×12 with Celestion G12-100 Raven drivers [G7] | Requested modern high-gain alternative, compared directly with V30/Karnivore | 57, 906, 121 |

G06/G07 require the exact enclosure, speaker complement and revision to be
documented; the speaker manufacturer pages alone establish neither a cabinet
model nor a completed capture. The earlier Karnivore and Raven private metadata
remains useful for research and user import, not automatic factory inclusion.
G12-100 Raven must not be replaced by G12K-100 or another similarly named driver.

G01 is the Standard OS target; the existing private Mesa Traditional 4FB capture
is a different enclosure. G05 is specifically C12K; it does not rename a vintage
Bassman C12N/C12NA capture. Earlier Framus/G12M and Bassman/Jensen requests remain
research/import candidates with their own identities. An ENGL V30 multi-position
bank may extend the collection after comparison; retain the existing embedded
V30 source independently. Do not count every V30 cabinet or mic position as a
newly demonstrated sonic role.

### Bass

| ID | Working descriptor | Physical reference / capture target | Intended coverage | Initial mic set |
|---|---|---|---|---|
| B01 | Classic 810 | Ampeg SVT-810E 8×10, separated sealed chambers [B1] | Classic bass foundation, midrange definition and driven Garmr/Hel comparisons | 20, 421, 112 |
| B02 | Modern 410 | Darkglass 410 / DG410NE, Eminence neo drivers and P-Audio horn [B2] | Modern extended-range articulation; Hrímfaxi/Nidavellir | 20, 421, 57 |
| B03 | Reference 212 | Glockenklang Double, 2×12 plus horn [B3] | Clean reference, complex lines and DI blend evaluation | 20, 121, 87 |
| B04 | Hybrid 410 | Hartke HyDrive HD410, paper/aluminium hybrid drivers and HF unit [B4] | A different attack/upper-mid texture from B02; industrial and picked bass | 20, 421, 57 |
| B05 | Power 215 | MESA Standard PowerHouse 2×15, front Tri-Port and horn [B5] | Large 15-inch cabinet option for Ymir/Hel and full-range bass comparisons | 20, 112, 121 |

B03's preferred comparison candidate is Vanderkley 212MNT. Keep it a distinct
asset if selected later; do not silently relabel Glockenklang data. Double and
Double Art also have different HF units. B04 is HD410, not the older HX410.

SUNN 200S/215S-family 2×15 is a priority expansion candidate for Hel after exact
enclosure, era and driver identity are resolved. Existing metadata for a 1968
SUNN 2×15 does not resolve every physical detail. Keep SUNN distinct from MESA
B05 and from later Fender-era SUNN models. Existing Shift Line, Traynor,
Bassman, Hartke PRO 2200 and personal bass imports retain their identities.

The 2×15 format alone does not establish a dark tone, slow transient or greater
low-frequency extension. Those are capture- and listening-dependent results.
Likewise, woofer-only close miking does not prove the response of a whole
horn/port-equipped bass enclosure. Inspect those contributions explicitly.

## Shared microphone pool: 8 types

A mic type is selectable only for cabinet/position combinations present in the
qualified asset manifest. Eight types does not promise a 12×8 Cartesian library.

| Descriptor | Physical microphone reference | Capture state to record | Planned role |
|---|---|---|---|
| Dynamic 57 | Shure SM57, cardioid dynamic [M1] | Exact orientation and distance | Default guitar attack and upper-mid definition |
| Dynamic 421 | Sennheiser MD421-II, cardioid dynamic [M2] | Five-position bass switch; M/full-range starting capture | Guitar/bass body and lower-mid support |
| Dynamic 906 | Sennheiser e906, supercardioid dynamic [M3] | Presence switch; Flat starting capture | Alternative guitar presence and rhythm texture |
| Ribbon 121 | Royer R-121, figure-8 ribbon [M4] | Front/rear side, distance and preamp gain | Body and smoother high-end balance with a dynamic mic |
| Ribbon 160 | beyerdynamic M160, hypercardioid double ribbon [M5] | Exact orientation and distance | Different ribbon contour for high-gain/lead |
| Condenser 87 | Neumann U87 Ai, large-diaphragm condenser [M6] | Cardioid starting capture; pad/filter settings | Broad-band clean guitar and bass reference |
| Dynamic 20 | Electro-Voice RE20, cardioid dynamic with Variable-D [M7] | Bass-tilt switch; flat starting capture | Bass pitch/low-mid reference with restrained proximity effect |
| Dynamic 112 | AKG D112 MkII, cardioid dynamic [M8] | Exact orientation and distance | Bass low-end weight and attack contrast |

D6, M201TG, i5, V7 X and U67 are later mic candidates; retaining an imported
capture using one does not make a full factory grid available.

Starting recipes to audition: guitar 57+121, 57+421, 57+160; bass 20+421 and
20+112. Fix input and playback levels before choosing blend percentages. These
are starting combinations, not claims of an artist's actual studio settings.

## Data and backend requirements

### Asset acquisition and grid

Prefer existing creator/supplier measurements with explicit permission for
plugin embedding and redistribution, or separately authorized measurements.
Do not make owner-operated hardware reamping a prerequisite. Ordinary NAM
anchors, commercial listening references and downloaded private IRs remain
separate from the redistributable CAB bank.

Each bank needs exact cabinet/speaker/mic identity, mic switch/pattern, speaker
selection, radial coordinate, distance reference plane, angle, horn/port state,
sample rate, length, onset/phase processing, level calibration, original file
hash, processed file hash, creator and redistribution evidence.

As a sizing example, the initial mic sets above contain 37 cabinet/mic pairs.
Five radial positions × three measured distances would require 555 IRs before
extra angle/horn/room variants. This is a workload estimate, not an asset count.
Use the actual supplier grid and validate its resolution; do not invent evenly
spaced coordinates for unrelated files. Continuous controls may interpolate
within qualified cells; node values must reproduce the measured response.
No extrapolation beyond supported bounds.

The source manifests currently describe two embedded factory IRs; no complete
licensed bass mic grid is established. Preserve fixed factory assets and user
imports while new banks are acquired. A free download or verified account is
not by itself plugin-redistribution permission.

### Engine and timing

At the reviewed source, Source/Cabinet.h already prepares JUCE convolution
kernels on a worker and crossfades swaps. Extend that ownership/lifecycle model.
Preserve lane amp → cabinet → merge routing.

- Prepare/decode/resample/interpolate/cache outside the audio callback.
- Coalesce rapid mic moves; discard stale generations and bound cache/work queues.
- Reclaim kernels off the audio thread, including close/releaseResources.
- Benchmark up to six stereo mic paths in three-lane Matrix mode, plus overlapping
  old/new kernels during changes; this is a path-count bound, not a CPU multiplier.
- Keep algorithmic latency separate from captured onset and user mic delay.
  Preserve Matrix LOW DI alignment and never silently trim old IR timing.
- Current legacy loading uses Trim::no and Normalise::yes. Preserve that sound.
  New measured banks need a consistent capture-level policy; independent
  per-position normalization must not erase meaningful distance/mic differences.
- Specify mono pan versus stereo balance. A centered old stereo IR must retain
  its existing gain and channel separation.

### State migration

Keep the four existing cabinet-source automation values and parameter ordinals.
Append new identities; do not expand an old choice range in a way that remaps
normalized host automation. Migrate each old cabinet to A only, B disabled,
extra mic delay zero, unity trim and unchanged audio/filters. Preserve existing
lane polarity at its current stage; initialize new per-mic polarity to normal
without duplicating the lane inversion.

The current processor rejects states above 64 MiB and each imported IR is
limited to 4 MiB. Naively serializing six maximum-sized files in the active
state and both A/B snapshots can approach 96 MiB after base64. Design a shared,
hash-addressed embedded asset table with per-slot references; verify both
reused and genuinely distinct large IRs against an explicit total-size budget.
Enforce the total serialized-state budget, including asset encoding and metadata,
before accepting an import or state operation that adds assets. If it would exceed
the supported limit, reject that new operation explicitly and preserve the previous
recoverable state; never truncate or discard embedded assets, or write a state the
reader will reject. Deduplication alone does not bound genuinely different A/B
asset sets. Do not simply raise the parser cap without evaluating project size and
recall.

## Completion evidence

| Gate | Required result |
|---|---|
| Assets | Actual audio, identity, coordinate and gain/timing manifest, hashes and distribution evidence for every shipped combination |
| Legacy recall | Existing factory/User IR projects and .chimera files retain sound, A/B and automation; imports survive original-file moves |
| Grid and blend | Measured-node accuracy; continuous movement without clicks, unintended notches or level jumps; documented blend/headroom behavior |
| Routing | Classic, both Dual modes and Matrix; mono/stereo; LOW DI 0/50/100%; bass-note and crossover phase checks |
| Realtime cost | Zero callback allocation/blocking in the watched paths; repeated CPU p99/deadline-miss results on stated hardware, including drag/swap load |
| Lifecycle | UI open/close, instance removal, project/DAW close, repeated prepare/release and worker teardown |
| Product sound | Same-DI guitar and bass audition with raw and level-matched results; selected Niflheimr and Náströnd presets retain distinct roles |
| Delivery | Exact-source candidate, installed factory inventory, user IR/preset preservation and EN/DE/KR documentation |

Cover at least 44.1/48/96 kHz, practical host buffer sizes and the supported
amp oversampling modes. A test's numeric acceptance threshold must be defined
before results are labelled PASS; no guessed global CPU percentage. Physical
speaker distortion/compression is outside the first linear-IR CAB scope.

## References

Official hardware descriptions below were reviewed on 2026-10-08. Product
pages document hardware, not Chimera acquisition or capture fidelity.

- UI: https://neuraldsp.com/getting-started/tips-for-using-your-plugin
- Bass cab controls: https://neuraldsp.com/manual/darkglass-ultimate
- G1: https://legacy.mesaboogie.com/cabinets--simulators/guitar-cabinets--simulators/rectifier-series/4x12-recto-standard-oversized-straight.html
- G2: https://www.marshall.com/us/en/product/1960a-4x12-angled-cabinet?color=black&pid=1007269
- G3: https://www.marshall.com/my/en/product/1960ax-4x12-angled-cabinet?redirected=1
- G4: https://orangeamps.com/en-es/products/ppc412
- G5: https://intl.fender.com/products/65-twin-reverb
- Open-back enclosure reference: https://www.fender.com/articles/parts-and-accessories/whats-the-difference-between-open-back-and-closed-back-speaker-enclosures
- G6: https://eminence.com/products/karnivore-by-kristian-kohle
- G7: https://celestion.com/product/g12-100-raven-2/
- B1: https://ampeg.com/products/classic/cabs.html
- B2: https://www.darkglass.com/products/dg410ne
- B3: https://glockenklang.de/double/
- B4: https://www.hartke.com/products/cabinets/hydrive-hd-cabinets/hd410/
- B5: https://rosette.mesaboogie.com/cabinets--simulators/bass-cabinets/powerhouse-series/standard-powerhouse/2x15.html
- M1: https://www.shure.com/en-US/products/microphones/sm57
- M2: https://www.sennheiser.com/en-us/catalog/products/mikrofon/md-421-ii/md-421-ii-000984
- M3: https://docs.cloud.sennheiser.com/en-us/evolution-wired/manual-e906-using.html
- M4: https://royerlabs.com/r-121/
- M5: https://europe.beyerdynamic.com/p/m-160
- M6: https://www.neumann.com/en-us/products/microphones/u-87-ai/
- M7: https://products.electrovoice.com/product.php/product.php?id=91
- M8: https://www.akg.com/D112MkII.html?dwvar_D112MkII_color=Black-GLOBAL-Current

Repository evidence: Source/Cabinet.h, Source/ChimeraDSP.h,
Source/PluginProcessor.cpp, Source/IRLibrary.cpp, docs/THIRD_PARTY_NOTICES.md,
docs/reference/ir-catalog.json, docs/reference/raven-ir-catalog.json and the
PR #24 IR browser/collection changes. See also
[IR/routing design](FX_AND_IR_DESIGN.md) and [catalog roadmap](CATALOG_ROADMAP.md).
