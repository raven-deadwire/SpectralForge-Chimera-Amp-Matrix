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

No DSP filter is removed or reset. No parameter ID, ordinal, default, source
enumeration or stored IR asset format changes.

## Niflheimr bank

| Stored ordinal | Previous name | New name | Routing and purpose |
| --- | --- | --- | --- |
| 43 | Frostline Precision | Progressive Clank | Classic Hrímfaxi; precise progressive-metal picking |
| 44 | Carrion Barrage | Death Grind | Dual at 180 Hz; restrained Hrímfaxi lows and Garmr upper grind |
| 45 | Foundry Pulse | Industrial Pulse | Matrix at 140 / 1600 Hz; clean LOW DI, Nidavellir MID, Hrímfaxi HIGH |
| 46 | Jötunn Hammer | Slam Punch | Matrix at 110 / 900 Hz; LOW DI 75% / Ymir AMP+CAB 25%, Ymir MID, Garmr HIGH |
| 47 | Mirebound Monolith | Doom Fuzz | Classic Hel; sustained sludge/doom fuzz |

Each active lane has a mode-specific native context, channel controls, bass
cabinet recipe and visible gain trims. Global OUTPUT stays at 0 dB. The bank
still has five entries at 43–47 with the same serialized IDs. Existing saved
projects and `.chimera` snapshots retain their stored route, values and name;
selecting a factory preset recalls the new recipe. The current EN/DE/KR manual
and catalog/audition labels use the new names.

## Verification

The focused native test is `ChimeraCabPanelRoutingTests`. Its executable also
accepts `--classic-room-regression` and `--ir-isolation-regression` to isolate
the two reported UI paths. Coverage includes real native window navigation,
all three modes, state and host-notification invariance, an inactive rig's IR
window lifetime, retained filter/factory/personal sources, independent A/B
controls, intentional source selection, same-IR reselection, audio invariance
around navigation and project recall.

Niflheimr production tests cover dirty-state recall, the correct native bank
for every lane, actual audio contribution from each split band, silent parked
LOW hardware in Industrial Pulse, ordinary and +6 dB input headroom, binary
and A/B restore, and retaining an existing Classic snapshot. The pre-existing
RMS > −30 dBFS and peak < 0.95 limits are unchanged.

Initial local checks: catalog contract and four audition coordinator tests
pass. GCC syntax checks cover the edited production and test translation
units. Native build, interaction, audio and platform results are recorded in
the associated pull request as they complete. The new routing recipes are
authored starting points; synthetic tests do not establish listening approval.
