#!/usr/bin/env python3
"""Procedural head shells for the 1.6 reference heads (models 26-30).

Unlike head-shell-00..25 (AI-created photographic source art), these are
drawn here from simple shapes: an original Chimera fascia per model, never a
copy of a manufacturer's logo or trade dress. Each image is one complete RGBA
enclosure with a blank control plate; live knobs and labels stay JUCE controls.

The script prints the metadata rows for Source/AmpHeadShellMetadata.h and the
reviewed plate fixtures for Tests/CabHeadAppearanceTests.h.
Run: python3 Tools/generate_reference_head_shells.py
"""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SHELLS = ROOT / 'Assets/Artwork/HeadShells'
ART = ROOT / 'Assets/Artwork'
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
W, H = 1672, 941

# body: x0, y0 (roof top), x1, y1 (body bottom); feet below y1.
MODELS = [
    dict(index=26, name='Crystal 120', surface='acrystal', body=(36, 170, 1636, 812), handle=True,
         tolex=(22, 22, 24), grille=(52, 52, 56), weave=(80, 80, 86), plate=(176, 180, 184), plateEdge=(108, 110, 114),
         badge=(225, 228, 232), badgeText=(30, 30, 34), piping=(150, 152, 156), split=.60),
    dict(index=27, name='Bluestack 100', surface='abluestack', body=(36, 150, 1636, 812), handle=True,
         tolex=(18, 17, 16), grille=(46, 40, 30), weave=(96, 84, 60), plate=(198, 164, 82), plateEdge=(120, 96, 40),
         badge=(236, 232, 220), badgeText=(20, 20, 20), piping=(228, 226, 218), split=.58),
    dict(index=28, name='Eightyfire', surface='aeightyfire', body=(36, 150, 1636, 812), handle=True,
         tolex=(16, 16, 16), grille=(34, 30, 26), weave=(70, 62, 50), plate=(206, 172, 90), plateEdge=(116, 92, 38),
         badge=(18, 18, 18), badgeText=(230, 226, 214), piping=(212, 210, 200), split=.58),
    dict(index=29, name='Fortress 103', surface='afortress', body=(36, 160, 1636, 812), handle=True,
         tolex=(20, 20, 22), grille=(40, 40, 42), weave=(64, 64, 68), plate=(206, 208, 206), plateEdge=(26, 26, 28),
         badge=(206, 208, 206), badgeText=(24, 24, 26), piping=(84, 84, 88), split=.56),
    dict(index=30, name='Northlight', surface='anorthlight', body=(236, 420, 1436, 782), handle=False,
         tolex=(30, 42, 66), grille=None, weave=None, plate=(40, 56, 86), plateEdge=(118, 150, 196),
         badge=(150, 190, 236), badgeText=(12, 20, 34), piping=(98, 130, 176), split=.30),
]


def shell(m):
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = m['body']
    roof = 34 if m['handle'] else 22
    # Feet first so the body overlaps their tops.
    foot_h = 30
    for fx in (x0 + 70, x1 - 170):
        d.rounded_rectangle((fx, y1 - 8, fx + 100, y1 + foot_h), 8, fill=(12, 12, 12, 255))
    if m['handle']:
        cx = (x0 + x1) // 2
        d.rounded_rectangle((cx - 210, y0 - 74, cx + 210, y0 - 48), 13, fill=(14, 14, 14, 255))
        for hx in (cx - 250, cx + 196):
            d.rounded_rectangle((hx, y0 - 66, hx + 54, y0 + 4), 8, fill=(150, 152, 156, 255), outline=(70, 70, 74, 255), width=3)
    # Complete enclosure: roof band (lighter) and front face.
    d.rounded_rectangle((x0, y0, x1, y1), 26, fill=m['tolex'] + (255,))
    top = tuple(min(255, c + 22) for c in m['tolex']) + (255,)
    d.rounded_rectangle((x0, y0, x1, y0 + roof), 26, fill=top)
    d.rectangle((x0, y0 + roof - 12, x1, y0 + roof), fill=top)
    front = (x0 + 18, y0 + roof + 10, x1 - 18, y1 - 18)
    # Tolex texture: deterministic fine grain.
    grain = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grain)
    for y in range(y0 + roof, y1, 4):
        shade = 10 if (y // 4) % 2 else 0
        gd.line((x0 + 4, y, x1 - 4, y), fill=(255, 255, 255, shade))
    img.alpha_composite(grain)
    fx0, fy0, fx1, fy1 = front
    split = int(fy0 + (fy1 - fy0) * m['split'])
    if m['grille']:
        g = (fx0 + 20, fy0 + 14, fx1 - 20, split - 16)
        d.rounded_rectangle(g, 10, fill=m['grille'] + (255,), outline=m['piping'] + (255,), width=5)
        for x in range(g[0] + 8, g[2] - 6, 14):
            d.line((x, g[1] + 8, x, g[3] - 8), fill=m['weave'] + (255,), width=3)
        for y in range(g[1] + 8, g[3] - 6, 14):
            d.line((g[0] + 8, y, g[2] - 8, y), fill=m['weave'] + (120,), width=2)
        badge = ((g[0] + g[2]) // 2 - 230, g[1] + 34, (g[0] + g[2]) // 2 + 230, g[1] + 112)
    else:
        badge = ((fx0 + fx1) // 2 - 230, fy0 + 16, (fx0 + fx1) // 2 + 230, fy0 + 80)
    d.rounded_rectangle(badge, 12, fill=m['badge'] + (255,), outline=m['piping'] + (255,), width=3)
    font = ImageFont.truetype(FONT, 46 if m['grille'] else 40)
    tw = d.textlength(m['name'].upper(), font=font)
    d.text(((badge[0] + badge[2] - tw) / 2, badge[1] + (badge[3] - badge[1] - 50) / 2), m['name'].upper(), font=font, fill=m['badgeText'] + (255,))
    plate = (fx0 + 26, split + 6, fx1 - 26, fy1 - 14)
    d.rounded_rectangle(plate, 8, fill=m['plate'] + (255,), outline=m['plateEdge'] + (255,), width=6)
    # Brushed finish inside the plate, kept subtle so JUCE labels stay legible.
    for y in range(plate[1] + 8, plate[3] - 6, 3):
        d.line((plate[0] + 8, y, plate[2] - 8, y), fill=tuple(min(255, c + (6 if y % 2 else -4)) for c in m['plate']) + (255,))
    # Power jewel and jack on the plate rim, outside the knob row.
    d.ellipse((plate[2] - 52, plate[1] + 16, plate[2] - 24, plate[1] + 44), fill=(200, 40, 30, 255) if m['grille'] else (90, 170, 255, 255))
    return img, front, plate


def alpha_bounds(img):
    a = img.getchannel('A').point(lambda v: 255 if v > 32 else 0)
    l, t, r, b = a.getbbox()
    return l, t, r - l, b - t


def main():
    provenance_path = SHELLS / 'provenance.json'
    provenance = json.loads(provenance_path.read_text())
    provenance['assets'] = [a for a in provenance['assets'] if a['modelIndex'] < 26]
    for m in MODELS:
        img, front, plate = shell(m)
        path = SHELLS / f"head-shell-{m['index']}.png"
        img.save(path, optimize=True)
        bounds = alpha_bounds(img)
        x0, y0, x1, y1 = m['body']
        support = y1 + 30
        fr = (front[0], front[1], front[2] - front[0], front[3] - front[1])
        pl = (plate[0], plate[1], plate[2] - plate[0], plate[3] - plate[1])
        print(f"META {m['index']}: Definition{{\"headshell{m['index']}_png\",{W},{H},{{{bounds[0]},{bounds[1]},{bounds[2]},{bounds[3]}}},"
              f"{{{fr[0]},{fr[1]},{fr[2]},{fr[3]}}},{support},{'true' if m['handle'] else 'false'}}}, // {m['index']}: {m['name']}")
        print(f"PLATE {m['index']}: {{{pl[0]},{pl[1]},{pl[2]},{pl[3]}}}, // {m['index']}")
        print(f"PLATE-FRACTION {m['index']}: x {(pl[0]-fr[0])/fr[2]:.4f}..{(pl[0]+pl[2]-fr[0])/fr[2]:.4f} y {(pl[1]-fr[1])/fr[3]:.4f}..{(pl[1]+pl[3]-fr[1])/fr[3]:.4f}")
        # Compact fascia for the selector/panel raster bank.
        fascia = img.crop((x0 - 4, m['body'][1] - (80 if m['handle'] else 6), x1 + 4, support + 2)).resize((960, 350 if m['handle'] else 290), Image.LANCZOS)
        fascia.save(ART / f"{m['surface']}.png", optimize=True)
        data = path.read_bytes()
        provenance['assets'].append({
            'modelIndex': m['index'], 'modelName': m['name'], 'catalogStatus': 'active',
            'asset': f"Assets/Artwork/HeadShells/head-shell-{m['index']}.png",
            'identityReference': f"Assets/Artwork/{m['surface']}.png",
            'sha256': hashlib.sha256(data).hexdigest(), 'byteLength': len(data),
            'pixelSize': [W, H], 'pixelMode': 'RGBA', 'alphaBounds': list(bounds),
            'runtimeMetadataIndex': m['index'],
            'generation': {'tool': 'Tools/generate_reference_head_shells.py',
                           'instructionRecord': {'kind': 'procedural_drawing',
                                                 'text': 'Original Chimera fascia drawn from rectangles, lines and the model alias; no manufacturer logo or trade dress.'}},
            'postGenerationProcessing': 'None. The script writes the production PNG directly.'})
    provenance['assetCount'] = 26 + len(MODELS)
    provenance['activeCatalogCount'] = 25 + len(MODELS)
    provenance_path.write_text(json.dumps(provenance, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
