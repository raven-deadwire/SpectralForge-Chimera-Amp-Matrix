# CAB amplifier display dimensions

Checked 2026-10-09. `Source/CabHeadDimensions.h` is a presentation-only catalogue,
in the exact released `AmpModel` order. It changes neither DSP nor saved state.
All values below are **width × height × depth in millimetres**. The C++ API uses
metres. The same pixels-per-metre factor must be used for a head, cabinet,
speaker and microphone in a given scene; fitting every head to a common picture
rectangle would remove the intended physical differences again.

`authored = false` means a complete manufacturer-published enclosure envelope
is available for the stated format. It does not claim that the SpectralForge
voicing or artwork is a physically measured reproduction. `authored = true`
means one or more dimensions are a chosen display envelope, including head
conversions of combos, unverified legacy formats, and original SpectralForge
designs. These rows must not be presented as manufacturer measurements.

## Catalogue and primary provenance

| Index | Chimera model | Display W × H × D, mm | Authored | Source and format decision |
|---:|---|---:|:---:|---|
| 0 | Glass | 673.10 × 225 × 263.53 | Yes | [Fender '65 Twin Reverb](https://www.fender.com/products/65-twin-reverb/): width 26.5 in and depth 10.375 in. The actual reference is a combo, not a separate head. Only those two chassis-envelope dimensions are reused; the 225 mm head height is authored. The combo's 504.6 mm height is deliberately not represented as a head. |
| 1 | Brit Edge | 665 × 265 × 205 | No | [Marshall JTM45 2245](https://www.marshall.com/gg/en/product/jtm45-2245-vintage-reissue-head?pid=1007103), physical-unit dimensions. |
| 2 | Tight 515 | 676.275 × 254 × 298.45 | No | [Peavey 6505 II](https://peavey.com/collections/guitar-amplifiers/products/6505-ii-guitar-amp-head): manual-derived product dimensions 26.625 × 10 × 11.75 in. This is the selected full-size 6505-family enclosure; packaged shipping dimensions are not used. |
| 3 | Wide Rect | 647.70 × 254 × 250.825 | No | [Mesa Dual Rectifier head](https://rosette.mesaboogie.com/amplifiers/electric/rectifier-series/dual-rectifier/head.html): 25.5 × 10 × 9.875 in. |
| 4 | Liquid Lead | 476.25 × 240 × 270 | Yes | [Mesa Mark IV archive](https://production.mesaboogie.com/support/out-of-production/mark-iv.html) confirms the short-head width of 18.75 in. Height and depth are conservative authored dimensions because the retrieved primary page does not specify them. The separate 22.5 in wide-body format is not used. |
| 5 | Iron Tube | 610 × 292 × 324 | No | [Ampeg SVT-CL / SVT-VR / V-4B quick-start guide](https://ampeg.jp/products/pdf/SVT-CL_VR_4B_QSGuide_Rev._A.pdf), SVT-VR technical-specification column. |
| 6 | Solid Punch | 440 × 135 × 240 | Yes | Authored compact metal enclosure. The current [Gallien-Krueger manuals catalogue](https://www.gallien-krueger.com/manuals) did not provide a retrievable 800RB dimension record; conflicting third-party widths are not promoted to measured data. |
| 7 | Modern Bass | 431.80 × 133.35 × 355.60 | No | [Aguilar DB751](https://aguilaramp.com/en-eu/products/db751): 17 × 5.25 × 14 in, without detachable rack ears. This visual represents the power-amplifier component of the B7K Ultra + DB751 reference chain, not the pedal or an invented combined chassis. |
| 8 | Chime 30 | 705 × 284 × 266 | No | [VOX AC30CH](https://voxamps.com/en-gb/product/ac30-custom-head/); [manufacturer manual](https://voxamps.com/wp-content/uploads/support/AC_C1_C2_V212C_OM_EFGSJ10e.pdf). The official separate-head format is used for the AC30-derived voice. |
| 9 | Orange Crown | 550 × 270 × 280 | No | [Orange Rockerverb 50 MKIII](https://orangeamps.com/en-us/products/rockerverb-50-mkiii), unboxed dimensions. |
| 10 | Vintage Valve | 622.30 × 254 × 342.90 | No | [Fender Super Bassman](https://www.fender.com/products/super-bassman): 24.5 × 10 × 13.5 in. Inches are converted directly, avoiding the page's rounded centimetres. |
| 11 | Metro Clean | 336.55 × 66.675 × 257.81 | No | [Mesa Subway D-800+](https://subway.mesaboogie.com/amplifiers/bass/subway-series/subway-d800-plus/head.html): 13.25 × 2.625 × 10.15 in. This is the compact D-800+ format, not a large wooden head shell. |
| 12 | Prism Chime | 546.10 × 273.05 × 266.70 | No | [Matchless C-30 catalogue](https://www.matchlessamplifiers.com/amplifiers-and-cabinets/c-30): HC-30 separate head, 21.5 × 10.75 × 10.5 in. The DC-30 combo reference uses this official head-family format on the CAB scene. |
| 13 | Silk Lead | 580 × 260 × 270 | Yes | Authored medium wooden head. No verified primary dimensional drawing for a particular Dumble Overdrive Special chassis was available; the many custom-built versions are not treated as a single measured product. |
| 14 | Taste Punch | 270 × 45 × 210 | No | [EICH T900](https://www.eich-amps.com/t900), metric dimensions. This is the low-profile T900, not the taller T900 Classic. The manufacturer's rounded inch height differs from its metric value; the explicitly labelled metric dimensions are authoritative here. |
| 15 | Cinder 120 | 483 × 133 × 220 | No | [ZUTA GBG120](https://zutagroup.com/products/gbg120-tube-amp-by-zuta), specification section W × D × H = 483 × 220 × 133 mm. Width includes the 19-inch front/rack ears. The specifications' 220 mm depth is used instead of the introductory rounded 200 mm description. |
| 16 | Iron Compact | 340 × 170 × 220 | No | [ENGL Ironball E606](https://www.engl-usa.com/products/32548-ironball-e606); [older manufacturer E606 manual, mirrored copy](https://manuals.plus/m/6ed92a4f857e7da5cc37010fbbce8fc4c20132bfb74abcf9999e6f7a23549fd7_optim.pdf), technical data on page 7, lists height 14 (17) cm. The larger 170 mm value is selected as the complete display envelope with the carrying handle. The current product page and [2024 manual](https://www.engl-amps.com/wp-content/uploads/2024/03/E600_E606-OM-2-Ironball-Head-Combo.pdf) list only 140 mm; the older larger envelope is retained rather than adding a handle above the height budget. |
| 17 | Fourfold | 740 × 300 × 280 | No | [Diezel VH4](https://www.diezelamplification.com/vh4/): metric sequence 74 × 28 × 30 cm is resolved using the same line's labelled axes, 29 in length × 11 in wide × 12 in high. Thus display width is 740, height 300 and depth 280 mm; the unlabelled metric sequence is not assumed to be W × H × D. |
| 18 | Classic Tube | 610 × 292 × 330 | No | [Ampeg SVT-CL / SVT-VR / V-4B quick-start guide](https://ampeg.jp/products/pdf/SVT-CL_VR_4B_QSGuide_Rev._A.pdf), SVT-CL technical-specification column. |
| 19 | Monolith | 670 × 250 × 270 | Yes | Authored vintage full-size head envelope. Original SUNN Model T and later reissue formats are not interchangeable; a verified original-format primary dimensional drawing was not found. |
| 20 | Night Harvest | 730 × 280 × 255 | Yes | Conservative full-size display envelope. [Fortin's Evil Pumpkin product page](https://fortinamps.com/products/evil-pumpkin%C2%AE-3-channel-midi-100w-free-hydra-midi-pedal) confirms the model but the accessible specification text provides no dimensions; its linked manual could not be read. This row remains explicitly authored. |
| 21 | Hot Lead | 635 × 260.35 × 222.25 | No | [Soldano SLO-100 head](https://www.soldano.com/products/classic/amplifiers/slo-100-snakeskin/): 25 × 10.25 × 8.75 in. The current wooden head format is used, not the distinct four-space rackmount version. |
| 22 | Blue Storm | 690 × 280 × 270 | Yes | Authored full-size legacy-head envelope. Conflicting legacy Uberschall dimension sets and unspecified revisions prevent asserting an exact Rev Blue measurement. A newer Uberschall Mk2 chassis is not substituted. |
| 23 | Special Edition | 710 × 270 × 290 | No | [ENGL E670FE](https://www.engl-usa.com/products/32545-e670fe-special-edition-founders-edition); [manufacturer product page](https://www.engl-amps.com/shop/heads/engl-e670fe-special-edition-founders-edition/). |
| 24 | Náströnd | 700 × 270 × 285 | Yes | SpectralForge original display design: full-size high-gain guitar head. These are authored product proportions, not dimensions attributed to an external amplifier. |
| 25 | Niflheimr | 610 × 245 × 300 | Yes | SpectralForge original display design: broad bass head with a lower enclosure than the SVT format. These are authored product proportions. |

## Rendering interpretation

- Width and height describe a physical front/enclosure envelope. Transparent
  bitmap margins and the shape of the cropped source image never determine
  width, height, or camera zoom.
- Most manufacturers report external product height without separating feet,
  handles and shell. Keep those published envelopes intact; the renderer's
  fascia, feet and carrying handle are a visual decomposition within that
  envelope, not separately measured manufacturer parts. Iron Compact explicitly
  uses its manufacturer's 170 mm handle-inclusive envelope for consistency.
- Project the roof from the recorded depth at the common camera angle. Do not
  derive its depth from a fraction of front width or stretch a shallow compact
  head to the roof dimensions of a larger tube head.
- Small desktop heads must remain physically small on a large cabinet. No
  per-model minimum visual width is permitted. If a whole rig needs to fit a
  view, change the shared scene scale for all its equipment together.
- These values are suitable for relative equipment proportions, not fabrication,
  flight-case sizing, or a claim of a dimensionally surveyed hardware replica.
