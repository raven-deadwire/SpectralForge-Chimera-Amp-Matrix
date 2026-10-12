# Classic room, CAB/IR controls and Niflheimr presets — 12 October 2026

## Reported problems

The user reported that Classic did not expose the cabinet room, selecting a
modeled cabinet made the IR Loader filters appear active, and Niflheimr's five
presets all used Classic with names that did not clearly describe their use.
This change is based on main `3817246f779a50f19b5378ec9661a2275e6b91b2`.

## Room navigation

Classic, Dual and Matrix open the RIGS page on their one-, two- or three-rig
room. Every mode offers RIG CONTROLS / CABINET ROOM. Clicking a cabinet opens
its editor; ROOM returns to the overview. An explicit request to focus a
cabinet still opens that cabinet directly.

Changing mode returns an open cabinet workspace to its room. If the focused
rig no longer exists, its panel and owned IR library close after any active
microphone drag finishes. Navigation does not select a new audio source.

## CABINET and IR LOADER

Each microphone uses either the modeled response or its selected captured
IR/filter-only source. `IRLibrary::build()` already selects those paths
exclusively; the reported presentation is not evidence of two cabinet
convolutions being cascaded. The existing transition crossfade is unchanged.

The two views share that microphone's gain, delay, polarity and cuts. The old
IR Loader showed those controls as editable even while that microphone used
CABINET, and labeled its retained source as `Filters only` without indicating
that the source was inactive.

While a microphone uses CABINET, the IR view now labels its retained source
`Stored / ...`, shows `IR inactive`, and disables those shared controls in
that view. CABINET retains the working controls. When both microphones use
CABINET, the IR view also disables the shared A/B blend. A deliberate captured
IR or Filters only selection switches only that microphone; merely browsing
a cabinet group or changing tabs preserves the complete state. Mixed modeled
and captured A/B configurations remain supported.

The separate main-editor IR library now also clears Mic A's modeled switch
when a factory IR is deliberately selected. A retained personal IR can be
reselected even when its filename is unchanged.

The library initializes its filter defaults without queuing change callbacks.
Its final refresh already applies those defaults; the redundant deferred
callbacks could otherwise clear a valid first selection and disable LOAD.
The immediate-selection regression retains that original failing sequence.

No DSP filter is removed or reset. No parameter ID, ordinal, default, source
enumeration or stored IR asset format changes.

## Native room painting

The native macOS regression also exposed long event-processing slices before
a queued room click reached its listener. One failing Matrix transition spent
about 1.40 and 1.43 seconds in consecutive CF run-loop calls; other observed
slices spent most of their time in AppKit polling. The full CAB state test and
IR isolation test passed in that same run, while the independent room test
retained its original failure and two-second readiness limit.

The cabinet grille uses small repeating background and foreground textures.
JUCE's scaled CoreGraphics tiling path draws each repetition separately. To
reduce this room-paint workload, each shared skin prepares larger textures
from exact whole repetitions once during construction. The original inspected
crops, physical scale, origin, foreground alpha and resampling settings remain
available; manually constructed skins retain their original-tile fallback.
Each repeat contains four columns and three rows of the original crop. The
two unused oblique speaker identity images retain 32-pixel thumbnails; visible
speakers still use the unchanged 192-pixel front images. Counting all repeated
textures gives 16,759,936 retained RGBA bytes, below the unchanged 16 MiB limit
and 8,448 bytes less than before.

Cache construction stays in skin initialization; native image copying during
a fill remains a separate renderer cost. No viewport-sized cache or audio
parameter is introduced. Pixel ownership, repetition and native/software
material contracts cover the change; current native timing and platform
results are recorded in PR #54. The dispatcher and click deadlines are unchanged.

## Niflheimr bank

| Stored ordinal | Previous name | New name | Routing and purpose |
| --- | --- | --- | --- |
| 43 | Frostline Precision | Clear Clank | Classic Hrímfaxi; controlled lows and clear string attack |
| 44 | Carrion Barrage | Death Grind | Dual at 180 Hz; restrained Hrímfaxi lows and Garmr upper grind |
| 45 | Foundry Pulse | Clean Low Pulse | Matrix at 140 / 1600 Hz; clean LOW DI, Nidavellir MID drive, Hrímfaxi HIGH attack |
| 46 | Jötunn Hammer | Slam Punch | Matrix at 110 / 900 Hz; LOW DI 75% / Ymir AMP+CAB 25%, Ymir MID, Garmr HIGH |
| 47 | Mirebound Monolith | Doom Fuzz | Classic Hel; sustained sludge/doom fuzz |

The 12 October naming follow-up replaced the draft names `Progressive Clank`
and `Industrial Pulse` with `Clear Attack` and `Clean Low Drive`. On 13 October
(KST), the user clarified that **Clank** and **Pulse** must remain in the names.
The current names are **Clear Clank** and **Clean Low Pulse**, with matching
catalog/manual labels and default factory export filenames. Their descriptions
still explain string attack and clean LOW DI. The authored audio parameters,
routing modes and stable IDs are retained; existing saved references keep their
stored names and sounds.

Each active lane has a mode-specific native context, channel controls, bass
cabinet recipe and visible gain trims. Global OUTPUT stays at 0 dB. The bank
still has five entries at 43–47 with the same serialized IDs. Existing saved
projects and `.chimera` snapshots retain their stored route, values and name;
selecting a factory preset recalls the new recipe. The current EN/DE/KR manual
and catalog/audition labels use the new names.

## Verification

The focused native tests are `ChimeraCabPanelRoutingTests` and
`ChimeraCabPanelIRIsolationTests`. Their executable accepts
`--classic-room-regression`, `--ir-isolation-regression`, and the combined
`--room-ir-regressions`. Separate CTest entries ensure a failure in one path
does not prevent the other from being exercised. Coverage includes native window navigation,
all three modes, state and host-notification invariance, an inactive rig's IR
window lifetime, retained filter/factory/personal sources, independent A/B
controls, intentional source selection, same-IR reselection, audio invariance
around navigation and project recall.

Niflheimr production tests cover dirty-state recall, the correct native bank
for every lane, actual audio contribution from each split band, silent parked
LOW hardware in Clean Low Pulse, ordinary and +6 dB input headroom, binary
and A/B restore, and retaining an existing Classic snapshot. The pre-existing
RMS > −30 dBFS and peak < 0.95 limits are unchanged.

The legacy rig-level check compares the exact canonical normalized zero
detent, including the host parameter value. JUCE's 0.01 dB snapping can leave
a sub-microdecibel raw residual with fused floating-point arithmetic; testing
raw equality with `0.f` is not a portable zero-detent check. Adjacent
0.01 dB settings and actual legacy output corrections remain distinguishable.
Evidence records both raw and normalized values; no audio limit is widened.

The dedicated `ChimeraNiflheimrPresetTests` CTest runs the stock modeled-CAB
presets through the production processor. The Niflheimr DI workflow builds
and runs it on Windows, Linux and macOS alongside its existing head renderer
and common-IR comparison contracts. The common-IR variant replaces only
enabled cabinets and preserves the same routing, native channels and LOW DI
roles; its manifest records these choices explicitly.

### Local production render, 12 October 2026

The measurements below retain the draft labels recorded at source
`97d6786ba35fa541f2ff50b621222aac4f007a5b`: `Progressive Clank` is now
`Clear Clank`, and `Industrial Pulse` is now `Clean Low Pulse`. These are
historical render results; the display-name follow-up does not claim a new
render or real-DI listening result.

The Release/GCC 13.3 build passed both the five-preset production-recall
measurement and the strict Niflheimr route/state/audio tests. These values
use the unchanged deterministic six-pluck bass fixture (48 kHz, 128-sample
blocks, 1.2 seconds, stereo). Every preset recalls global OUTPUT at 0 dB.

| Preset | RMS dBFS | Peak dBFS | Peak with +6 dB input, dBFS |
| --- | ---: | ---: | ---: |
| Progressive Clank | -24.2670 | -9.64738 | -6.38747 |
| Death Grind | -25.0032 | -11.7928 | -9.52069 |
| Industrial Pulse | -23.3847 | -9.32282 | -5.42369 |
| Slam Punch | -27.1406 | -11.0145 | -5.85054 |
| Doom Fuzz | -24.7331 | -13.1380 | -7.24318 |

All five pass the existing RMS and peak limits. Binary and A/B restore have
zero measured sample error; every authored lane contributes, and Industrial
Pulse's parked LOW amp/cabinet remains silent. No further gain adjustment was
made from this fixture. Its limited upper-band content does not establish
musical balance or justify equalizing these different voicings.

The catalog contract and four audition coordinator tests also pass. Local
offscreen CAB visual contracts pass with the repeated textures, including
material phase, rail continuity, HF transmission and the original memory
budget (16,759,936 total RGBA bytes; 114,432 bytes in repeat textures). Local
native window execution is blocked by this environment's Xvfb socket
restriction; actual interaction results, screenshots, and current-source
platform status are recorded in [PR #54](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/54).
The new routing recipes remain authored starting points; synthetic tests do
not establish real-DI listening approval.
