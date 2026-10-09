# CAB microphone presentation dimensions

Verified against manufacturer publications on 2026-10-09. These values correct
the relative size and upright silhouette of the CAB artwork. They do not alter
audio, identify the revision used to capture an imported IR, or claim that
Chimera's original DSP responses replicate the reference hardware.

`Source/CabMicrophoneDimensions.h` stores metres. The table below uses millimetres.
`width` is the visible front-body width; `height` is its length with the microphone
drawn upright, before scene rotation. It is not necessarily the dimension named
“height” in a manufacturer's horizontally oriented drawing. Bodies use the same
pixels-per-metre projection as the speaker units. PNG padding and the source
image's proportions are not dimensional evidence.

| Catalog ID | Presentation reference / revision | Width × height (mm) | Measurement boundary and primary source |
| --- | --- | ---: | --- |
| `dynamic-57` | Shure SM57 | 32 × 157 | Maximum grille diameter × overall body length; excludes clip/cable. [Shure user guide, dimensions](https://pubs.shure.com/view/guide/SM57/en-US.pdf). |
| `dynamic-421` | Sennheiser MD 421-II | 49 × 215 | 215 × 46 × 49 overall specification; uses wider 49 mm front body dimension, excluding the removable stand clip. Uses the named full-size II, not MD 421 Kompakt. [Sennheiser MD 421-II](https://www.sennheiser.com/en-sg/catalog/products/microphones/md-421-ii/md-421-ii-000984). |
| `dynamic-441` | Sennheiser MD 441-U | 36 × 270 | 270 × 33 × 36 overall specification; 36 mm front width, 33 mm depth. No removable MZQ 441 clip. [Sennheiser quick guide, specifications](https://www.sennheiser.com/globalassets/digizuite/41239-en-quick_guide_md_441-u_11_2022.pdf). |
| `dynamic-906` | Sennheiser e 906 | 55 × 134 | Front width × vertical body/XLR length; 34 mm thickness is excluded from the front projection. [Sennheiser specifications](https://docs.cloud.sennheiser.com/en-us/evolution-wired/evolution-wired/specifications-e906.html), [dimension drawing](https://www.sennheiser.com/globalassets/digizuite/41690-en-e_906_product_specification_v1.1_en.pdf). |
| `dynamic-20` | Electro-Voice RE20 | 54.4 × 216.7 | Maximum grille diameter, not narrower 49.2 mm shaft; excludes external suspension. [Electro-Voice engineering data, dimension drawing](https://products.electrovoice.com/binary/RE20_Engineering_Data_Sheet.PDF). |
| `dynamic-7` | Shure SM7B, standard windscreen | 62.5 × 197.1 | Uses cylindrical body/standard-foam diameter and axial length from guide v2.6, p11. The 97.4 mm mounting width and 149.2 mm stand envelope are excluded: the Chimera bitmap has no yoke. Not the larger A7WS foam. [Shure user guide](https://pubs.shure.com/view/guide/SM7B/en-US.pdf). |
| `dynamic-201` | beyerdynamic M 201, current M Series | 24 × 147 | Body diameter × length; no external clip/windshield. [beyerdynamic M Series specifications, p6](https://global.beyerdynamic.com/amfile/file/download/file/1717). |
| `dynamic-88` | beyerdynamic M 88, current M Series | 48.5 × 173 | Head diameter × length. Current M Series dimensions are used consistently; older M 88 TG documentation lists a different length. [beyerdynamic M Series specifications, p4](https://global.beyerdynamic.com/amfile/file/download/file/1717). |
| `dynamic-112` | AKG D112 MKII | 70 × 126 | Front body diameter × upright height, including the integral swivel shown in the bitmap. The separate 115 mm length is front-to-back depth and must not become front width. [AKG cutsheet, p2](https://be.akg.com/on/demandware.static/-/Sites-masterCatalog_Harman/default/dwef0475a5/pdfs/AKG_d112_mkII_cutsheet.pdf). |
| `ribbon-121` | Royer R-121 | 25 × 155.7 | Body width × length; excludes separate shock mount. Uses product-page millimetres; the cutsheet rounds length to 156 mm. [Royer R-121 technical specifications](https://royerlabs.com/r-121/). |
| `ribbon-160` | beyerdynamic M 160, current M Series | 38 × 164 | Head diameter × length. Uses current M Series geometry; older 156 mm literature describes an earlier revision. [beyerdynamic M Series specifications, p10](https://global.beyerdynamic.com/amfile/file/download/file/1717). |
| `ribbon-4038` | Coles 4038 | 83 × 197 | Manufacturer's complete 197 × 83 × 61 mm envelope, including lower hinge/connector assembly; 61 mm depth excluded. The PDF drawing was visually checked because extracted imperial fractions can be misread. [Coles specification](https://coleselectroacoustics.com/wp-content/uploads/2021/12/4038Spec.pdf). |
| `condenser-87` | Neumann U 87 Ai | 56 × 200 | Body maximum diameter × length; no EA 87/Z 48 suspension. [Neumann data and diagrams](https://www.neumann.com/en-de/products/microphones/u-87-ai). |
| `condenser-414` | AKG C414 XLS / XLII | 50 × 160 | Front width × body height; 38 mm depth and H85 suspension excluded. Family variants retain their catalog metadata; this is the stated artwork sizing reference. [AKG C414 XLS specifications](https://www.akg.com/C414XLS.html), [XLS/XLII manual, p46](https://uk.akg.com/on/demandware.static/-/Sites-masterCatalog_Harman/default/dwaabcd3f2/pdfs/AKG_C414XLS_C414XLII_Manual.pdf). |
| `condenser-184` | Neumann KM 184, Series 180 | 22 × 107 | Body diameter × length; no clip or foam. [Neumann data and diagrams](https://www.neumann.com/en-us/products/microphones/km-184-series-180/). |
| `condenser-47-fet` | Neumann U 47 fet i reissue | 63 × 160 | Body diameter × length. Separate side swivel arm is excluded because it is absent from the front artwork. [Neumann data and diagrams](https://www.neumann.com/en-us/products/microphones/u-47-fet-i/). |
| `condenser-4050` | Audio-Technica AT4050 | 53.4 × 188 | Maximum body diameter × length; no AT8449 suspension. [Audio-Technica specifications](https://www.audio-technica.co.jp/product/AT4050/). |
| `condenser-201-fet` | Mojave MA-201fet | 51 × 194 | Published metric microphone dimensions, excluding the carrying case and shock mount. [Mojave cutsheet](https://mojaveaudio.com/pdf/MA-201fetCutsheet.pdf). |
| `condenser-67` | Neumann U 67 current reissue | 56 × 200 | Body diameter × length; no Z 48 suspension. [Neumann data and diagrams](https://www.neumann.com/en-gb/products/microphones/u-67-set). |
| `chimera-strike` | Chimera original design | 50 × 200 | Authored enclosure dimensions, not an external hardware measurement. |

## Authored and unknown identities

The original legacy roles retain baked viewing angles in their bitmaps, unlike
the upright catalog images. Their authored **projected image envelopes** are
Attack Dynamic 160 × 135 mm (diagonal end-address view), Body Ribbon 55 × 200 mm,
and Detail Condenser 90 × 200 mm. These are not upright body diameters.
All three and Chimera Strike return `authored=true`. They are not silently
substituted with dimensions attributed to an external microphone.

Catalog indices are presentation lookups only and follow `micCatalog::models`.
An invalid index falls back to the authored Attack Dynamic size, with
`authored=true`; it must not establish the identity of an unknown IR capture.
Unknown and mixed IRs must retain their unknown/mixed labels and generic images.

## Scope of fidelity

This is a dimensioned 2D body-envelope correction. It does not turn a bitmap into
a calibrated 3D model or add perspective information absent from the asset.
Changing camera zoom can scale all equipment together. Changing a speaker's
diameter must not independently resize the microphone body. An asset's integral
mount should only be included when the selected dimension boundary includes it;
adding a yoke or shock mount later requires a separate body-versus-mount envelope.
