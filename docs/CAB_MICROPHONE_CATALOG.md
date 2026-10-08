# CAB microphone catalog

The approved initial CAB microphone roster is **20 identities: 9 moving-coil
dynamics, 3 ribbons and 8 condensers**. Nineteen entries identify external
microphone references; **Chimera Strike** is the one Chimera-original microphone
and belongs to the condenser group. Strike is a working name.

This catalog supplies grouped labels and filters for the
[fixed captured-IR CAB panel](CAB_PANEL_PROTOTYPE.md). It does not add 20 bundled
microphone responses or a calibrated microphone-modeling engine. The two embedded
factory IR files remain `Assets/IRs/guitar_v30_sm57.wav` and
`Assets/IRs/guitar_jensen_sm57.wav`. Additional playable choices depend on actual
available captures and their metadata.

## Canonical roster

Aliases are the primary names in the grouped selector. The external-reference
column identifies the corresponding microphone, without asserting that every
hardware revision, switch position or polar pattern has been captured or modeled.
Where the approved roster does not specify a revision, retain the general model
name and preserve any more specific information supplied with an individual IR.

### Dynamic — 9

| UI alias | External reference |
|---|---|
| Dynamic 57 | Shure SM57 |
| Dynamic 421 | Sennheiser MD421 |
| Dynamic 441 | Sennheiser MD441 |
| Dynamic 906 | Sennheiser e906 |
| Dynamic 20 | Electro-Voice RE20 |
| Dynamic 7 | Shure SM7B |
| Dynamic 201 | beyerdynamic M201 |
| Dynamic 88 | beyerdynamic M88 |
| Dynamic 112 | AKG D112 |

Dynamic 58 / Shure SM58, Dynamic 609 / Sennheiser e609 and Dynamic 52 / Shure Beta
52A are excluded from the planned core roster. This is a catalog-scope decision:
existing user imports, their original microphone metadata and their saved audio
remain valid. Do not delete these captures or relabel them as another core model.

### Ribbon — 3

| UI alias | External reference |
|---|---|
| Ribbon 121 | Royer R-121 |
| Ribbon 160 | beyerdynamic M160 |
| Ribbon 4038 | Coles 4038 |

Three ribbons are sufficient for the initial catalog scope. Their intended
coverage is a cabinet/body and dynamic-blend baseline, a focused rhythm
alternative, and a smooth classic ribbon texture, respectively. These are
selection and development intentions; differences between actual captures still
depend on the complete recording chain and require listening validation. A fourth
ribbon needs a distinct, demonstrated use case rather than a target count.

### Condenser — 8

| UI alias | External reference or origin |
|---|---|
| Condenser 87 | Neumann U87 |
| Condenser 414 | AKG C414 |
| Condenser 184 | Neumann KM184 |
| Condenser 47 FET | Neumann U47 fet |
| Condenser 4050 | Audio-Technica AT4050 |
| Condenser 201 FET | Mojave MA-201fet |
| Condenser 67 | Neumann U67 |
| Chimera Strike | Chimera original; no external reference |

Condenser 4050, Condenser 201 FET and Condenser 67 replace the three removed dynamic
slots in the proposed core. Adding the original condenser produces the agreed
9 / 3 / 8 balance. The roster does not assign an unspecified U87, C414, MD421 or
D112 revision to a more specific variant.

## Capture selection and compatibility

The microphone selector groups the aliases under Dynamic, Ribbon and Condenser.
After selection, show the original reference in smaller text without a
`REFERENCE:` prefix. For Strike, show **Chimera original** instead of assigning an
external manufacturer or model.

A microphone identity filters or labels available captures. Changing a filter
does not transform the loaded IR or change the audio. Only successfully selecting
a valid captured-IR asset changes the capture used by that slot. An identity with
no matching capture does not become a playable modeled microphone merely because
it exists in this catalog. Retain full capture filenames and their supplied
position, voicing and prepared-mix information; do not infer physical microphone
position or distance from a catalog identity.

Keep an unfiltered route to imported captures, including unknown microphones,
multi-microphone mixes and models outside the core roster. Catalog recognition is
presentation metadata and must not replace the original provenance or overwrite
user-entered microphone descriptions. Removing a core identity does not remove a
loaded IR or an entry from the user's library.

Catalog string IDs are presentation identities, not host parameter ordinals. The
existing `cabtype` and `cabBtype` values retain their meanings:

| Serialized value | Source |
|---:|---|
| 0 | Filters only |
| 1 | V30 / SM57 factory IR |
| 2 | Jensen / SM57 factory IR |
| 3 | User IR |

Do not append microphone identities to these source enumerations or reindex their
existing values. State schema **11**, asset-hash references, legacy import support
and Mic A/B restoration remain unchanged by this catalog work. Matching an
external reference does not authorize bundling the corresponding private capture.

## Chimera Strike development target

Strike is an **original condenser concept for high-gain guitar and bass
cabinets**. Its intended response supports low-tuned note and picking separation,
restrained excess low-mid buildup and controlled high-gain fizz. It should be
usable as a single microphone and in a two-microphone blend.

These are unverified design targets. This catalog does not supply a measured
Strike response, a validated DSP model or listening evidence that those targets
have been achieved. Strike has no one-to-one external microphone reference and
must not be mapped to Audix i5 or reclassified as a dynamic or ribbon. Playable
original response work and its validation are separate from catalog/UI
integration.

## Related speaker work

Chimera Fang 12 and Chimera Abyss are separate original speaker-model work. The
12-inch Abyss proposal remains provisional; this microphone change does not lock
its diameter, enclosure or response. Existing Karnivore and Raven speaker
references must be retained as external references: Eminence Karnivore by
Kristian Kohle and Celestion G12-100 Raven. Neither becomes a Chimera-original
speaker through this catalog update.

## Primary references

These manufacturer pages support microphone identity and product background.
They are not evidence that the corresponding response has been implemented or
calibrated in Chimera.

| Catalog entry | Manufacturer reference |
|---|---|
| Condenser 4050 | [Audio-Technica AT4050](https://www.audio-technica.com/en-us/at4050) |
| Condenser 201 FET | [Mojave MA-201fet](https://www.mojaveaudio.com/products/ma-201fet) |
| Condenser 67 | [Neumann U67](https://newsroom.neumann.com/u-67-return-of-a-legend) |
| Ribbon 121 | [Royer R-121](https://royerlabs.com/r-121/) |
| Ribbon 160 | [beyerdynamic M160](https://global.beyerdynamic.com/p/m-160) |
| Ribbon 4038 | [Coles 4038](https://coleselectroacoustics.com/4038-studio-ribbon-microphone/) |
